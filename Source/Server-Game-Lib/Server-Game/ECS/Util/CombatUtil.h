#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct CreatureThreatTable;
        struct UnitCombatInfo;
    }

    namespace Singletons
    {
    }
}

namespace ECS::Util::Combat
{
    bool IsUnitInCombat(World& world, entt::entity entity);

    bool AddUnitToThreatTable(World& world, Components::UnitCombatInfo& targetCombatInfo, entt::entity creatureEntity, entt::entity targetEntity, f64 threatAmount = 0.0f, bool setInCombat = true);
    void AddUnitToThreatTables(World& world, Components::UnitCombatInfo& targetCombatInfo, entt::entity creatureEntity, entt::entity targetEntity, f64 threatAmount = 0.0f, bool setInCombat = true);
    bool RemoveUnitFromThreatTable(Components::CreatureThreatTable& threatTable, ObjectGUID targetGUID);
    void SortThreatTable(Components::CreatureThreatTable& threatTable);
    void ReindexThreatTable(Components::CreatureThreatTable& threatTable);
}