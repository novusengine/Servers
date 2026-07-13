#include "CombatUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include "Server-Game/ECS/Components/CreatureThreatTable.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
#include "Server-Game/ECS/Components/UnitSpellCooldownHistory.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <entt/entt.hpp>

using namespace ECS;

namespace ECS::Util::Combat
{
    bool IsUnitInCombat(World& world, entt::entity entity)
    {
        bool isInCombat = world.AllOf<Tags::IsInCombat>(entity);
        return isInCombat;
    }

    bool AddUnitToThreatTable(World& world, Components::UnitCombatInfo& targetCombatInfo, entt::entity creatureEntity, entt::entity targetEntity, f64 threatAmount, bool setInCombat)
    {
        auto* creatureThreatTable = world.TryGet<Components::CreatureThreatTable>(creatureEntity);
        if (!creatureThreatTable)
            return false;

        auto& creatureObjectInfo = world.Get<Components::ObjectInfo>(creatureEntity);
        auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

        auto itr = creatureThreatTable->guidToIndex.find(targetObjectInfo.guid);
        if (itr == creatureThreatTable->guidToIndex.end())
        {

            u32 index = static_cast<u32>(creatureThreatTable->threatList.size());
            creatureThreatTable->guidToIndex[targetObjectInfo.guid] = index;

            ThreatEntry& entry = creatureThreatTable->threatList.emplace_back();
            entry.guid = targetObjectInfo.guid;
            entry.threatValue = threatAmount;

            targetCombatInfo.threatTables[creatureObjectInfo.guid] = ECS::ThreatTableEntry{
                .timeSinceLastActivity = 0.0f
            };
        }
        else
        {
            u32 index = itr->second;
            creatureThreatTable->threatList[index].threatValue += threatAmount;

            targetCombatInfo.threatTables[creatureObjectInfo.guid].timeSinceLastActivity = 0.0f;
        }

        world.EmplaceOrReplace<Events::CreatureNeedsThreatTableUpdate>(creatureEntity);

        if (setInCombat)
        {
            if (!world.AllOf<Tags::IsInCombat>(creatureEntity))
                world.EmplaceOrReplace<Events::UnitEnterCombat>(creatureEntity, targetEntity);

            if (!world.AllOf<Tags::IsInCombat>(targetEntity))
                world.EmplaceOrReplace<Events::UnitEnterCombat>(targetEntity, creatureEntity);
        }

        return true;
    }

    void AddUnitToThreatTables(World& world, Components::UnitCombatInfo& targetCombatInfo, entt::entity creatureEntity, entt::entity targetEntity, f64 threatAmount, bool setInCombat)
    {
        for (const auto& threatTableEntry : targetCombatInfo.threatTables)
        {
            entt::entity threatEntity = world.GetEntity(threatTableEntry.first);
            if (threatEntity == targetEntity)
                continue;

            Util::Combat::AddUnitToThreatTable(world, targetCombatInfo, threatEntity, targetEntity, threatAmount, setInCombat);
        }
    }

    bool RemoveUnitFromThreatTable(Components::CreatureThreatTable& threatTable, ObjectGUID targetGUID)
    {
        auto itr = threatTable.guidToIndex.find(targetGUID);
        if (itr == threatTable.guidToIndex.end())
            return false;

        u32 index = itr->second;
        threatTable.threatList.erase(threatTable.threatList.begin() + index);
        threatTable.guidToIndex.erase(itr);

        ReindexThreatTable(threatTable);
        return true;
    }

    void SortThreatTable(Components::CreatureThreatTable& threatTable)
    {
        std::sort(threatTable.threatList.begin(), threatTable.threatList.end(), [](const ThreatEntry& a, const ThreatEntry& b)
        {
            return a.threatValue > b.threatValue;
        });

        ReindexThreatTable(threatTable);
    }
    void ReindexThreatTable(Components::CreatureThreatTable& threatTable)
    {
        threatTable.guidToIndex.clear();

        u32 numEntries = static_cast<u32>(threatTable.threatList.size());
        for (u32 i = 0; i < numEntries; i++)
        {
            const ThreatEntry& entry = threatTable.threatList[i];
            threatTable.guidToIndex[entry.guid] = i;
        }
    }
}