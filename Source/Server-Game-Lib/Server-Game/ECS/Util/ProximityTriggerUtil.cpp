#include "ProximityTriggerUtil.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/ProximityTriggers.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/LuaManager.h>
#include <Scripting/Zenith.h>

namespace ECS::Util::ProximityTrigger
{
    bool ProximityTriggerExists(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID)
    {
        bool exists = proximityTriggers.triggerIDToEntity.contains(triggerID);
        return exists;
    }
    bool ProximityTriggerGetByID(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID, entt::entity& entity)
    {
        if (!ProximityTriggerExists(proximityTriggers, triggerID))
            return false;

        entity = proximityTriggers.triggerIDToEntity[triggerID];
        return true;
    }

    bool AddTriggerToWorld(Singletons::ProximityTriggers& proximityTriggers, entt::entity entity, u32 triggerID, const Components::WorldAABB& worldAABB, bool isServerSideOnly)
    {
        if (ProximityTriggerExists(proximityTriggers, triggerID))
            return false;

        proximityTriggers.triggerIDToEntity[triggerID] = entity;

        if (isServerSideOnly)
            proximityTriggers.triggerVisTree.Insert(reinterpret_cast<const f32*>(&worldAABB.min.x), reinterpret_cast<const f32*>(&worldAABB.max.x), triggerID);

        return true;
    }
    bool RemoveTriggerFromWorld(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID, bool isServerSideOnly)
    {
        if (!ProximityTriggerExists(proximityTriggers, triggerID))
            return false;

        proximityTriggers.triggerIDToEntity.erase(triggerID);

        if (isServerSideOnly)
            proximityTriggers.triggerVisTree.Remove(triggerID);

        return true;
    }

    bool IsPlayerInTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity)
    {
        bool isInTrigger = proximityTrigger.playersInside.contains(playerEntity);
        return isInTrigger;
    }
    bool AddPlayerToTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity)
    {
        if (IsPlayerInTrigger(proximityTrigger, playerEntity))
            return false;

        proximityTrigger.playersInside.insert(playerEntity);
        return true;
    }
    bool RemovePlayerFromTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity)
    {
        if (!IsPlayerInTrigger(proximityTrigger, playerEntity))
            return false;

        proximityTrigger.playersInside.erase(playerEntity);
        return true;
    }

    bool IsPlayerInTriggerEntered(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity)
    {
        bool isInTriggerEntered = proximityTrigger.playersEntered.contains(playerEntity);
        return isInTriggerEntered;
    }
    bool AddPlayerToTriggerEntered(World& world, Components::ProximityTrigger& proximityTrigger, entt::entity triggerEntity, entt::entity playerEntity)
    {
        if (IsPlayerInTriggerEntered(proximityTrigger, playerEntity))
            return false;

        proximityTrigger.playersEntered.insert(playerEntity);
        world.EmplaceOrReplace<Tags::ProximityTriggerHasEnteredPlayers>(triggerEntity);
        return true;
    }
    bool RemovePlayerFromTriggerEntered(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity)
    {
        if (!IsPlayerInTriggerEntered(proximityTrigger, playerEntity))
            return false;

        proximityTrigger.playersExited.erase(playerEntity);
        return true;
    }

    void OnEnter(World& world, Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(world.zenithKey);

        zenith->CallEvent(MetaGen::Server::Lua::TriggerEvent::OnEnter, MetaGen::Server::Lua::TriggerEventDataOnEnter{
            .triggerID = trigger.triggerID,
            .playerID = entt::to_integral(playerEntity)
        });
    }
    void OnExit(World& world, Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(world.zenithKey);

        zenith->CallEvent(MetaGen::Server::Lua::TriggerEvent::OnExit, MetaGen::Server::Lua::TriggerEventDataOnExit{
            .triggerID = trigger.triggerID,
            .playerID = entt::to_integral(playerEntity)
        });
    }
    void OnStay(World& world, Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(world.zenithKey);

        zenith->CallEvent(MetaGen::Server::Lua::TriggerEvent::OnStay, MetaGen::Server::Lua::TriggerEventDataOnStay{
            .triggerID = trigger.triggerID,
            .playerID = entt::to_integral(playerEntity)
        });
    }
}