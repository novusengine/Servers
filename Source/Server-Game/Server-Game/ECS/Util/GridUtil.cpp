#include "GridUtil.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Util::Grid
{
    void SendToNearby(entt::entity entity, std::shared_ptr<Bytebuffer>& buffer, bool sendToSelf)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();
        Singletons::WorldState& worldState = registry->ctx().get<Singletons::WorldState>();

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = registry->get<Components::VisibilityInfo>(entity);
        Components::NetInfo& netInfo = registry->get<Components::NetInfo>(entity);

        u32 mapID = 0; // TODO : Get this from the entity
        World& world = worldState.GetWorld(mapID);

        if (sendToSelf && networkState.activeSocketIDs.contains(netInfo.socketID))
            networkState.server->SendPacket(netInfo.socketID, buffer);

        for (const GameDefine::ObjectGuid& guid : visibilityInfo.visiblePlayers)
        {
            entt::entity otherEntity = world.GetEntity(guid);
            if (otherEntity == entt::null)
                continue;

            Components::NetInfo& otherNetInfo = registry->get<Components::NetInfo>(otherEntity);
            if (!networkState.activeSocketIDs.contains(otherNetInfo.socketID))
                continue;

            auto& targetVisibilityInfo = registry->get<Components::VisibilityInfo>(otherEntity);
            if (!targetVisibilityInfo.visiblePlayers.contains(objectInfo.guid))
                continue;

            networkState.server->SendPacket(otherNetInfo.socketID, buffer);
        }
    }
}