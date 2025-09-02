#include "NetworkUtil.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Util::Network
{
    bool IsSocketActive(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        bool isActive = networkState.activeSocketIDs.contains(socketID);
        return isActive;
    }

    bool IsSocketRequestedToClose(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        bool isRequestedToClose = networkState.socketIDRequestedToClose.contains(socketID);
        return isRequestedToClose;
    }

    bool IsCharacterLinkedToSocket(::ECS::Singletons::NetworkState& networkState, u64 characterID)
    {
        bool isLinked = networkState.characterIDToSocketID.contains(characterID);
        return isLinked;
    }

    bool IsCharacterLinkedToEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID)
    {
        bool isLinked = networkState.characterIDToEntity.contains(characterID);
        return isLinked;
    }

    bool IsSocketLinkedToEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        bool isLinked = networkState.socketIDToEntity.contains(socketID);
        return isLinked;
    }

    bool GetSocketIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, ::Network::SocketID& socketID)
    {
        socketID = ::Network::SOCKET_ID_INVALID;

        if (!IsCharacterLinkedToSocket(networkState, characterID))
            return false;

        socketID = networkState.characterIDToSocketID[characterID];
        return true;
    }
    bool GetEntityIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, entt::entity& entity)
    {
        entity = entt::null;

        if (!IsCharacterLinkedToEntity(networkState, characterID))
            return false;

        entity = networkState.characterIDToEntity[characterID];
        return true;
    }
    bool GetEntityIDFromSocketID(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity)
    {
        entity = entt::null;

        if (!IsSocketLinkedToEntity(networkState, socketID))
            return false;

        entity = networkState.socketIDToEntity[socketID];
        return true;
    }

    bool ActivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        if (IsSocketActive(networkState, socketID))
            return false;

        networkState.activeSocketIDs.insert(socketID);
        return true;
    }
    bool DeactivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        if (!IsSocketActive(networkState, socketID))
            return false;

        networkState.activeSocketIDs.erase(socketID);
        networkState.socketIDRequestedToClose.erase(socketID);

        return true;
    }
    bool RequestSocketToClose(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        if (!IsSocketActive(networkState, socketID))
            return false;

        if (IsSocketRequestedToClose(networkState, socketID))
            return false;

        networkState.socketIDRequestedToClose.insert(socketID);
        networkState.server->CloseSocketID(socketID);
        return true;
    }
    bool UnlinkSocketFromEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        if (!IsSocketLinkedToEntity(networkState, socketID))
            return false;

        networkState.socketIDToEntity.erase(socketID);
        return true;
    }
    bool UnlinkCharacterFromSocket(::ECS::Singletons::NetworkState& networkState, u64 characterID)
    {
        if (!IsCharacterLinkedToSocket(networkState, characterID))
            return false;

        networkState.characterIDToSocketID.erase(characterID);
        return true;
    }
    bool UnlinkCharacterFromEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID)
    {
        if (!IsCharacterLinkedToEntity(networkState, characterID))
            return false;

        networkState.characterIDToEntity.erase(characterID);
        return true;
    }
    bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, std::shared_ptr<Bytebuffer>& buffer)
    {
        if (!IsSocketActive(networkState, socketID))
            return false;

        networkState.server->SendPacket(socketID, buffer);
        return true;
    }
    void SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, std::shared_ptr<Bytebuffer>& buffer)
    {
        auto& objectInfo = world.Get<Components::ObjectInfo>(sender);
        if (sendToSelf)
        {
            if (Components::NetInfo* netInfo = world.TryGet<Components::NetInfo>(sender))
            {
                if (Util::Network::IsSocketActive(networkState, netInfo->socketID))
                    networkState.server->SendPacket(netInfo->socketID, buffer);
            }
        }

        for (const ObjectGUID& guid : visibilityInfo.visiblePlayers)
        {
            entt::entity otherEntity = world.GetEntity(guid);
            if (!world.ValidEntity(otherEntity))
                continue;

            Components::NetInfo& otherNetInfo = world.Get<Components::NetInfo>(otherEntity);
            if (!Util::Network::IsSocketActive(networkState, otherNetInfo.socketID))
                continue;

            auto& targetVisibilityInfo = world.Get<Components::VisibilityInfo>(otherEntity);

            if (objectInfo.guid.GetType() == ObjectGUID::Type::Player)
            {
                if (!targetVisibilityInfo.visiblePlayers.contains(objectInfo.guid))
                    continue;
            }
            else
            {
                if (!targetVisibilityInfo.visibleCreatures.contains(objectInfo.guid))
                    continue;
            }

            networkState.server->SendPacket(otherNetInfo.socketID, buffer);
        }
    }
}