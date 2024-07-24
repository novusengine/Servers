#pragma once
#include <Base/Container/ConcurrentQueue.h>

#include <Network/Define.h>

#include <memory>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>

namespace Network
{
	class Server;
	class PacketHandler;
}

namespace ECS::Singletons
{
	struct NetworkState
	{
	public:
		std::unique_ptr<Network::Server> server;
		std::unique_ptr<Network::PacketHandler> packetHandler;

		robin_hood::unordered_set<Network::SocketID> socketIDRequestedToClose;
		robin_hood::unordered_map<Network::SocketID, u32> socketIDToCharacterID;
		robin_hood::unordered_map<Network::SocketID, entt::entity> socketIDToEntity;
		robin_hood::unordered_map<u32, Network::SocketID> characterIDToSocketID;
		robin_hood::unordered_map<entt::entity, Network::SocketID> EntityToSocketID;
	};
}