#include "ProximityTrigger.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/ProximityTriggers.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CollisionUtil.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>
#include <tracy/Tracy.hpp>

namespace ECS::Systems
{
    void ProximityTrigger::Init(entt::registry& registry)
    {
        auto& ctx = registry.ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& proximityTriggers = ctx.emplace<Singletons::ProximityTriggers>();

        for(const auto& trigger : gameCache.proximityTriggerTables.triggers)
        {
            entt::entity triggerEntity = registry.create();

            auto& transform = registry.emplace<Components::Transform>(triggerEntity);
            transform.mapID = trigger.mapID;
            transform.position = trigger.position;

            registry.emplace<Components::AABB>(triggerEntity, vec3(0, 0, 0), trigger.extents);
            auto& proximityTrigger = registry.emplace<Components::ProximityTrigger>(triggerEntity);
            proximityTrigger.triggerID = trigger.id;
            proximityTrigger.name = trigger.name;
            proximityTrigger.flags = trigger.flags;
            registry.emplace<Tags::ProximityTriggerNeedsInitialization>(triggerEntity);

            gameCache.proximityTriggerTables.triggerIDToEntity[trigger.id] = triggerEntity;
        }
    }

    void OnEnter(Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        NC_LOG_INFO("Trigger '{}' entered!", trigger.name);
    }

    void OnExit(Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        NC_LOG_INFO("Trigger '{}' exited!", trigger.name);
    }

    void OnStay(Components::ProximityTrigger& trigger, entt::entity playerEntity)
    {
        NC_LOG_INFO("Trigger '{}' stayed!", trigger.name);
    }

    void ProximityTrigger::Update(entt::registry& registry, f32 deltaTime)
    {
        ZoneScopedN("ECS::ProximityTriggers");

        auto& ctx = registry.ctx();
        
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& proximityTriggers = ctx.get<Singletons::ProximityTriggers>();
        auto& worldState = ctx.get<Singletons::WorldState>();

        // Initialize any new triggers that need to be created
        auto proximityTriggerNeedsinitializationView = registry.view<Components::Transform, Components::AABB, Components::ProximityTrigger, Tags::ProximityTriggerNeedsInitialization>();
        proximityTriggerNeedsinitializationView.each([&registry, &worldState, &networkState, &proximityTriggers](entt::entity entity, Components::Transform& transform, Components::AABB& aabb, Components::ProximityTrigger& trigger)
        {
            World& world = worldState.GetWorld(transform.mapID);

            bool isServerSideOnly = (trigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) != Generated::ProximityTriggerFlagEnum::None;

            if (isServerSideOnly)
            {
                registry.emplace<Tags::ProximityTriggerIsServerSideOnly>(entity);

                // Calculate world AABB
                Components::WorldAABB worldAABB;
                worldAABB.min = transform.position + aabb.centerPos - aabb.extents;
                worldAABB.max = transform.position + aabb.centerPos + aabb.extents;

                // Add server side only triggers to the activeTriggers R-tree
                proximityTriggers.activeTriggersVisTree.Insert(reinterpret_cast<f32*>(&worldAABB.min), reinterpret_cast<f32*>(&worldAABB.max), trigger.triggerID);
                proximityTriggers.activeTriggers.insert(trigger.triggerID);
            }
            else
            {
                registry.emplace<Tags::ProximityTriggerIsClientSide>(entity);

                // Replicate to the clients if this is a client side trigger
                std::shared_ptr<Bytebuffer> triggerCreateMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::ProximityTrigger::BuildProximityTriggerCreate(triggerCreateMessage, trigger.triggerID, trigger.name, trigger.flags, transform.mapID, transform.position, aabb.extents))
                    return;

                world.GetPlayersInRadius(transform.position.x, transform.position.z, 100000.0f, [&](const GameDefine::ObjectGuid& guid) -> bool
                {
                    entt::entity playerEntity = world.GetEntity(guid);

                    Network::SocketID socketID;
                    if (!Util::Network::GetSocketIDFromCharacterID(networkState, guid.GetCounter(), socketID))
                        return true;

                    networkState.server->SendPacket(socketID, triggerCreateMessage);
                    return true;
                });
            }
        });
        registry.clear<Tags::ProximityTriggerNeedsInitialization>();

        // Destroy any triggers that need to be removed
        for(auto triggerID : proximityTriggers.triggerIDsToDestroy)
        {
            if (!gameCache.proximityTriggerTables.triggerIDToEntity.contains(triggerID))
                continue;

            entt::entity triggerEntity = gameCache.proximityTriggerTables.triggerIDToEntity[triggerID];

            auto& transform = registry.get<Components::Transform>(triggerEntity);
            World& world = worldState.GetWorld(transform.mapID);

            std::shared_ptr<Bytebuffer> triggerDestroyMessage = Bytebuffer::Borrow<32>();
            if (!Util::MessageBuilder::ProximityTrigger::BuildProximityTriggerDelete(triggerDestroyMessage, triggerID))
                continue;

            gameCache.proximityTriggerTables.triggerIDToEntity.erase(triggerID);
            proximityTriggers.activeTriggers.erase(triggerID);

            auto& proximityTrigger = registry.get<Components::ProximityTrigger>(triggerEntity);
            bool isServerSideOnly = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) == Generated::ProximityTriggerFlagEnum::IsServerSideOnly;

            world.GetPlayersInRadius(transform.position.x, transform.position.z, 100000.0f, [&](const GameDefine::ObjectGuid& guid) -> bool
            {
                entt::entity playerEntity = world.GetEntity(guid);

                if (isServerSideOnly && proximityTrigger.playersInside.contains(playerEntity))
                {
                    OnExit(proximityTrigger, playerEntity);
                }

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, guid.GetCounter(), socketID))
                    return true;

                networkState.server->SendPacket(socketID, triggerDestroyMessage);
                return true;
            });

            registry.destroy(triggerEntity);
        }
        proximityTriggers.triggerIDsToDestroy.clear();

        // Call client side trigger events
        // For these we only care about OnEnter
        auto clientSideTriggerView = registry.view<Components::Transform, Components::AABB, Components::ProximityTrigger, Tags::ProximityTriggerIsClientSide>();
        clientSideTriggerView.each([&registry, &worldState](entt::entity triggerEntity, Components::Transform& triggerTransform, Components::AABB& triggerAABB, Components::ProximityTrigger& trigger)
        {
            // Loop over players that just entered the trigger and call OnEnter
            for(auto playerEntity : trigger.playersEntered)
            {
                if (!registry.valid(playerEntity))
                    continue;
                OnEnter(trigger, playerEntity);
                trigger.playersInside.insert(playerEntity);
            }
            trigger.playersEntered.clear();
        });

        // Call server side trigger events
        robin_hood::unordered_set<entt::entity> playersToRemove;
        playersToRemove.reserve(1024);

        for (u32 triggerID : proximityTriggers.activeTriggers)
        {
            entt::entity triggerEntity = gameCache.proximityTriggerTables.triggerIDToEntity[triggerID];
            if (!registry.valid(triggerEntity))
                continue;

            auto& triggerTransform = registry.get<Components::Transform>(triggerEntity);
            auto& triggerAABB = registry.get<Components::AABB>(triggerEntity);
            auto& proximityTrigger = registry.get<Components::ProximityTrigger>(triggerEntity);

            World& world = worldState.GetWorld(triggerTransform.mapID);

            vec4 minMaxRect = vec4(
                triggerTransform.position.x + triggerAABB.centerPos.x - triggerAABB.extents.x,
                triggerTransform.position.z + triggerAABB.centerPos.z - triggerAABB.extents.z,
                triggerTransform.position.x + triggerAABB.centerPos.x + triggerAABB.extents.x,
                triggerTransform.position.z + triggerAABB.centerPos.z + triggerAABB.extents.z
            );

            playersToRemove = proximityTrigger.playersInside;

            world.GetPlayersInRect(minMaxRect, [&](const GameDefine::ObjectGuid& guid) -> bool
            {
                entt::entity playerEntity = world.GetEntity(guid);

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, guid.GetCounter(), socketID))
                    return true;

                if (proximityTrigger.playersInside.contains(playerEntity))
                {
                    OnStay(proximityTrigger, playerEntity);
                    playersToRemove.erase(playerEntity);
                }
                else
                {
                    OnEnter(proximityTrigger, playerEntity);
                    proximityTrigger.playersInside.insert(playerEntity);
                }

                return true;
            });

            // Loop over players that have exited the trigger and call OnExit
            for (auto playerEntity : playersToRemove)
            {
                if (!registry.valid(playerEntity))
                    continue;

                OnExit(proximityTrigger, playerEntity);
                proximityTrigger.playersInside.erase(playerEntity);
            }
        }
    }
}