#include "FactionModifierUtil.h"

#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/CharacterReputation.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/UnitFaction.h"
#include "Server-Game/ECS/Singletons/FactionModifierState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Gameplay/Faction/FactionRuntimeData.h"

#include <Base/Util/DebugHandler.h>

#include <MetaGen/Shared/Spell/Spell.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace ECS::Util::FactionModifier
{
    using namespace Components;
    using namespace Gameplay::Faction;

    namespace
    {
        bool IsLocalField(ModifierField field)
        {
            return field == ModifierField::LocalStanding || field == ModifierField::LocalReaction;
        }

        bool IsEarlier(const ModifierContributor& left, const ModifierContributor& right)
        {
            if (left.priority != right.priority)
                return left.priority > right.priority;

            if (left.applicationSequence != right.applicationSequence)
                return left.applicationSequence < right.applicationSequence;

            if (left.effectIndex != right.effectIndex)
                return left.effectIndex < right.effectIndex;

            return left.effectID < right.effectID;
        }

        bool TryGetFactionIndex(const FactionRuntimeData& runtime, i32 rawFactionID, FactionIndex& factionIndex)
        {
            if (rawFactionID < 0 || rawFactionID > std::numeric_limits<FactionID>::max() || !runtime.TryGetFactionIndex(static_cast<FactionID>(rawFactionID), factionIndex))
            {
                NC_LOG_ERROR("Faction modifier references unknown faction ID {0}", rawFactionID);
                return false;
            }

            return true;
        }

        bool TryBuildContributor(const FactionRuntimeData& runtime, const AuraInfo& auraInfo, const AuraEffect& effect, u8 effectIndex, u64 applicationSequence, ModifierContributor& contributor)
        {
            using EffectType = MetaGen::Shared::Spell::SpellEffectTypeEnum;

            contributor = {
                .sourceAuraInstanceID = 0,
                .spellID = auraInfo.spellID,
                .effectID = effect.effectID,
                .effectIndex = effectIndex,
                .priority = effect.priority,
                .applicationSequence = applicationSequence
            };

            const EffectType type = static_cast<EffectType>(effect.type);
            switch (type)
            {
                case EffectType::AuraFactionOverride:
                {
                    FactionIndex faction = INVALID_FACTION_INDEX;
                    if (!TryGetFactionIndex(runtime, effect.value1, faction))
                        return false;
                    contributor.field = ModifierField::EffectiveFaction;
                    contributor.value = faction;
                    return true;
                }
                case EffectType::AuraFactionStandingOverride:
                {
                    FactionIndex faction = INVALID_FACTION_INDEX;
                    if (!TryGetFactionIndex(runtime, effect.miscValue1, faction) || faction == NONE_FACTION_INDEX || !HasFlag(runtime.GetDefinition(faction).flags, DefinitionFlags::AllowsReputation))
                    {
                        NC_LOG_ERROR("Faction standing modifier requires a reputation-enabled faction ID {0}", effect.miscValue1);
                        return false;
                    }
                    contributor.field = ModifierField::LocalStanding;
                    contributor.factionSelector = faction;
                    contributor.value = effect.value1;
                    return true;
                }
                case EffectType::AuraFactionReactionOverride:
                {
                    FactionIndex faction = INVALID_FACTION_INDEX;
                    if (!TryGetFactionIndex(runtime, effect.miscValue1, faction) || faction == NONE_FACTION_INDEX || !IsValidReaction(effect.value1))
                    {
                        NC_LOG_ERROR("Faction reaction modifier has invalid faction ID {0} or reaction {1}", effect.miscValue1, effect.value1);
                        return false;
                    }
                    contributor.field = ModifierField::LocalReaction;
                    contributor.factionSelector = faction;
                    contributor.value = effect.value1;
                    return true;
                }
                case EffectType::AuraFactionPlayerReactionMinOverride:
                    contributor.field = ModifierField::PlayerReactionMinimum;
                    contributor.value = effect.value1;
                    break;
                case EffectType::AuraFactionPlayerReactionMaxOverride:
                    contributor.field = ModifierField::PlayerReactionMaximum;
                    contributor.value = effect.value1;
                    break;
                case EffectType::AuraFactionUnitReactionOverride:
                    contributor.field = ModifierField::UnitReaction;
                    contributor.value = effect.value1;
                    break;
                default:
                    return false;
            }

            if (!IsValidReaction(contributor.value))
            {
                NC_LOG_ERROR("Faction modifier spell {0} effect {1} has invalid reaction {2}", auraInfo.spellID, effect.effectID, contributor.value);
                return false;
            }

            return true;
        }

        u64 AcquireApplicationSequence(World& world)
        {
            Singletons::FactionModifierState* state = world.FindSingleton<Singletons::FactionModifierState>();
            if (!state)
                state = &world.EmplaceSingleton<Singletons::FactionModifierState>();
            if (state->nextApplicationSequence == 0 || state->nextApplicationSequence == std::numeric_limits<u64>::max())
            {
                NC_LOG_CRITICAL("Faction modifier application sequence exhausted for map {0}", world.mapID);
                return 0;
            }

            return state->nextApplicationSequence++;
        }

        bool HasFactionModifierEffect(const AuraEffectInfo& auraEffectInfo)
        {
            using EffectType = MetaGen::Shared::Spell::SpellEffectTypeEnum;
            return std::any_of(auraEffectInfo.effects.begin(), auraEffectInfo.effects.end(), [](const auto& effect)
            {
                switch (static_cast<EffectType>(effect.type))
                {
                    case EffectType::AuraFactionOverride:
                    case EffectType::AuraFactionStandingOverride:
                    case EffectType::AuraFactionReactionOverride:
                    case EffectType::AuraFactionPlayerReactionMinOverride:
                    case EffectType::AuraFactionPlayerReactionMaxOverride:
                    case EffectType::AuraFactionUnitReactionOverride:
                        return true;
                    default:
                        return false;
                }
            });
        }

        bool ApplyAuraWithSequence(World& world, entt::entity auraEntity, entt::entity targetEntity, const AuraInfo& auraInfo, const AuraEffectInfo& auraEffectInfo, const FactionRuntimeData& runtime, u64 applicationSequence)
        {
            std::vector<ModifierContributor> contributors;
            contributors.reserve(auraEffectInfo.effects.size());
            const u32 auraInstanceID = entt::to_integral(auraEntity);

            for (u32 i = 0; i < auraEffectInfo.effects.size(); ++i)
            {
                ModifierContributor contributor;
                if (!TryBuildContributor(runtime, auraInfo, auraEffectInfo.effects[i], static_cast<u8>(i), applicationSequence, contributor))
                    continue;

                contributor.sourceAuraInstanceID = auraInstanceID;
                contributors.push_back(contributor);
            }

            if (contributors.empty())
                return false;

            if (!world.AllOf<UnitFaction>(targetEntity))
            {
                NC_LOG_ERROR("Faction modifier aura {0} targets entity {1} without UnitFaction", auraInfo.spellID, entt::to_integral(targetEntity));
                return false;
            }

            const bool hasLocalContributor = std::any_of(contributors.begin(), contributors.end(),
                [](const ModifierContributor& contributor)
            {
                return IsLocalField(contributor.field);
            });
            if (hasLocalContributor && !world.AllOf<CharacterReputation>(targetEntity))
            {
                NC_LOG_ERROR("Player-local faction modifier aura {0} targets non-character entity {1}", auraInfo.spellID, entt::to_integral(targetEntity));
                return false;
            }

            FactionModifiers& modifiers = world.GetOrEmplace<FactionModifiers>(targetEntity);
            if (modifiers.contributors.size() + contributors.size() > MAX_FACTION_MODIFIER_CONTRIBUTORS)
            {
                NC_LOG_ERROR("Faction modifier contributor limit {0} exceeded for entity {1}", MAX_FACTION_MODIFIER_CONTRIBUTORS, entt::to_integral(targetEntity));
                return false;
            }

            for (const ModifierContributor& contributor : contributors)
            {
                AddContributor(modifiers, contributor, entt::to_integral(targetEntity), world.mapID);
            }

            RecomputeResolvedState(world, targetEntity, modifiers);
            return true;
        }
    }

    const ModifierContributor* FindWinningContributor(const FactionModifiers& modifiers, ModifierField field, FactionIndex factionSelector)
    {
        const ModifierContributor* winner = nullptr;
        for (const ModifierContributor& contributor : modifiers.contributors)
        {
            if (contributor.field != field || contributor.factionSelector != factionSelector)
                continue;

            if (!winner || IsEarlier(contributor, *winner))
                winner = &contributor;
        }

        return winner;
    }

    bool AddContributor(FactionModifiers& modifiers, const ModifierContributor& contributor, u32 unitEntity, u32 mapID, FactionModifierConflictReporter reporter)
    {
        if (modifiers.contributors.size() >= MAX_FACTION_MODIFIER_CONTRIBUTORS)
            return false;

        const ModifierContributor* equalPriorityIncumbent = nullptr;
        for (const ModifierContributor& existing : modifiers.contributors)
        {
            if (existing.field != contributor.field || existing.factionSelector != contributor.factionSelector || existing.priority != contributor.priority)
            {
                continue;
            }

            if (!equalPriorityIncumbent || IsEarlier(existing, *equalPriorityIncumbent))
                equalPriorityIncumbent = &existing;
        }

        if (reporter && equalPriorityIncumbent && equalPriorityIncumbent->value != contributor.value)
        {
            reporter(FactionModifierConflict{
                .field = contributor.field,
                .priority = contributor.priority,
                .incumbentSpellID = equalPriorityIncumbent->spellID,
                .incumbentEffectID = equalPriorityIncumbent->effectID,
                .incumbentOutcome = equalPriorityIncumbent->value,
                .incumbentApplicationSequence = equalPriorityIncumbent->applicationSequence,
                .contenderSpellID = contributor.spellID,
                .contenderEffectID = contributor.effectID,
                .contenderOutcome = contributor.value,
                .contenderApplicationSequence = contributor.applicationSequence,
                .unitEntity = unitEntity,
                .mapID = mapID
            });
        }

        modifiers.contributors.push_back(contributor);
        return true;
    }

    u32 RemoveContributors(FactionModifiers& modifiers, u32 sourceAuraInstanceID)
    {
        const size_t oldSize = modifiers.contributors.size();
        std::erase_if(modifiers.contributors, [sourceAuraInstanceID](const ModifierContributor& contributor)
        {
            return contributor.sourceAuraInstanceID == sourceAuraInstanceID;
        });
        return static_cast<u32>(oldSize - modifiers.contributors.size());
    }

    void RecomputeResolvedState(World& world, entt::entity targetEntity, FactionModifiers& modifiers)
    {
        UnitFaction* unitFaction = world.TryGet<UnitFaction>(targetEntity);
        if (!unitFaction)
            return;

        const FactionIndex oldFaction = unitFaction->effectiveFaction;
        const u8 oldBounds = unitFaction->effectivePlayerReactionBounds;

        unitFaction->flags = 0;
        unitFaction->effectiveFaction = unitFaction->assignedFaction;
        if (const ModifierContributor* winner = FindWinningContributor(modifiers, ModifierField::EffectiveFaction))
        {
            unitFaction->effectiveFaction = static_cast<FactionIndex>(winner->value);
            unitFaction->flags |= static_cast<u8>(UnitFactionFlags::HasEffectiveFactionOverride);
        }

        ReactionBounds bounds = ReactionBounds::IsValidPacked(unitFaction->assignedPlayerReactionBounds)
            ? ReactionBounds::Unpack(unitFaction->assignedPlayerReactionBounds)
            : ReactionBounds::Unpack(NEUTRAL_REACTION_BOUNDS);

        if (const ModifierContributor* winner = FindWinningContributor(modifiers, ModifierField::PlayerReactionMinimum))
        {
            bounds.minimum = static_cast<Reaction>(winner->value);
            unitFaction->flags |= static_cast<u8>(UnitFactionFlags::HasPlayerReactionBoundsOverride);
        }

        if (const ModifierContributor* winner = FindWinningContributor(modifiers, ModifierField::PlayerReactionMaximum))
        {
            bounds.maximum = static_cast<Reaction>(winner->value);
            unitFaction->flags |= static_cast<u8>(UnitFactionFlags::HasPlayerReactionBoundsOverride);
        }

        if (!bounds.IsValid())
        {
            NC_LOG_ERROR("Faction modifiers resolve invalid player reaction bounds for entity {0}; Neutral will be used", entt::to_integral(targetEntity));
            unitFaction->effectivePlayerReactionBounds = NEUTRAL_REACTION_BOUNDS;
        }
        else
        {
            unitFaction->effectivePlayerReactionBounds = bounds.Pack();
        }

        unitFaction->effectiveUnitReactionOverride = INHERIT_REACTION_BOUND;
        if (const ModifierContributor* winner = FindWinningContributor(modifiers, ModifierField::UnitReaction))
        {
            unitFaction->effectiveUnitReactionOverride = static_cast<u8>(winner->value);
            unitFaction->flags |= static_cast<u8>(UnitFactionFlags::HasUnitReactionOverride);
        }

        if (unitFaction->effectiveFaction != oldFaction || unitFaction->effectivePlayerReactionBounds != oldBounds)
            world.EmplaceOrReplace<Events::UnitNeedsFactionUpdate>(targetEntity);

        robin_hood::unordered_flat_set<FactionIndex> selectors;
        selectors.reserve(modifiers.perceptionByFaction.size() + modifiers.contributors.size());
        for (const auto& [faction, perception] : modifiers.perceptionByFaction)
        {
            selectors.insert(faction);
        }

        for (const ModifierContributor& contributor : modifiers.contributors)
        {
            if (IsLocalField(contributor.field))
                selectors.insert(contributor.factionSelector);
        }

        for (FactionIndex faction : selectors)
        {
            ResolvedFactionPerception resolved;
            if (const ModifierContributor* standing = FindWinningContributor(modifiers, ModifierField::LocalStanding, faction))
            {
                resolved.effectiveStandingValue = standing->value;
                resolved.activeFields |= static_cast<u8>(PerceptionOverrideFields::Standing);
            }

            if (const ModifierContributor* reaction = FindWinningContributor(modifiers, ModifierField::LocalReaction, faction))
            {
                resolved.effectiveReaction = static_cast<Reaction>(reaction->value);
                resolved.activeFields |= static_cast<u8>(PerceptionOverrideFields::Reaction);
            }

            const auto oldItr = modifiers.perceptionByFaction.find(faction);
            const bool hadOld = oldItr != modifiers.perceptionByFaction.end();
            const bool changed = !hadOld || oldItr->second.activeFields != resolved.activeFields || oldItr->second.effectiveStandingValue != resolved.effectiveStandingValue || oldItr->second.effectiveReaction != resolved.effectiveReaction;

            if (resolved.activeFields == 0)
            {
                if (hadOld)
                {
                    modifiers.perceptionByFaction.erase(faction);
                    modifiers.pendingPerceptionNetwork.insert(faction);
                }
            }
            else
            {
                modifiers.perceptionByFaction.insert_or_assign(faction, resolved);
                if (changed)
                    modifiers.pendingPerceptionNetwork.insert(faction);
            }
        }
    }

    bool ApplyAura(World& world, entt::entity auraEntity, entt::entity targetEntity, const AuraInfo& auraInfo, const AuraEffectInfo& auraEffectInfo, const FactionRuntimeData& runtime)
    {
        if (world.AllOf<FactionModifierApplication>(auraEntity))
            return RefreshAura(world, auraEntity, targetEntity, auraInfo, auraEffectInfo, runtime);

        if (!HasFactionModifierEffect(auraEffectInfo))
            return false;

        const u64 sequence = AcquireApplicationSequence(world);
        if (sequence == 0 || !ApplyAuraWithSequence(world, auraEntity, targetEntity, auraInfo, auraEffectInfo, runtime, sequence))
            return false;

        world.Emplace<FactionModifierApplication>(auraEntity, FactionModifierApplication{ .applicationSequence = sequence });
        return true;
    }

    bool RefreshAura(World& world, entt::entity auraEntity, entt::entity targetEntity, const AuraInfo& auraInfo, const AuraEffectInfo& auraEffectInfo, const FactionRuntimeData& runtime)
    {
        const FactionModifierApplication* application = world.TryGet<FactionModifierApplication>(auraEntity);
        if (!application)
            return ApplyAura(world, auraEntity, targetEntity, auraInfo, auraEffectInfo, runtime);

        FactionModifiers* modifiers = world.TryGet<FactionModifiers>(targetEntity);
        std::vector<ModifierContributor> previousContributors;
        if (modifiers)
        {
            const u32 auraInstanceID = entt::to_integral(auraEntity);
            for (const ModifierContributor& contributor : modifiers->contributors)
            {
                if (contributor.sourceAuraInstanceID == auraInstanceID)
                    previousContributors.push_back(contributor);
            }

            RemoveContributors(*modifiers, auraInstanceID);
        }

        const bool applied = ApplyAuraWithSequence(world, auraEntity, targetEntity, auraInfo, auraEffectInfo, runtime, application->applicationSequence);
        if (!applied && modifiers)
        {
            modifiers->contributors.insert(modifiers->contributors.end(), previousContributors.begin(), previousContributors.end());
            RecomputeResolvedState(world, targetEntity, *modifiers);
        }

        return applied;
    }

    bool RemoveAura(World& world, entt::entity auraEntity, entt::entity targetEntity)
    {
        if (!world.AllOf<FactionModifierApplication>(auraEntity))
            return false;

        FactionModifiers* modifiers = world.TryGet<FactionModifiers>(targetEntity);
        if (!modifiers || RemoveContributors(*modifiers, entt::to_integral(auraEntity)) == 0)
            return false;

        RecomputeResolvedState(world, targetEntity, *modifiers);
        return true;
    }
}
