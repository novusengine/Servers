#include "NetworkConnection.h"

#include "Gameserver/Application/EnttRegistries.h"
#include "Gameserver/ECS/Singletons/DatabaseState.h"
#include "Gameserver/ECS/Singletons/NetworkState.h"
#include "Gameserver/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>

#include <Network/Packet.h>
#include <Network/PacketHandler.h>
#include <Network/Server.h>

#include <Server/Database/DBController.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
	bool HandleOnConnected(Network::SocketID socketID, std::shared_ptr<Network::Packet> inPacket)
	{
		std::string charName = "";

		if (!inPacket->payload->GetString(charName))
			return false;

		entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
		Singletons::NetworkState& networkState = ctx.emplace<Singletons::NetworkState>();
		Singletons::DatabaseState& databaseState = ctx.emplace<Singletons::DatabaseState>();

		u32 characterID = std::numeric_limits<u32>().max();
		// Check if the character exists
		{
			auto itr = databaseState.characterNameToDefinition.find(charName);
			if (itr == databaseState.characterNameToDefinition.end())
			{
				// Send back that the character does not exist
				return false;
			}

			characterID = itr->second.id;
		}

		// Check if someone else is already connected to this character
		{
			auto itr = networkState.characterIDToSocketID.find(characterID);
			if (itr != networkState.characterIDToSocketID.end())
			{
				// Send back that the character is already in use or kick current user out
				return false;
			}
		}

		// Check if socketID is already connected to a character
		{
			auto itr = networkState.socketIDToCharacterID.find(socketID);
			if (itr != networkState.socketIDToCharacterID.end())
			{
				// Unload Character
				return false;
			}
		}

		// Setup network state for the character, and send back confirmation
		networkState.socketIDToCharacterID[socketID] = characterID;
		networkState.characterIDToSocketID[characterID] = socketID;

		std::shared_ptr<Network::Packet> packet = Network::Packet::Borrow();
		{
			packet->payload = Bytebuffer::Borrow<128>();
			packet->payload->Put(Network::Opcode::SMSG_CONNECTED);
			packet->payload->PutU16(sizeof(u8) + static_cast<u8>(charName.size()) + 1);
			packet->payload->PutU8(0);
			packet->payload->PutString(charName);

			packet->header = *reinterpret_cast<Network::Packet::Header*>(packet->payload->GetDataPointer());
		}

		Network::SocketMessageEvent messageEvent;
		{
			messageEvent.socketID = socketID;
			messageEvent.packet = std::move(packet);
		}

		networkState.server->SendPacket(messageEvent);

		return true;
	}

	void NetworkConnection::Init(entt::registry& registry)
	{
        entt::registry::context& ctx = registry.ctx();

        Singletons::NetworkState& networkState = ctx.emplace<Singletons::NetworkState>();

		// Setup NetServer
		{
			networkState.server = std::make_unique<Network::Server>();
			networkState.packetHandler = std::make_unique<Network::PacketHandler>();

			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CONNECTED, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 3u, 11u, &HandleOnConnected));

			// Bind to IP/Port
			std::string ipAddress = "127.0.0.1";
			u16 port = 4000;

			Network::Socket::Result initResult = networkState.server->Init(Network::Socket::Mode::TCP, ipAddress, port);
			if (initResult == Network::Socket::Result::SUCCESS)
			{
				DebugHandler::Print("Network : Listening on ({0}, {1})", ipAddress, port);
			}
			else
			{
				DebugHandler::PrintFatal("Network : Failed to bind on ({0}, {1})", ipAddress, port);
			}
		}
	}

	void NetworkConnection::Update(entt::registry& registry, f32 deltaTime)
	{
		entt::registry::context& ctx = registry.ctx();

		Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

		// Handle 'SocketConnectedEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketConnectedEvent>& connectedEvents = networkState.server->GetConnectedEvents();

			Network::SocketConnectedEvent connectedEvent;
			while (connectedEvents.try_dequeue(connectedEvent))
			{
				const Network::ConnectionInfo& connectionInfo = connectedEvent.connectionInfo;
				DebugHandler::Print("Network : Client connected from (SocketID : {0}, \"{1}:{2}\")", static_cast<u32>(connectedEvent.socketID), connectionInfo.ipAddrStr, connectionInfo.port);
			}
		}

		// Handle 'SocketDisconnectedEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketDisconnectedEvent>& disconnectedEvents = networkState.server->GetDisconnectedEvents();

			Network::SocketDisconnectedEvent disconnectedEvent;
			while (disconnectedEvents.try_dequeue(disconnectedEvent))
			{
				DebugHandler::Print("Network : Client disconnected (SocketID : {0})", static_cast<u32>(disconnectedEvent.socketID));

				networkState.socketIDRequestedToClose.erase(disconnectedEvent.socketID);

				// Start Cleanup here
				auto itr = networkState.socketIDToCharacterID.find(disconnectedEvent.socketID);
				if (itr != networkState.socketIDToCharacterID.end())
				{
					u32 characterID = itr->second;

					networkState.socketIDToCharacterID.erase(itr);
					networkState.characterIDToSocketID.erase(characterID);
				}
			}
		}

		// Handle 'SocketMessageEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketMessageEvent>& messageEvents = networkState.server->GetMessageEvents();

			Network::SocketMessageEvent messageEvent;
			while (messageEvents.try_dequeue(messageEvent))
			{
				DebugHandler::Print("Network : Message from (SocketID : {0}, Opcode : {1}, Size : {2})", static_cast<std::underlying_type<Network::SocketID>::type>(messageEvent.socketID), static_cast<std::underlying_type<Network::Opcode>::type>(messageEvent.packet->header.opcode), messageEvent.packet->header.size);
			
				// Invoke MessageHandler here
				if (networkState.packetHandler->CallHandler(messageEvent.socketID, messageEvent.packet))
					continue;

				// Failed to Call Handler, Close Socket
				{
					auto itr = networkState.socketIDRequestedToClose.find(messageEvent.socketID);

					if (itr == networkState.socketIDRequestedToClose.end())
					{
						networkState.server->CloseSocketID(messageEvent.socketID);
						networkState.socketIDRequestedToClose.emplace(messageEvent.socketID);
					}
				}
			}
		}
	}
}