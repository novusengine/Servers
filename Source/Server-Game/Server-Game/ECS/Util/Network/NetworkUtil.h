#pragma once
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Network/Define.h>
#include <Network/Server.h>

#include <entt/fwd.hpp>

namespace ECS
{
    namespace Components
    {
        struct VisibilityInfo;
    }

    namespace Util::Network
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

        bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, std::shared_ptr<Bytebuffer>& buffer);
        void SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, std::shared_ptr<Bytebuffer>& buffer);

        template <::Network::PacketConcept... Packets>
        std::shared_ptr<Bytebuffer> CreatePacketBuffer(Packets&&... packets)
        {
            u32 totalSize = ((sizeof(::Network::MessageHeader) + packets.GetSerializedSize()) + ...);
            std::shared_ptr<Bytebuffer> buffer = Bytebuffer::BorrowRuntime(totalSize);

            auto AppendPacket = [&](auto&& packet)
            {
                using PacketType = std::decay_t<decltype(packet)>;

                ::Network::MessageHeader header =
                {
                    .opcode = PacketType::PACKET_ID,
                    .size = packet.GetSerializedSize()
                };

                buffer->Put(header);
                buffer->Serialize(packet);
            };

            (AppendPacket(std::forward<Packets>(packets)), ...);
            return buffer;
        }

        template <::Network::PacketConcept... Packets>
        bool AppendPacketToBuffer(std::shared_ptr<Bytebuffer>& buffer, Packets&&... packets)
        {
            bool failed = false;
            u32 totalSize = ((sizeof(::Network::MessageHeader) + std::decay_t<Packets>().GetSerializedSize()) + ...);

            auto AppendPacket = [&](auto&& packet)
            {
                using PacketType = std::decay_t<decltype(packet)>;

                ::Network::MessageHeader header =
                {
                    .opcode = PacketType::PACKET_ID,
                    .size = packet.GetSerializedSize()
                };

                failed |= !buffer->Put(header);
                failed |= !buffer->Serialize(packet);
            };

            (AppendPacket(std::forward<Packets>(packets)), ...);
            return !failed;
        }

        template <::Network::PacketConcept... Packets>
        bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, Packets&&... packets)
        {
            if (!IsSocketActive(networkState, socketID))
                return false;

            std::shared_ptr<Bytebuffer> buffer = CreatePacketBuffer(std::forward<Packets>(packets)...);
            networkState.server->SendPacket(socketID, buffer);
            return true;
        }

        template <::Network::PacketConcept... Packets>
        void SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, Packets&&... packets)
        {
            std::shared_ptr<Bytebuffer> buffer = CreatePacketBuffer(std::forward<Packets>(packets)...);
            SendToNearby(networkState, world, sender, visibilityInfo, sendToSelf, buffer);
        }
    }
}