#pragma once

#include "Server-Game/Gameplay/Faction/FactionGameplayDefines.h"
#include "Server-Game/Gameplay/Faction/FactionRuntimeData.h"

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct CharacterReputation;
        struct FactionModifiers;
        struct ReputationState;
        struct UnitFaction;
    }
}

namespace ECS::Util::Faction
{
    bool TryGetFactionIndex(const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, Gameplay::Faction::FactionIndex& factionIndex);
    Gameplay::Faction::FactionID GetFactionID(const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionIndex factionIndex);

    bool SetAssignedFaction(Components::UnitFaction& unitFaction, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);
    bool SetAssignedFaction(World& world, entt::entity entity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);
    bool SetAssignedPlayerReactionBounds(Components::UnitFaction& unitFaction, Gameplay::Faction::Reaction minimum, Gameplay::Faction::Reaction maximum);
    bool SetAssignedPlayerReactionBounds(World& world, entt::entity entity, Gameplay::Faction::Reaction minimum, Gameplay::Faction::Reaction maximum);

    const Components::ReputationState* FindReputation(const Components::CharacterReputation& reputation, Gameplay::Faction::FactionIndex factionIndex);
    Components::ReputationState* FindReputation(Components::CharacterReputation& reputation, Gameplay::Faction::FactionIndex factionIndex);
    const Components::ReputationState* FindReputation(const Components::CharacterReputation& reputation, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);

    bool DiscoverReputation(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& characterFaction, Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID, Gameplay::Faction::DiscoverySource source);
    bool DiscoverReputation(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, Gameplay::Faction::DiscoverySource source);
    i32 GetPersistentReputationValue(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID);
    i32 GetPersistentReputationValue(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);
    const Gameplay::Faction::StandingThreshold& GetPersistentStanding(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID);
    const Gameplay::Faction::StandingThreshold& GetPersistentStanding(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);
    const Gameplay::Faction::StandingThreshold& GetEffectiveStanding(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID);
    const Gameplay::Faction::StandingThreshold& GetEffectiveStanding(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID);
    bool SetReputation(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& characterFaction, Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID, i32 value, Gameplay::Faction::ReputationSource source);
    bool SetReputation(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, i32 value, Gameplay::Faction::ReputationSource source);
    bool ModifyReputation(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& characterFaction, Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID, i32 delta, Gameplay::Faction::ReputationSource source);
    bool ModifyReputation(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, i32 delta, Gameplay::Faction::ReputationSource source);
    bool RemoveReputation(const Gameplay::Faction::FactionRuntimeData& runtime, Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID, Gameplay::Faction::ReputationSource source);
    bool RemoveReputation(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, Gameplay::Faction::ReputationSource source);
    bool SetReputationFlags(const Gameplay::Faction::FactionRuntimeData& runtime, Components::CharacterReputation& reputation, Gameplay::Faction::FactionID factionID, u16 flags, Gameplay::Faction::ReputationSource source);
    bool SetReputationFlags(World& world, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, u16 flags, Gameplay::Faction::ReputationSource source);

    Gameplay::Faction::Reaction GetReaction(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& observerFaction, const Components::CharacterReputation* observerReputation, const Components::UnitFaction& targetFaction, const Components::CharacterReputation* targetReputation, const Components::FactionModifiers* observerModifiers = nullptr);
    Gameplay::Faction::Reaction GetReaction(World& world, entt::entity observer, entt::entity target, const Gameplay::Faction::FactionRuntimeData& runtime);
    Gameplay::Faction::Reaction GetPresentationReaction(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& playerFaction, const Components::CharacterReputation& playerReputation, const Components::UnitFaction& unitFaction, const Components::FactionModifiers* playerModifiers = nullptr);
    Gameplay::Faction::Reaction GetPresentationReaction(World& world, entt::entity player, entt::entity unit, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool IsHostileTo(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& observerFaction, const Components::CharacterReputation* observerReputation, const Components::UnitFaction& targetFaction, const Components::CharacterReputation* targetReputation, const Components::FactionModifiers* observerModifiers = nullptr);
    bool IsHostileTo(World& world, entt::entity observer, entt::entity target, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool CanAttack(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& attackerFaction, const Components::CharacterReputation* attackerReputation, const Components::UnitFaction& targetFaction, const Components::CharacterReputation* targetReputation, const Components::FactionModifiers* attackerModifiers = nullptr, const Components::FactionModifiers* targetModifiers = nullptr);
    bool CanAttack(World& world, entt::entity attacker, entt::entity target, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool CanAssist(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& actorFaction, const Components::CharacterReputation* actorReputation, const Components::UnitFaction& targetFaction, const Components::CharacterReputation* targetReputation, const Components::FactionModifiers* actorModifiers = nullptr);
    bool CanAssist(World& world, entt::entity actor, entt::entity target, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool CanInteract(const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& playerFaction, const Components::CharacterReputation& playerReputation, const Components::UnitFaction& unitFaction, const Components::FactionModifiers* playerModifiers = nullptr);
    bool CanInteract(World& world, entt::entity player, entt::entity unit, const Gameplay::Faction::FactionRuntimeData& runtime);
}
