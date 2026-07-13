#pragma once
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Network/Define.h>
#include <Network/Server.h>

#include <entt/fwd.hpp>

#include <limits>

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
        bool IsAccountLinkedToSocket(::ECS::Singletons::NetworkState& networkState, u64 accountID);
        bool IsAccountLinkedToEntity(::ECS::Singletons::NetworkState& networkState, u64 accountID);
        bool IsCharacterLinkedToSocket(::ECS::Singletons::NetworkState& networkState, u64 characterID);
        bool IsCharacterLinkedToEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID);
        bool IsSocketLinkedToAccountEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
        bool IsSocketLinkedToCharacterEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);

        bool GetSocketIDFromAccountID(::ECS::Singletons::NetworkState& networkState, u64 accountID, ::Network::SocketID& socketID);
        bool GetSocketIDFromCharacterID(::ECS::Singletons::NetworkState& networkState, u64 characterID, ::Network::SocketID& socketID);
        bool GetAccountEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity);
        bool GetAccountEntity(::ECS::Singletons::NetworkState& networkState, u64 accountID, entt::entity& entity);
        bool GetCharacterEntity(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, entt::entity& entity);
        bool GetCharacterEntity(::ECS::Singletons::NetworkState& networkState, u64 characterID, entt::entity& entity);

        bool ActivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
        bool DeactivateSocket(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);
        bool RequestSocketToClose(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID);

        void LinkSocketToAccount(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 accountID, entt::entity accountEntity);
        void LinkSocketToCharacter(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 characterID, entt::entity characterEntity);
        void UnlinkSocketFromAccount(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 accountID);
        void UnlinkSocketFromCharacter(::ECS::Singletons::NetworkState& networkState, ::Network::SocketID socketID, u64 characterID);

        inline ::Network::PacketSendOptions CreateReplicationSendOptions(::Network::OpcodeType opcode, ObjectGUID guid, u32 subKey = 0)
        {
            u64 coalesceKey = (guid.GetData() * 0x9E3779B185EBCA87ull) ^ (static_cast<u64>(opcode) << 32) ^ subKey;
            if (coalesceKey == 0)
                coalesceKey = 1;

            return
            {
                .priority = ::Network::PacketPriority::Replication,
                .coalesceKey = coalesceKey
            };
        }

        bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, PacketRef packet, ::Network::PacketSendOptions options = { });
        bool SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, const PacketRef& packet, ::Network::PacketSendOptions options = { });

        template <::Network::PacketConcept... Packets>
        PacketRef CreatePacketBuffer(PacketArena& arena, Packets&&... packets)
        {
            static_assert(sizeof...(Packets) > 0);

            bool failed = false;
            size_t totalSize = 0;

            auto CalculatePacketSize = [&](const auto& packet)
            {
                const size_t packetSize = packet.GetSerializedSize();
                if (packetSize > std::numeric_limits<u16>::max() || packetSize > std::numeric_limits<size_t>::max() - sizeof(::Network::MessageHeader) || totalSize > std::numeric_limits<size_t>::max() - sizeof(::Network::MessageHeader) - packetSize)
                {
                    failed = true;
                    return;
                }

                totalSize += sizeof(::Network::MessageHeader) + packetSize;
            };

            (CalculatePacketSize(packets), ...);
            if (failed)
                return { };

            PacketWriter writer = arena.Acquire(totalSize);
            if (!writer.IsValid())
                return { };

            Bytebuffer& buffer = writer.GetBuffer();

            auto AppendPacket = [&](auto&& packet)
            {
                using PacketType = std::decay_t<decltype(packet)>;
                const u32 packetSize = packet.GetSerializedSize();

                ::Network::MessageHeader header =
                {
                    .opcode = PacketType::PACKET_ID,
                    .size = static_cast<u16>(packetSize)
                };

                failed |= !buffer.Put(header);
                failed |= !buffer.Serialize(packet);
            };

            (AppendPacket(std::forward<Packets>(packets)), ...);
            if (failed)
                return { };

            return writer.Seal();
        }

        template <::Network::PacketConcept... Packets>
        bool AppendPacketToBuffer(Bytebuffer& buffer, Packets&&... packets)
        {
            bool failed = false;

            auto AppendPacket = [&](auto&& packet)
            {
                using PacketType = std::decay_t<decltype(packet)>;
                const u32 packetSize = packet.GetSerializedSize();
                if (packetSize > std::numeric_limits<u16>::max() || buffer.GetSpace() < sizeof(::Network::MessageHeader) + packetSize)
                {
                    failed = true;
                    return;
                }

                ::Network::MessageHeader header =
                {
                    .opcode = PacketType::PACKET_ID,
                    .size = static_cast<u16>(packetSize)
                };

                failed |= !buffer.Put(header);
                failed |= !buffer.Serialize(packet);
            };

            (AppendPacket(std::forward<Packets>(packets)), ...);
            return !failed;
        }

        template <::Network::PacketConcept... Packets>
        bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, ::Network::PacketSendOptions options, Packets&&... packets)
        {
            if (!IsSocketActive(networkState, socketID))
                return false;

            PacketRef packet = CreatePacketBuffer(networkState.packetArena, std::forward<Packets>(packets)...);
            if (!packet.IsValid())
                return false;

            return networkState.server->SendPacket(socketID, std::move(packet), options);
        }

        template <::Network::PacketConcept... Packets>
        bool SendPacket(Singletons::NetworkState& networkState, ::Network::SocketID socketID, Packets&&... packets)
        {
            return SendPacket(networkState, socketID, ::Network::PacketSendOptions { }, std::forward<Packets>(packets)...);
        }

        template <::Network::PacketConcept... Packets>
        bool SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, ::Network::PacketSendOptions options, Packets&&... packets)
        {
            PacketRef packet = CreatePacketBuffer(world.packetArena, std::forward<Packets>(packets)...);
            if (!packet.IsValid())
                return false;

            return SendToNearby(networkState, world, sender, visibilityInfo, sendToSelf, packet, options);
        }

        template <::Network::PacketConcept... Packets>
        bool SendToNearby(Singletons::NetworkState& networkState, World& world, const entt::entity sender, const Components::VisibilityInfo& visibilityInfo, bool sendToSelf, Packets&&... packets)
        {
            return SendToNearby(networkState, world, sender, visibilityInfo, sendToSelf, ::Network::PacketSendOptions { }, std::forward<Packets>(packets)...);
        }
    }
}
