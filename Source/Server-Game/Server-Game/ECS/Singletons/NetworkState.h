#pragma once
#include <Base/Container/ConcurrentQueue.h>

#include <Network/Define.h>

#include <memory>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>

namespace Network
{
    class Server;
    class GameMessageRouter;
}

namespace ECS
{
    struct CharacterLoginRequest
    {
        Network::SocketID socketID;
        u32 nameHash;
    };

    namespace Singletons
    {
        struct NetworkState
        {
        public:
            std::unique_ptr<Network::Server> server;
            std::unique_ptr<Network::GameMessageRouter> gameMessageRouter;

            robin_hood::unordered_set<Network::SocketID> socketIDRequestedToClose;
            robin_hood::unordered_map<Network::SocketID, u64> socketIDToCharacterID;
            robin_hood::unordered_map<Network::SocketID, entt::entity> socketIDToEntity;
            robin_hood::unordered_map<u64, Network::SocketID> characterIDToSocketID;
            robin_hood::unordered_map<entt::entity, Network::SocketID> entityToSocketID;

            moodycamel::ConcurrentQueue<CharacterLoginRequest> characterLoginRequest;
        };
    }
}