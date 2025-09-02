#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct WorldAABB;
        struct ProximityTrigger;
    }

    namespace Singletons
    {
        struct ProximityTriggers;
    }
}

namespace ECS::Util::ProximityTrigger
{
    bool ProximityTriggerExists(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID);
    bool ProximityTriggerGetByID(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID, entt::entity& entity);
   
    bool AddTriggerToWorld(Singletons::ProximityTriggers& proximityTriggers, entt::entity entity, u32 triggerID, const Components::WorldAABB& worldAABB, bool isServerSideOnly);
    bool RemoveTriggerFromWorld(Singletons::ProximityTriggers& proximityTriggers, u32 triggerID, bool isServerSideOnly);
    
    bool IsPlayerInTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity);
    bool AddPlayerToTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity);
    bool RemovePlayerFromTrigger(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity);

    bool IsPlayerInTriggerEntered(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity);
    bool AddPlayerToTriggerEntered(World& world, Components::ProximityTrigger& proximityTrigger, entt::entity triggerEntity, entt::entity playerEntity);
    bool RemovePlayerFromTriggerEntered(Components::ProximityTrigger& proximityTrigger, entt::entity playerEntity);

    void OnEnter(Components::ProximityTrigger& trigger, entt::entity playerEntity);
    void OnStay(Components::ProximityTrigger& trigger, entt::entity playerEntity);
    void OnExit(Components::ProximityTrigger& trigger, entt::entity playerEntity);
}