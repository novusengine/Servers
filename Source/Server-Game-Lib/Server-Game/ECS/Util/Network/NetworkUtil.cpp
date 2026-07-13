#include "NetworkUtil.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"

#include <Network/Server.h>

#include <entt/entt.hpp>
#include "NetworkUtil.h"

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

    bool IsAccountLinkedToSocket(::ECS::Singletons::NetworkState& networkState, u64 accountID)
    {
        bool isLinked = networkState.accountIDToSocketID.contains(accountID);
        return isLinked;
    }

    bool IsAccountLinkedToEntity(::ECS::Singletons::NetworkState& networkState, u64 accountID)
    {
        bool isLinked = networkState.accountIDToEntity.contains(accountID);
        return isLinked;
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

    bool IsSocketLinkedToAccountEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        bool isLinked = networkState.socketIDToAccountEntity.contains(socketID);
        return isLinked;
    }

    bool IsSocketLinkedToCharacterEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID)
    {
        bool isLinked = networkState.socketIDToCharacterEntity.contains(socketID);
        return isLinked;
    }

    bool GetSocketIDFromAccountID(::ECS::Singletons::NetworkState& networkState, u64 accountID, ::Network::SocketID& socketID)
    {
        socketID = ::Network::SOCKET_ID_INVALID;

        auto it = networkState.accountIDToSocketID.find(accountID);
        if (it == networkState.accountIDToSocketID.end())
            return false;

        socketID = it->second;
        return true;
    }
    bool GetSocketIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, ::Network::SocketID& socketID)
    {
        socketID = ::Network::SOCKET_ID_INVALID;

        auto it = networkState.characterIDToSocketID.find(characterID);
        if (it == networkState.characterIDToSocketID.end())
            return false;

        socketID = it->second;
        return true;
    }

    bool GetAccountEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity)
    {
        entity = entt::null;

        auto it = networkState.socketIDToAccountEntity.find(socketID);
        if (it == networkState.socketIDToAccountEntity.end())
            return false;

        entity = it->second;
        return true;
    }
    bool GetAccountEntity(::ECS::Singletons::NetworkState& networkState, u64 accountID, entt::entity& entity)
    {
        entity = entt::null;

        auto it = networkState.accountIDToEntity.find(accountID);
        if (it == networkState.accountIDToEntity.end())
            return false;

        entity = it->second;
        return true;
    }

    bool GetCharacterEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity)
    {
        entity = entt::null;

        auto it = networkState.socketIDToCharacterEntity.find(socketID);
        if (it == networkState.socketIDToCharacterEntity.end())
            return false;

        entity = it->second;
        return true;
    }
    bool GetCharacterEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID, entt::entity& entity)
    {
        entity = entt::null;

        auto it = networkState.characterIDToEntity.find(characterID);
        if (it == networkState.characterIDToEntity.end())
            return false;

        entity = it->second;
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

    void LinkSocketToAccount(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 accountID, entt::entity accountEntity)
    {
        networkState.socketIDToAccountEntity[socketID] = accountEntity;
        networkState.accountIDToEntity[accountID] = accountEntity;
        networkState.accountIDToSocketID[accountID] = socketID;
    }
    void LinkSocketToCharacter(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 characterID, entt::entity characterEntity)
    {
        networkState.socketIDToCharacterEntity[socketID] = characterEntity;
        networkState.characterIDToEntity[characterID] = characterEntity;
        networkState.characterIDToSocketID[characterID] = socketID;
    }
    void UnlinkSocketFromAccount(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 accountID)
    {
        networkState.socketIDToAccountEntity.erase(socketID);
        networkState.socketIDToSessionKeys.erase(socketID);
        networkState.accountIDToEntity.erase(accountID);
        networkState.accountIDToSocketID.erase(accountID);
    }
    void UnlinkSocketFromCharacter(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 characterID)
    {
        networkState.socketIDToCharacterEntity.erase(socketID);
        networkState.characterIDToSocketID.erase(characterID);
        networkState.characterIDToEntity.erase(characterID);
    }
    bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, PacketRef packet, ::Network::PacketSendOptions options)
    {
        if (!packet.IsValid() || !IsSocketActive(networkState, socketID))
            return false;

        return networkState.server->SendPacket(socketID, std::move(packet), options);
    }
    bool SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, const PacketRef& packet, ::Network::PacketSendOptions options)
    {
        if (!packet.IsValid())
            return false;

        bool sentToAllRecipients = true;

        auto& objectInfo = world.Get<Components::ObjectInfo>(sender);
        if (sendToSelf)
        {
            if (Components::NetInfo* netInfo = world.TryGet<Components::NetInfo>(sender))
            {
                if (Util::Network::IsSocketActive(networkState, netInfo->socketID))
                    sentToAllRecipients &= networkState.server->SendPacket(netInfo->socketID, packet, options);
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

            sentToAllRecipients &= networkState.server->SendPacket(otherNetInfo.socketID, packet, options);
        }

        return sentToAllRecipients;
    }
}
