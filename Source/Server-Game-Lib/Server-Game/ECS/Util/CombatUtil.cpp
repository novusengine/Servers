#include "CombatUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include "Server-Game/ECS/Components/CreatureThreatTable.h"
#include "Server-Game/ECS/Components/CreatureCombatInfo.h"
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

#include <algorithm>
#include <cmath>

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
        if (const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(creatureEntity);
            creatureCombatInfo && creatureCombatInfo->isEvading)
        {
            return false;
        }

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

    void ClearCreatureThreatTable(World& world, entt::entity creatureEntity)
    {
        auto* threatTable = world.TryGet<Components::CreatureThreatTable>(creatureEntity);
        auto* objectInfo = world.TryGet<Components::ObjectInfo>(creatureEntity);
        if (!threatTable || !objectInfo)
            return;

        for (const ThreatEntry& entry : threatTable->threatList)
        {
            entt::entity targetEntity = world.GetEntity(entry.guid);
            if (!world.ValidEntity(targetEntity))
                continue;

            if (auto* targetCombatInfo = world.TryGet<Components::UnitCombatInfo>(targetEntity))
                targetCombatInfo->threatTables.erase(objectInfo->guid);
        }

        threatTable->threatList.clear();
        threatTable->guidToIndex.clear();

        auto* combatInfo = world.TryGet<Components::UnitCombatInfo>(creatureEntity);
        if (!combatInfo)
            return;

        for (const auto& pair : combatInfo->threatTables)
        {
            entt::entity otherCreatureEntity = world.GetEntity(pair.first);
            if (!world.ValidEntity(otherCreatureEntity))
                continue;

            if (auto* otherThreatTable = world.TryGet<Components::CreatureThreatTable>(otherCreatureEntity))
                RemoveUnitFromThreatTable(*otherThreatTable, objectInfo->guid);
        }
        combatInfo->threatTables.clear();
    }

    void RefreshCreatureCombatParticipants(World& world, entt::entity creatureEntity, f32 range)
    {
        const auto* threatTable = world.TryGet<Components::CreatureThreatTable>(creatureEntity);
        const auto* objectInfo = world.TryGet<Components::ObjectInfo>(creatureEntity);
        const auto* transform = world.TryGet<Components::Transform>(creatureEntity);
        if (!threatTable || !objectInfo || !transform)
            return;

        const f32 clampedRange = std::max(range, 0.0f);
        const f32 rangeSq = clampedRange * clampedRange;
        for (const ThreatEntry& entry : threatTable->threatList)
        {
            entt::entity targetEntity = world.GetEntity(entry.guid);
            if (!world.ValidEntity(targetEntity) || !world.AllOf<Tags::IsAlive>(targetEntity))
                continue;

            const auto* targetTransform = world.TryGet<Components::Transform>(targetEntity);
            auto* targetCombatInfo = world.TryGet<Components::UnitCombatInfo>(targetEntity);
            if (!targetTransform || !targetCombatInfo)
                continue;

            const vec3 offset = targetTransform->position - transform->position;
            if (offset.x * offset.x + offset.z * offset.z > rangeSq || std::abs(offset.y) > clampedRange)
                continue;

            auto timerItr = targetCombatInfo->threatTables.find(objectInfo->guid);
            if (timerItr != targetCombatInfo->threatTables.end())
                timerItr->second.timeSinceLastActivity = 0.0f;
        }
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
