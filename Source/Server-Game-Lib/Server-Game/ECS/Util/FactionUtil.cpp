#include "FactionUtil.h"

#include "Server-Game/ECS/Components/CharacterReputation.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/FactionModifiers.h"
#include "Server-Game/ECS/Components/UnitFaction.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <Base/Util/DebugHandler.h>

#include <entt/entt.hpp>

#include <algorithm>
#include <limits>

namespace ECS::Util::Faction
{
    using namespace Components;

    namespace
    {
        using namespace Gameplay::Faction;

        bool TryResolveReputationFaction(const FactionRuntimeData& runtime, FactionID factionID, FactionIndex& factionIndex)
        {
            if (!runtime.TryGetFactionIndex(factionID, factionIndex))
            {
                NC_LOG_ERROR("Unknown faction ID {0}", factionID);
                return false;
            }

            if (!HasFlag(runtime.GetDefinition(factionIndex).flags, DefinitionFlags::AllowsReputation))
            {
                NC_LOG_ERROR("Faction {0} does not allow reputation", factionID);
                return false;
            }

            return true;
        }

        ReputationSource DiscoveryProvenance(DiscoverySource source, FactionID factionID)
        {
            return {
                .type = source == DiscoverySource::Interaction
                            ? ReputationSourceType::DiscoveryInteraction
                            : ReputationSourceType::DiscoveryTarget,
                .contentID = factionID
            };
        }

        void MarkDirty(CharacterReputation& reputation, FactionIndex factionIndex, ReputationSource source)
        {
            const bool wasClean = reputation.dirtyByFaction.empty() && reputation.removedDirtyByFaction.empty();
            reputation.removedDirtyByFaction.erase(factionIndex);
            reputation.dirtyByFaction.insert_or_assign(factionIndex, source);
            if (wasClean)
            {
                reputation.nextPersistenceEpoch = 0;
                reputation.persistenceRetryCount = 0;
            }
        }

        void MarkRemoved(CharacterReputation& reputation, FactionIndex factionIndex, ReputationSource source)
        {
            const bool wasClean = reputation.dirtyByFaction.empty() && reputation.removedDirtyByFaction.empty();
            reputation.dirtyByFaction.erase(factionIndex);
            reputation.removedDirtyByFaction.insert_or_assign(factionIndex, source);
            if (wasClean)
            {
                reputation.nextPersistenceEpoch = 0;
                reputation.persistenceRetryCount = 0;
            }
        }

        void MarkNetworkUpdate(CharacterReputation& reputation, FactionIndex factionIndex, bool immediate)
        {
            const ReputationNetworkUpdateFlags flags = immediate ? ReputationNetworkUpdateFlags::Immediate : ReputationNetworkUpdateFlags::None;
            auto [itr, inserted] = reputation.pendingNetworkByFaction.try_emplace(factionIndex, flags);
            if (!inserted && immediate)
                itr->second = ReputationNetworkUpdateFlags::Immediate;
        }

        bool IsLocked(const ReputationState& state, ReputationSource source)
        {
            return (state.flags & static_cast<u16>(ReputationFlags::Locked)) != 0 && source.type != ReputationSourceType::Administrative;
        }

        Reaction MatrixOrPlayerReputation(const FactionRuntimeData& runtime, const UnitFaction& playerFaction, const CharacterReputation* playerReputation, const UnitFaction& otherFaction, const FactionModifiers* playerModifiers)
        {
            if (playerModifiers)
            {
                const auto perceptionItr = playerModifiers->perceptionByFaction.find(otherFaction.effectiveFaction);
                if (perceptionItr != playerModifiers->perceptionByFaction.end())
                {
                    const ResolvedFactionPerception& perception = perceptionItr->second;
                    if ((perception.activeFields & static_cast<u8>(PerceptionOverrideFields::Reaction)) != 0)
                        return perception.effectiveReaction;

                    if ((perception.activeFields & static_cast<u8>(PerceptionOverrideFields::Standing)) != 0)
                        return runtime.GetStanding(perception.effectiveStandingValue).reaction;
                }
            }

            if (playerReputation && HasFlag(runtime.GetDefinition(otherFaction.effectiveFaction).flags, DefinitionFlags::AllowsReputation))
            {
                if (const ReputationState* state = FindReputation(*playerReputation, otherFaction.effectiveFaction))
                    return runtime.GetStanding(state->value).reaction;
            }

            return runtime.GetRelation(playerFaction.effectiveFaction, otherFaction.effectiveFaction);
        }

        Reaction UnitToPlayer(const FactionRuntimeData& runtime, const UnitFaction& unitFaction, const UnitFaction& playerFaction, const CharacterReputation* playerReputation)
        {
            Reaction reaction = runtime.GetRelation(unitFaction.effectiveFaction, playerFaction.effectiveFaction);
            if (playerReputation && HasFlag(runtime.GetDefinition(unitFaction.effectiveFaction).flags, DefinitionFlags::AllowsReputation))
            {
                if (const ReputationState* state = FindReputation(*playerReputation, unitFaction.effectiveFaction))
                    reaction = runtime.GetStanding(state->value).reaction;
            }

            const ReactionBounds bounds = ReactionBounds::IsValidPacked(unitFaction.effectivePlayerReactionBounds)
                ? ReactionBounds::Unpack(unitFaction.effectivePlayerReactionBounds)
                : ReactionBounds::Unpack(NEUTRAL_REACTION_BOUNDS);

            return bounds.Clamp(reaction);
        }

        Reaction PresentationUnitToPlayer(const FactionRuntimeData& runtime, const UnitFaction& unitFaction, const UnitFaction& playerFaction, const CharacterReputation& playerReputation, const FactionModifiers* playerModifiers)
        {
            const ResolvedFactionPerception* perception = nullptr;
            if (playerModifiers)
            {
                const auto itr = playerModifiers->perceptionByFaction.find(unitFaction.effectiveFaction);
                if (itr != playerModifiers->perceptionByFaction.end())
                    perception = &itr->second;
            }

            Reaction reaction = runtime.GetRelation(unitFaction.effectiveFaction, playerFaction.effectiveFaction);
            if (perception && (perception->activeFields & static_cast<u8>(PerceptionOverrideFields::Standing)) != 0)
            {
                reaction = runtime.GetStanding(perception->effectiveStandingValue).reaction;
            }
            else if (HasFlag(runtime.GetDefinition(unitFaction.effectiveFaction).flags, DefinitionFlags::AllowsReputation))
            {
                if (const ReputationState* state = FindReputation(playerReputation, unitFaction.effectiveFaction))
                    reaction = runtime.GetStanding(state->value).reaction;
            }

            const ReactionBounds bounds = ReactionBounds::IsValidPacked(unitFaction.effectivePlayerReactionBounds)
                ? ReactionBounds::Unpack(unitFaction.effectivePlayerReactionBounds)
                : ReactionBounds::Unpack(NEUTRAL_REACTION_BOUNDS);

            reaction = bounds.Clamp(reaction);

            if (IsValidReaction(unitFaction.effectiveUnitReactionOverride))
                reaction = static_cast<Reaction>(unitFaction.effectiveUnitReactionOverride);

            if (perception && (perception->activeFields & static_cast<u8>(PerceptionOverrideFields::Reaction)) != 0)
                reaction = perception->effectiveReaction;

            return reaction;
        }

        bool HasAtWarFlag(const FactionRuntimeData& runtime, const CharacterReputation* attackerReputation, const UnitFaction& targetFaction)
        {
            if (!attackerReputation)
                return false;

            const DefinitionRuntime& definition = runtime.GetDefinition(targetFaction.effectiveFaction);
            if (!HasFlag(definition.flags, DefinitionFlags::AllowsReputation) || !HasFlag(definition.flags, DefinitionFlags::CanSetAtWar))
                return false;

            const ReputationState* state = FindReputation(*attackerReputation, targetFaction.effectiveFaction);
            return state && (state->flags & static_cast<u16>(ReputationFlags::AtWar)) != 0;
        }
    }

    bool TryGetFactionIndex(const FactionRuntimeData& runtime, FactionID factionID, FactionIndex& factionIndex)
    {
        return runtime.TryGetFactionIndex(factionID, factionIndex);
    }

    FactionID GetFactionID(const FactionRuntimeData& runtime, FactionIndex factionIndex)
    {
        return runtime.GetFactionID(factionIndex);
    }

    bool SetAssignedFaction(UnitFaction& unitFaction, const FactionRuntimeData& runtime, FactionID factionID)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!runtime.TryGetFactionIndex(factionID, factionIndex))
        {
            NC_LOG_ERROR("Cannot assign unknown faction ID {0}", factionID);
            return false;
        }

        unitFaction.assignedFaction = factionIndex;
        if ((unitFaction.flags & static_cast<u8>(UnitFactionFlags::HasEffectiveFactionOverride)) == 0)
            unitFaction.effectiveFaction = factionIndex;

        return true;
    }

    bool SetAssignedFaction(World& world, entt::entity entity, const FactionRuntimeData& runtime, FactionID factionID)
    {
        UnitFaction* unitFaction = world.TryGet<UnitFaction>(entity);
        if (!unitFaction)
        {
            NC_LOG_ERROR("Cannot assign faction ID {0} to entity {1}: UnitFaction is missing", factionID, entt::to_integral(entity));
            return false;
        }

        const FactionIndex oldEffectiveFaction = unitFaction->effectiveFaction;
        if (!SetAssignedFaction(*unitFaction, runtime, factionID))
            return false;

        if (unitFaction->effectiveFaction != oldEffectiveFaction)
            world.EmplaceOrReplace<Events::UnitNeedsFactionUpdate>(entity);

        return true;
    }

    bool SetAssignedPlayerReactionBounds(UnitFaction& unitFaction, Reaction minimum, Reaction maximum)
    {
        const ReactionBounds bounds{ .minimum = minimum, .maximum = maximum };
        if (!bounds.IsValid())
        {
            NC_LOG_ERROR("Cannot assign invalid player reaction bounds {0}..{1}", static_cast<u8>(minimum), static_cast<u8>(maximum));
            return false;
        }

        unitFaction.assignedPlayerReactionBounds = bounds.Pack();
        if ((unitFaction.flags & static_cast<u8>(UnitFactionFlags::HasPlayerReactionBoundsOverride)) == 0)
            unitFaction.effectivePlayerReactionBounds = bounds.Pack();

        return true;
    }

    bool SetAssignedPlayerReactionBounds(World& world, entt::entity entity, Reaction minimum, Reaction maximum)
    {
        UnitFaction* unitFaction = world.TryGet<UnitFaction>(entity);
        if (!unitFaction)
        {
            NC_LOG_ERROR("Cannot assign player reaction bounds to entity {0}: UnitFaction is missing", entt::to_integral(entity));
            return false;
        }

        const u8 oldEffectiveBounds = unitFaction->effectivePlayerReactionBounds;
        if (!SetAssignedPlayerReactionBounds(*unitFaction, minimum, maximum))
            return false;

        if (unitFaction->effectivePlayerReactionBounds != oldEffectiveBounds)
            world.EmplaceOrReplace<Events::UnitNeedsFactionUpdate>(entity);

        return true;
    }

    const ReputationState* FindReputation(const CharacterReputation& reputation, FactionIndex factionIndex)
    {
        const auto itr = reputation.byFaction.find(factionIndex);
        return itr == reputation.byFaction.end() ? nullptr : &itr->second;
    }

    ReputationState* FindReputation(CharacterReputation& reputation, FactionIndex factionIndex)
    {
        const auto itr = reputation.byFaction.find(factionIndex);
        return itr == reputation.byFaction.end() ? nullptr : &itr->second;
    }

    const ReputationState* FindReputation(const CharacterReputation& reputation, const FactionRuntimeData& runtime, FactionID factionID)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        return runtime.TryGetFactionIndex(factionID, factionIndex) ? FindReputation(reputation, factionIndex) : nullptr;
    }

    bool DiscoverReputation(const FactionRuntimeData& runtime, const UnitFaction& characterFaction, CharacterReputation& reputation, FactionID factionID, DiscoverySource source)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!runtime.TryGetFactionIndex(factionID, factionIndex) || factionIndex == NONE_FACTION_INDEX || !HasFlag(runtime.GetDefinition(factionIndex).flags, DefinitionFlags::AllowsReputation) || FindReputation(reputation, factionIndex))
            return false;

        const DefinitionRuntime& definition = runtime.GetDefinition(factionIndex);
        if (HasFlag(definition.flags, DefinitionFlags::HiddenUntilEarned))
            return false;

        const DefinitionFlags discoveryFlag = source == DiscoverySource::Interaction ? DefinitionFlags::DiscoverOnInteract : DefinitionFlags::DiscoverOnTarget;
        if (!HasFlag(definition.flags, discoveryFlag))
            return false;

        reputation.byFaction.emplace(factionIndex, ReputationState{
            .value = runtime.GetStartingReputation(characterFaction.assignedFaction, factionIndex),
            .flags = static_cast<u16>(ReputationFlags::Visible)
        });

        MarkDirty(reputation, factionIndex, DiscoveryProvenance(source, factionID));
        MarkNetworkUpdate(reputation, factionIndex, true);
        return true;
    }

    bool DiscoverReputation(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID, DiscoverySource source)
    {
        UnitFaction* characterFaction = world.TryGet<UnitFaction>(characterEntity);
        CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        if (!characterFaction || !reputation)
            return false;

        return DiscoverReputation(runtime, *characterFaction, *reputation, factionID, source);
    }

    i32 GetPersistentReputationValue(const FactionRuntimeData& runtime, const CharacterReputation& reputation, FactionID factionID)
    {
        const ReputationState* state = FindReputation(reputation, runtime, factionID);
        return state ? state->value : 0;
    }

    i32 GetPersistentReputationValue(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID)
    {
        const CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        return reputation ? GetPersistentReputationValue(runtime, *reputation, factionID) : 0;
    }

    const StandingThreshold& GetPersistentStanding(const FactionRuntimeData& runtime, const CharacterReputation& reputation, FactionID factionID)
    {
        return runtime.GetStanding(GetPersistentReputationValue(runtime, reputation, factionID));
    }

    const StandingThreshold& GetPersistentStanding(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID)
    {
        return runtime.GetStanding(GetPersistentReputationValue(world, characterEntity, runtime, factionID));
    }

    const StandingThreshold& GetEffectiveStanding(const FactionRuntimeData& runtime, const CharacterReputation& reputation, FactionID factionID)
    {
        return GetPersistentStanding(runtime, reputation, factionID);
    }

    const StandingThreshold& GetEffectiveStanding(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID)
    {
        FactionIndex faction = INVALID_FACTION_INDEX;
        const FactionModifiers* modifiers = world.TryGet<FactionModifiers>(characterEntity);
        if (modifiers && runtime.TryGetFactionIndex(factionID, faction))
        {
            const auto perceptionItr = modifiers->perceptionByFaction.find(faction);
            if (perceptionItr != modifiers->perceptionByFaction.end() && (perceptionItr->second.activeFields & static_cast<u8>(PerceptionOverrideFields::Standing)) != 0)
                return runtime.GetStanding(perceptionItr->second.effectiveStandingValue);
        }

        return GetPersistentStanding(world, characterEntity, runtime, factionID);
    }

    bool SetReputation(const FactionRuntimeData& runtime, const UnitFaction& characterFaction, CharacterReputation& reputation, FactionID factionID, i32 value, ReputationSource source)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!TryResolveReputationFaction(runtime, factionID, factionIndex))
            return false;

        ReputationState* state = FindReputation(reputation, factionIndex);
        if (state && IsLocked(*state, source))
            return false;

        if (!state)
        {
            const i32 startingValue = runtime.GetStartingReputation(characterFaction.assignedFaction, factionIndex);
            const bool hidden = HasFlag(runtime.GetDefinition(factionIndex).flags, DefinitionFlags::HiddenUntilEarned);
            if (hidden && value == startingValue)
                return false;

            state = &reputation.byFaction.emplace(factionIndex, ReputationState{
                .value = value,
                .flags = static_cast<u16>(ReputationFlags::Visible)
            }).first->second;

            MarkDirty(reputation, factionIndex, source);
            MarkNetworkUpdate(reputation, factionIndex, true);
            return true;
        }

        if (state->value == value)
            return false;

        const StandingID oldStandingID = runtime.GetStanding(state->value).id;
        const u16 oldFlags = state->flags;
        state->value = value;
        if (HasFlag(runtime.GetDefinition(factionIndex).flags, DefinitionFlags::HiddenUntilEarned))
            state->flags |= static_cast<u16>(ReputationFlags::Visible);

        MarkDirty(reputation, factionIndex, source);
        const bool standingChanged = runtime.GetStanding(value).id != oldStandingID;
        MarkNetworkUpdate(reputation, factionIndex, standingChanged || state->flags != oldFlags);
        return true;
    }

    bool SetReputation(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID, i32 value, ReputationSource source)
    {
        UnitFaction* characterFaction = world.TryGet<UnitFaction>(characterEntity);
        CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        return characterFaction && reputation && SetReputation(runtime, *characterFaction, *reputation, factionID, value, source);
    }

    bool ModifyReputation(const FactionRuntimeData& runtime, const UnitFaction& characterFaction, CharacterReputation& reputation, FactionID factionID, i32 delta, ReputationSource source)
    {
        if (delta == 0)
            return false;

        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!TryResolveReputationFaction(runtime, factionID, factionIndex))
            return false;

        const ReputationState* state = FindReputation(reputation, factionIndex);
        if (state && IsLocked(*state, source))
            return false;

        const i32 currentValue = state ? state->value : runtime.GetStartingReputation(characterFaction.assignedFaction, factionIndex);
        const i64 wideValue = static_cast<i64>(currentValue) + static_cast<i64>(delta);
        const i32 value = static_cast<i32>(std::clamp(wideValue, static_cast<i64>(std::numeric_limits<i32>::min()), static_cast<i64>(std::numeric_limits<i32>::max())));
        return SetReputation(runtime, characterFaction, reputation, factionID, value, source);
    }

    bool ModifyReputation(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID, i32 delta, ReputationSource source)
    {
        UnitFaction* characterFaction = world.TryGet<UnitFaction>(characterEntity);
        CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        return characterFaction && reputation && ModifyReputation(runtime, *characterFaction, *reputation, factionID, delta, source);
    }

    bool RemoveReputation(const FactionRuntimeData& runtime, CharacterReputation& reputation, FactionID factionID, ReputationSource source)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!TryResolveReputationFaction(runtime, factionID, factionIndex))
            return false;

        const ReputationState* state = FindReputation(reputation, factionIndex);
        if (!state || IsLocked(*state, source))
            return false;

        reputation.byFaction.erase(factionIndex);
        MarkRemoved(reputation, factionIndex, source);
        MarkNetworkUpdate(reputation, factionIndex, true);
        return true;
    }

    bool RemoveReputation(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID, ReputationSource source)
    {
        CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        return reputation && RemoveReputation(runtime, *reputation, factionID, source);
    }

    bool SetReputationFlags(const FactionRuntimeData& runtime, CharacterReputation& reputation, FactionID factionID, u16 flags, ReputationSource source)
    {
        FactionIndex factionIndex = INVALID_FACTION_INDEX;
        if (!TryResolveReputationFaction(runtime, factionID, factionIndex))
            return false;

        if ((flags & ~REPUTATION_FLAG_MASK) != 0)
        {
            NC_LOG_ERROR("Cannot set unknown reputation flags {0} for faction {1}", flags, factionID);
            return false;
        }

        const DefinitionRuntime& definition = runtime.GetDefinition(factionIndex);
        if ((flags & static_cast<u16>(ReputationFlags::AtWar)) != 0 && !HasFlag(definition.flags, DefinitionFlags::CanSetAtWar))
        {
            NC_LOG_ERROR("Faction {0} cannot be set at war", factionID);
            return false;
        }

        ReputationState* state = FindReputation(reputation, factionIndex);
        if (!state || state->flags == flags)
            return false;

        state->flags = flags;
        MarkDirty(reputation, factionIndex, source);
        MarkNetworkUpdate(reputation, factionIndex, true);
        return true;
    }

    bool SetReputationFlags(World& world, entt::entity characterEntity, const FactionRuntimeData& runtime, FactionID factionID, u16 flags, ReputationSource source)
    {
        CharacterReputation* reputation = world.TryGet<CharacterReputation>(characterEntity);
        return reputation && SetReputationFlags(runtime, *reputation, factionID, flags, source);
    }

    Reaction GetReaction(const FactionRuntimeData& runtime, const UnitFaction& observerFaction, const CharacterReputation* observerReputation, const UnitFaction& targetFaction, const CharacterReputation* targetReputation, const FactionModifiers* observerModifiers)
    {
        if (observerFaction.effectiveFaction == NONE_FACTION_INDEX || targetFaction.effectiveFaction == NONE_FACTION_INDEX)
            return Reaction::Neutral;

        if (IsValidReaction(observerFaction.effectiveUnitReactionOverride))
            return static_cast<Reaction>(observerFaction.effectiveUnitReactionOverride);

        if (observerReputation)
            return MatrixOrPlayerReputation(runtime, observerFaction, observerReputation, targetFaction, observerModifiers);

        if (targetReputation)
            return UnitToPlayer(runtime, observerFaction, targetFaction, targetReputation);

        return runtime.GetRelation(observerFaction.effectiveFaction, targetFaction.effectiveFaction);
    }

    Reaction GetReaction(World& world, entt::entity observer, entt::entity target, const FactionRuntimeData& runtime)
    {
        const UnitFaction* observerFaction = world.TryGet<UnitFaction>(observer);
        const UnitFaction* targetFaction = world.TryGet<UnitFaction>(target);
        if (!observerFaction || !targetFaction)
            return Reaction::Neutral;

        return GetReaction(runtime, *observerFaction, world.TryGet<CharacterReputation>(observer), *targetFaction, world.TryGet<CharacterReputation>(target), world.TryGet<FactionModifiers>(observer));
    }

    Reaction GetPresentationReaction(const FactionRuntimeData& runtime, const UnitFaction& playerFaction, const CharacterReputation& playerReputation, const UnitFaction& unitFaction, const FactionModifiers* playerModifiers)
    {
        if (playerFaction.effectiveFaction == NONE_FACTION_INDEX || unitFaction.effectiveFaction == NONE_FACTION_INDEX)
            return Reaction::Neutral;

        return PresentationUnitToPlayer(runtime, unitFaction, playerFaction, playerReputation, playerModifiers);
    }

    Reaction GetPresentationReaction(World& world, entt::entity player, entt::entity unit, const FactionRuntimeData& runtime)
    {
        const UnitFaction* playerFaction = world.TryGet<UnitFaction>(player);
        const CharacterReputation* playerReputation = world.TryGet<CharacterReputation>(player);
        const UnitFaction* unitFaction = world.TryGet<UnitFaction>(unit);
        if (!playerFaction || !playerReputation || !unitFaction)
            return Reaction::Neutral;

        return GetPresentationReaction(runtime, *playerFaction, *playerReputation, *unitFaction, world.TryGet<FactionModifiers>(player));
    }

    bool IsHostileTo(const FactionRuntimeData& runtime, const UnitFaction& observerFaction, const CharacterReputation* observerReputation, const UnitFaction& targetFaction, const CharacterReputation* targetReputation, const FactionModifiers* observerModifiers)
    {
        return GetReaction(runtime, observerFaction, observerReputation, targetFaction, targetReputation, observerModifiers) == Reaction::Hostile;
    }

    bool IsHostileTo(World& world, entt::entity observer, entt::entity target, const FactionRuntimeData& runtime)
    {
        return GetReaction(world, observer, target, runtime) == Reaction::Hostile;
    }

    bool CanAttack(const FactionRuntimeData& runtime, const UnitFaction& attackerFaction, const CharacterReputation* attackerReputation, const UnitFaction& targetFaction, const CharacterReputation* targetReputation, const FactionModifiers* attackerModifiers, const FactionModifiers* targetModifiers)
    {
        if (HasAtWarFlag(runtime, attackerReputation, targetFaction))
            return true;

        const Reaction attackerToTarget = GetReaction(runtime, attackerFaction, attackerReputation, targetFaction, targetReputation, attackerModifiers);
        const Reaction targetToAttacker = GetReaction(runtime, targetFaction, targetReputation, attackerFaction, attackerReputation, targetModifiers);
        return attackerToTarget != Reaction::Friendly && targetToAttacker != Reaction::Friendly;
    }

    bool CanAttack(World& world, entt::entity attacker, entt::entity target, const FactionRuntimeData& runtime)
    {
        if (attacker == target)
            return false;

        const UnitFaction* attackerFaction = world.TryGet<UnitFaction>(attacker);
        const UnitFaction* targetFaction = world.TryGet<UnitFaction>(target);
        if (!attackerFaction || !targetFaction)
            return false;

        return CanAttack(runtime, *attackerFaction, world.TryGet<CharacterReputation>(attacker), *targetFaction, world.TryGet<CharacterReputation>(target), world.TryGet<FactionModifiers>(attacker), world.TryGet<FactionModifiers>(target));
    }

    bool CanAssist(const FactionRuntimeData& runtime, const UnitFaction& actorFaction, const CharacterReputation* actorReputation, const UnitFaction& targetFaction, const CharacterReputation* targetReputation, const FactionModifiers* actorModifiers)
    {
        return GetReaction(runtime, actorFaction, actorReputation, targetFaction, targetReputation, actorModifiers) == Reaction::Friendly;
    }

    bool CanAssist(World& world, entt::entity actor, entt::entity target, const FactionRuntimeData& runtime)
    {
        const UnitFaction* actorFaction = world.TryGet<UnitFaction>(actor);
        const UnitFaction* targetFaction = world.TryGet<UnitFaction>(target);
        if (!actorFaction || !targetFaction)
            return false;

        return CanAssist(runtime, *actorFaction, world.TryGet<CharacterReputation>(actor), *targetFaction, world.TryGet<CharacterReputation>(target), world.TryGet<FactionModifiers>(actor));
    }

    bool CanInteract(const FactionRuntimeData& runtime, const UnitFaction& playerFaction, const CharacterReputation& playerReputation, const UnitFaction& unitFaction, const FactionModifiers* playerModifiers)
    {
        const Reaction reaction = GetReaction(runtime, playerFaction, &playerReputation, unitFaction, nullptr, playerModifiers);
        return reaction == Reaction::Neutral || reaction == Reaction::Friendly;
    }

    bool CanInteract(World& world, entt::entity player, entt::entity unit, const FactionRuntimeData& runtime)
    {
        const UnitFaction* playerFaction = world.TryGet<UnitFaction>(player);
        const CharacterReputation* playerReputation = world.TryGet<CharacterReputation>(player);
        const UnitFaction* unitFaction = world.TryGet<UnitFaction>(unit);
        return playerFaction && playerReputation && unitFaction && CanInteract(runtime, *playerFaction, *playerReputation, *unitFaction, world.TryGet<FactionModifiers>(player));
    }
}
