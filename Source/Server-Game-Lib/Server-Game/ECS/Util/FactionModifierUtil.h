#pragma once

#include "Server-Game/ECS/Components/FactionModifiers.h"

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct AuraEffectInfo;
        struct AuraInfo;
        struct UnitFaction;
    }
}

namespace Gameplay::Faction
{
    struct FactionRuntimeData;
}

namespace ECS::Util::FactionModifier
{
    const Gameplay::Faction::ModifierContributor* FindWinningContributor(const Components::FactionModifiers& modifiers, Gameplay::Faction::ModifierField field, Gameplay::Faction::FactionIndex factionSelector = Gameplay::Faction::NONE_FACTION_INDEX);
    bool AddContributor(Components::FactionModifiers& modifiers, const Gameplay::Faction::ModifierContributor& contributor, u32 unitEntity, u32 mapID, Gameplay::Faction::FactionModifierConflictReporter reporter = Gameplay::Faction::ReportFactionModifierConflict);
    u32 RemoveContributors(Components::FactionModifiers& modifiers, u32 sourceAuraInstanceID);
    void RecomputeResolvedState(World& world, entt::entity targetEntity, Components::FactionModifiers& modifiers);

    bool ApplyAura(World& world, entt::entity auraEntity, entt::entity targetEntity, const Components::AuraInfo& auraInfo, const Components::AuraEffectInfo& auraEffectInfo, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool RefreshAura(World& world, entt::entity auraEntity, entt::entity targetEntity, const Components::AuraInfo& auraInfo, const Components::AuraEffectInfo& auraEffectInfo, const Gameplay::Faction::FactionRuntimeData& runtime);
    bool RemoveAura(World& world, entt::entity auraEntity, entt::entity targetEntity);
}
