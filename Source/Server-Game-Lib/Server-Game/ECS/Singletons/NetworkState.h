#pragma once
#include "Server-Game/Network/MessageRouter.h"

#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/PacketArena.h>

#include <Gameplay/GameDefine.h>

#include <Network/Define.h>
#include <Network/Server.h>

#include <memory>
#include <utility>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>
#include <spake2-ee/crypto_spake.h>

namespace ECS
{
    struct AccountLoginRequest
    {
    public:
        Network::SocketID socketID;
        std::string name;
    };
    struct CharacterListRequest
    {
    public:
        Network::SocketID socketID;
        entt::entity socketEntity;
    };
    struct CharacterLoginRequest
    {
    public:
        Network::SocketID socketID;
        std::string name;
    };

    namespace Singletons
    {
        struct PacketArenaConfig
        {
        public:
            size_t globalMaxReservedBytes = 256ull * 1024ull * 1024ull;
            size_t maxReservedBytes = PacketArena::DEFAULT_MAX_RESERVED_BYTES;
            size_t blockSize = PacketArena::DEFAULT_BLOCK_SIZE;
            size_t warmBlocksPerSizeClass = PacketArena::DEFAULT_WARM_BLOCKS_PER_SIZE_CLASS;
            f32 trimIntervalSeconds = 1.0f;
        };

        struct NetworkState
        {
        public:
            explicit NetworkState(PacketArenaConfig inPacketArenaConfig)
                : packetArenaConfig(std::move(inPacketArenaConfig)), packetArenaBudget(std::make_shared<PacketArenaBudget>(packetArenaConfig.globalMaxReservedBytes)), packetArena(packetArenaBudget, packetArenaConfig.maxReservedBytes, packetArenaConfig.blockSize), packetArenaTrimTimer(packetArenaConfig.trimIntervalSeconds)
            {
            }

            std::unique_ptr<Network::Server> server;
            std::unique_ptr<Network::MessageRouter> messageRouter;
            PacketArenaConfig packetArenaConfig;
            std::shared_ptr<PacketArenaBudget> packetArenaBudget;
            PacketArena packetArena;
            f32 packetArenaTrimTimer = 0.0f;
            f32 telemetryTimer = 0.0f;
            u32 inboundMessagesPerLanePerUpdate = 4096;
            bool connectionInfoCaptureEnabled = false;

            robin_hood::unordered_set<Network::SocketID> socketIDRequestedToClose;
            robin_hood::unordered_set<Network::SocketID> activeSocketIDs;
            robin_hood::unordered_map<Network::SocketID, entt::entity> socketIDToAccountEntity;
            robin_hood::unordered_map<Network::SocketID, entt::entity> socketIDToCharacterEntity;
            robin_hood::unordered_map<Network::SocketID, crypto_spake_shared_keys> socketIDToSessionKeys;

            robin_hood::unordered_map<u64, entt::entity> accountIDToEntity;
            robin_hood::unordered_map<u64, Network::SocketID> accountIDToSocketID;
            robin_hood::unordered_map<u64, entt::entity> characterIDToEntity;
            robin_hood::unordered_map<u64, Network::SocketID> characterIDToSocketID;

            moodycamel::ConcurrentQueue<AccountLoginRequest> accountLoginRequest;
            moodycamel::ConcurrentQueue<CharacterListRequest> characterListRequest;
            moodycamel::ConcurrentQueue<CharacterLoginRequest> characterLoginRequest;
        };
    }
}
