#pragma once
#include <Base/Types.h>

#include <Network/Define.h>

#include <entt/fwd.hpp>

namespace ECS::Singletons
{
    struct NetworkState;
}

namespace ECS::Util::Network
{
    bool IsSocketActive(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
    bool IsSocketRequestedToClose(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
    bool IsCharacterLinkedToSocket(::ECS::Singletons::NetworkState& networkState, u64 characterID);
    bool IsCharacterLinkedToEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID);
    bool IsSocketLinkedToEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);

    bool GetSocketIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, ::Network::SocketID& socketID);
    bool GetEntityIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, entt::entity& entity);
    bool GetEntityIDFromSocketID(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity);

    bool ActivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
    bool DeactivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
    bool RequestSocketToClose(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);

    bool UnlinkSocketFromEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
    bool UnlinkCharacterFromSocket(::ECS::Singletons::NetworkState& networkState, u64 characterID);
    bool UnlinkCharacterFromEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID);
}