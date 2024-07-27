#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GridSingleton.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Network/PacketHandler.h>
#include <Network/Server.h>

#include <Server-Common/Database/DBController.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
	bool HandleOnConnected(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		std::string charName = "";

		if (!recvPacket->GetString(charName))
			return false;

		entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
		Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
		Singletons::GridSingleton& gridSingleton = ctx.get<Singletons::GridSingleton>();
		Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

		u32 characterID = std::numeric_limits<u32>().max();

		struct Result
		{
			u8 CharacterDoesNotExist : 1 = 0;
			u8 AlreadyConnectedToACharacter : 1 = 0;
			u8 CharacterInUse : 1 = 0;
			u8 Unused : 5 = 0;
		};

		Result result;

		// Check if the character exists
		{
			if (!databaseState.characterNameToDefinition.contains(charName))
			{
				// Send back that the character does not exist
				result.CharacterDoesNotExist = 1;
			}
			else
			{
				characterID = databaseState.characterNameToDefinition[charName].id;
			}
		}

		// Check if socketID is already connected to a character
		if (!result.CharacterDoesNotExist)
		{
			if (networkState.socketIDToCharacterID.contains(socketID))
			{
				// Send back that the socketID is already connected to a character
                result.AlreadyConnectedToACharacter = 1;
			}
		}

		// Check if someone else is already connected to this character
		if (!result.AlreadyConnectedToACharacter)
		{
			if (networkState.characterIDToSocketID.contains(characterID))
			{
				// Send back that the character is already in use or kick current user out
				result.CharacterInUse = 1;
			}
		}

		u8 resultAsValue = *reinterpret_cast<u8*>(&result);
		if (result.CharacterDoesNotExist || result.AlreadyConnectedToACharacter || result.CharacterInUse)
		{
			std::shared_ptr<Bytebuffer> connectionMessage = nullptr;
			if (!Util::MessageBuilder::Authentication::BuildConnectedMessage(socketID, connectionMessage, resultAsValue, entt::null))
				return false;

			networkState.server->SendPacket(socketID, connectionMessage);
			return true;
		}

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		entt::entity entity = registry->create();

		// Setup network state for the character, and send back confirmation
		networkState.socketIDToCharacterID[socketID] = characterID;
		networkState.characterIDToSocketID[characterID] = socketID;

		networkState.socketIDToEntity[socketID] = entity;
		networkState.EntityToSocketID[entity] = socketID;

		// Add To Grid
		{
			gridSingleton.cell.mutex.lock();
			gridSingleton.cell.players.entering.insert(entity);
			gridSingleton.cell.mutex.unlock();
		}

		{
			std::shared_ptr<Bytebuffer> connectionMessage = nullptr;
			if (!Util::MessageBuilder::Authentication::BuildConnectedMessage(socketID, connectionMessage, resultAsValue, entity, &charName))
				return false;

			std::shared_ptr<Bytebuffer> unitStatsMessage = nullptr;
			Components::UnitStatsComponent& unitStatsComponent = UnitUtils::AddStatsComponent(*registry, entity);
			if (!Util::MessageBuilder::Entity::BuildUnitStatsMessage(unitStatsMessage, entity, unitStatsComponent))
                return false;

			Components::TargetInfo& targetInfo = registry->emplace<Components::TargetInfo>(entity);
			targetInfo.target = entt::null;

			registry->emplace<Components::Transform>(entity);
			auto& displayInfo = registry->emplace<Components::DisplayInfo>(entity);
			displayInfo.displayID = 50;

			networkState.server->SendPacket(socketID, connectionMessage);
			ECS::Util::Grid::SendToGrid(entity, unitStatsMessage, ECS::Singletons::GridUpdateFlag{ .SendToSelf = true });
		}

		return true;
	}
	bool HandleOnEntityMove(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		vec3 position = vec3(0.0f);
		quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
		Components::MovementFlags movementFlags = {};
		f32 verticalVelocity = 0.0f;

		if (!recvPacket->Get(position))
			return false;

		if (!recvPacket->Get(rotation))
			return false;

		if (!recvPacket->Get(movementFlags))
			return false;

		if (!recvPacket->Get(verticalVelocity))
			return false;

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		Components::Transform& transform = registry->get<Components::Transform>(socketEntity);

		transform.position = position;
		transform.rotation = rotation;

		std::shared_ptr<Bytebuffer> entityMoveMessage = nullptr;
		if (!Util::MessageBuilder::Entity::BuildEntityMoveMessage(entityMoveMessage, socketEntity, position, rotation, movementFlags, verticalVelocity))
			return false;

		ECS::Util::Grid::SendToGrid(socketEntity, entityMoveMessage);
		return true;
	}

	bool HandleOnEntityTargetUpdate(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		entt::entity targetEntity = entt::null;

		if (!recvPacket->Get(targetEntity))
            return false;

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];

		Components::TargetInfo& targetInfo = registry->get<Components::TargetInfo>(socketEntity);
		targetInfo.target = targetEntity;

		std::shared_ptr<Bytebuffer> targetUpdateMessage = nullptr;
		if (!Util::MessageBuilder::Entity::BuildEntityTargetUpdateMessage(targetUpdateMessage, socketEntity, targetEntity))
            return false;

		ECS::Util::Grid::SendToGrid(socketEntity, targetUpdateMessage);

		return true;
	}

	bool HandleOnRequestSpellCast(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		u32 spellID = 0;

		if (!recvPacket->GetU32(spellID))
            return false;

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		if (!registry->valid(socketEntity))
			return false;

		u8 result = 0;
		Components::TargetInfo* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity);

		if (registry->all_of<Components::CastInfo>(socketEntity))
		{
			result = 0x1;
		}
		else if (!targetInfo || !registry->valid(targetInfo->target))
		{
			result = 0x2;
		}

		if (result != 0x0)
		{
			std::shared_ptr<Bytebuffer> castResultMessage = nullptr;
			if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, 0.0f, 0.0f))
				return false;

			networkState.server->SendPacket(socketID, castResultMessage);
			return true;
		}

		Components::CastInfo& castInfo = registry->emplace<Components::CastInfo>(socketEntity);

		castInfo.target = targetInfo->target;
		castInfo.castTime = 1.5f;
		castInfo.duration = 0.0f;

		std::shared_ptr<Bytebuffer> castResultMessage = nullptr;
		if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, castInfo.castTime, castInfo.duration))
			return false;

		networkState.server->SendPacket(socketID, castResultMessage);

		// Send Grid Message
		{
			std::shared_ptr<Bytebuffer> entityCastSpellMessage = nullptr;
			if (!Util::MessageBuilder::Entity::BuildEntitySpellCastMessage(entityCastSpellMessage, socketEntity, castInfo.castTime, castInfo.duration))
                return false;

			ECS::Util::Grid::SendToGrid(socketEntity, entityCastSpellMessage);
		}

		return true;
	}

	bool HandleOnCheatDamage(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		f32 damage = 0.0f;

		if (!recvPacket->GetF32(damage))
            return false;

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		if (!registry->valid(socketEntity))
			return false;

		entt::entity targetEntity = socketEntity;
		if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
		{
			if (registry->valid(targetInfo->target))
			{
                targetEntity = targetInfo->target;
            }
		}

		if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
		{
			if (unitStatsComponent->currentHealth <= 0)
				return true;

			f32 damageToDeal = glm::min(damage, unitStatsComponent->currentHealth);

			unitStatsComponent->currentHealth -= damageToDeal;
			unitStatsComponent->healthIsDirty = true;

			// Send Grid Message
			{
				std::shared_ptr<Bytebuffer> damageDealtMessage = nullptr;
				if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
					return false;

				ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
			}
		}

		return true;
	}

	bool HandleOnCheatKill(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		if (!registry->valid(socketEntity))
			return false;

		entt::entity targetEntity = socketEntity;
		if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
		{
			if (registry->valid(targetInfo->target))
			{
				targetEntity = targetInfo->target;
			}
		}

		if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
		{
			if (unitStatsComponent->currentHealth <= 0.0f)
				return true;

			f32 damageToDeal = unitStatsComponent->currentHealth;

			unitStatsComponent->currentHealth = 0.0f;
			unitStatsComponent->healthIsDirty = true;

			// Send Grid Message
			{
				std::shared_ptr<Bytebuffer> damageDealtMessage = nullptr;
				if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
					return false;

				ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
			}
		}

		return true;
	}

	bool HandleOnCheatResurrect(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		if (!registry->valid(socketEntity))
			return false;

		entt::entity targetEntity = socketEntity;
		if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
		{
			if (registry->valid(targetInfo->target))
			{
				targetEntity = targetInfo->target;
			}
		}

		if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
		{
			unitStatsComponent->currentHealth = unitStatsComponent->maxHealth;
			unitStatsComponent->healthIsDirty = true;

			// Send Grid Message
			{
				std::shared_ptr<Bytebuffer> ressurectedMessage = nullptr;
				if (!Util::MessageBuilder::CombatLog::BuildRessurectedMessage(ressurectedMessage, socketEntity, targetEntity))
					return false;

				ECS::Util::Grid::SendToGrid(socketEntity, ressurectedMessage, { .SendToSelf = true });
			}
		}

		return true;
	}

	bool HandleOnCheatMorph(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		u32 displayID = 0;

		if (!recvPacket->GetU32(displayID))
            return false;

		entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
		Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

		if (!networkState.socketIDToEntity.contains(socketID))
			return false;

		entt::entity socketEntity = networkState.socketIDToEntity[socketID];
		if (!registry->valid(socketEntity))
			return false;

		auto& displayInfo = registry->get<Components::DisplayInfo>(socketEntity);
		displayInfo.displayID = displayID;

		std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = nullptr;
		if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, socketEntity, displayID))
			return false;

		ECS::Util::Grid::SendToGrid(socketEntity, displayInfoUpdateMessage, { .SendToSelf = 1 });

		return true;
	}

	bool HandleOnCheatCreateCharacter(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		std::string charName = "";
		u16 permissionlevel = 0;

		if (!recvPacket->GetString(charName))
			return false;

		entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
		Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
		Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

		if (!networkState.socketIDToCharacterID.contains(socketID))
			return false;

		u32 characterID = networkState.socketIDToCharacterID[socketID];
		if (!databaseState.characterIDToDefinition.contains(characterID))
			return false;

		auto& socketCharacterDefinition = databaseState.characterIDToDefinition[characterID];
		if (socketCharacterDefinition.permissionLevel <= 3)
			return false;

		struct Result
		{
			u8 NameIsInvalid : 1 = 0;
			u8 CharacterAlreadyExists : 1 = 0;
			u8 DatabaseTransactionFailed : 1 = 0;
			u8 Unused : 5 = 0;
		};

		Result result;

		if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
		{
			result.NameIsInvalid = 1;
		}
		else if (databaseState.characterNameToDefinition.contains(charName))
		{
			result.CharacterAlreadyExists = 1;
		}

		u8 resultAsValue = *reinterpret_cast<u8*>(&result);
		if (resultAsValue != 0)
		{
			std::shared_ptr<Bytebuffer> characterCreateResultMessage = nullptr;
			if (!Util::MessageBuilder::Cheat::BuildCharacterCreateResultMessage(characterCreateResultMessage, charName, resultAsValue))
				return false;

			networkState.server->SendPacket(socketID, characterCreateResultMessage);
			return true;
		}

		// Create Character
		{
			Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();

			if (auto conn = databaseState.controller->GetConnection(Server::Database::DBType::Character))
			{
				auto context = conn->Context();
				auto queryResult = context.exec_prepared("CreateCharacter", charName, permissionlevel);
				if (queryResult.empty())
                {
                    result.DatabaseTransactionFailed = true;
					context.abort();
                }
                else
                {
                    u32 characterID = queryResult[0][0].as<u32>();
                    Server::Database::CharacterDefinition characterDefinition = { .id = characterID, .name = charName, .permissionLevel = permissionlevel };
					
                    databaseState.characterIDToDefinition[characterID] = characterDefinition;
                    databaseState.characterNameToDefinition[charName] = characterDefinition;

					context.commit();
                }
			}
			else
			{
				result.DatabaseTransactionFailed = true;
			}
        }

		// Send Packet
		std::shared_ptr<Bytebuffer> characterCreateResultMessage = nullptr;
		if (!Util::MessageBuilder::Cheat::BuildCharacterCreateResultMessage(characterCreateResultMessage, charName, resultAsValue))
			return false;

		networkState.server->SendPacket(socketID, characterCreateResultMessage);
		return true;
	}

	bool HandleOnCheatDeleteCharacter(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& recvPacket)
	{
		std::string charName = "";

		if (!recvPacket->GetString(charName))
			return false;

		entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
		Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
		Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

		if (!networkState.socketIDToCharacterID.contains(socketID))
			return false;

		u32 characterID = networkState.socketIDToCharacterID[socketID];
		if (!databaseState.characterIDToDefinition.contains(characterID))
			return false;

		auto& socketCharacterDefinition = databaseState.characterIDToDefinition[characterID];
		if (socketCharacterDefinition.permissionLevel <= 3)
			return false;

		struct Result
		{
			u8 CharacterDoesNotExist : 1 = 0;
			u8 DatabaseTransactionFailed : 1 = 0;
			u8 InsufficientPermission : 1 = 0;
			u8 Unused : 5 = 0;
		};

		Result result;
		if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2) || !databaseState.characterNameToDefinition.contains(charName))
		{
			// Send back that the character does not exist
			result.CharacterDoesNotExist = 1;

			// Send Packet with Result
			u8 resultAsValue = *reinterpret_cast<u8*>(&result);

			std::shared_ptr<Bytebuffer> characterDeleteResultMessage = nullptr;
			if (!Util::MessageBuilder::Cheat::BuildCharacterDeleteResultMessage(characterDeleteResultMessage, charName, resultAsValue))
				return false;

			networkState.server->SendPacket(socketID, characterDeleteResultMessage);
			return true;
		}

		// Disconnect Character if online
		auto& characterDefinition = databaseState.characterNameToDefinition[charName];

		if (characterDefinition.permissionLevel >= socketCharacterDefinition.permissionLevel)
        {
            result.InsufficientPermission = 1;

            // Send Packet with Result
            u8 resultAsValue = *reinterpret_cast<u8*>(&result);

            std::shared_ptr<Bytebuffer> characterDeleteResultMessage = nullptr;
            if (!Util::MessageBuilder::Cheat::BuildCharacterDeleteResultMessage(characterDeleteResultMessage, charName, resultAsValue))
                return false;

            networkState.server->SendPacket(socketID, characterDeleteResultMessage);
            return true;
        }

		if (networkState.characterIDToSocketID.contains(characterDefinition.id))
		{
			Network::SocketID existingSocketID = networkState.characterIDToSocketID[characterDefinition.id];
			networkState.server->CloseSocketID(existingSocketID);
		}

		// Delete Character
		{
			Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();

			if (auto conn = databaseState.controller->GetConnection(Server::Database::DBType::Character))
			{
				auto context = conn->Context();
				auto queryResult = context.exec_prepared("DeleteCharacter", characterDefinition.id);

                if (queryResult.affected_rows() == 0)
                {
                    result.DatabaseTransactionFailed = true;
					context.abort();
                }
                else
                {
                    databaseState.characterIDToDefinition.erase(databaseState.characterNameToDefinition[charName].id);
                    databaseState.characterNameToDefinition.erase(charName);

					context.commit();
                }
			}
			else
			{
                result.DatabaseTransactionFailed = true;
            }
		}

		// Send Packet
		u8 resultAsValue = *reinterpret_cast<u8*>(&result);

		std::shared_ptr<Bytebuffer> characterDeleteResultMessage = nullptr;
		if (!Util::MessageBuilder::Cheat::BuildCharacterDeleteResultMessage(characterDeleteResultMessage, charName, resultAsValue))
			return false;

		networkState.server->SendPacket(socketID, characterDeleteResultMessage);
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
			networkState.packetHandler->SetMessageHandler(Network::Opcode::MSG_ENTITY_MOVE, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 36u, 36u, &HandleOnEntityMove));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::MSG_ENTITY_TARGET_UPDATE, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 4u, 4u, &HandleOnEntityTargetUpdate));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_REQUEST_SPELLCAST, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 4u, 4u, &HandleOnRequestSpellCast));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_DAMAGE, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 4u, 4u, &HandleOnCheatDamage));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_KILL, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 0u, 0u, &HandleOnCheatKill));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_RESURRECT, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 0u, 0u, &HandleOnCheatResurrect));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_MORPH, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 4u, 4u, &HandleOnCheatMorph));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_CREATE_CHARACTER, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 3u, 11u, &HandleOnCheatCreateCharacter));
			networkState.packetHandler->SetMessageHandler(Network::Opcode::CMSG_CHEAT_DELETE_CHARACTER, Network::OpcodeHandler(Network::ConnectionStatus::AUTH_NONE, 3u, 11u, &HandleOnCheatDeleteCharacter));

			// Bind to IP/Port
			std::string ipAddress = "0.0.0.0";
			u16 port = 4000;

			Network::Socket::Result initResult = networkState.server->Init(Network::Socket::Mode::TCP, ipAddress, port);
			if (initResult == Network::Socket::Result::SUCCESS)
			{
				NC_LOG_INFO("Network : Listening on ({0}, {1})", ipAddress, port);
			}
			else
			{
				NC_LOG_CRITICAL("Network : Failed to bind on ({0}, {1})", ipAddress, port);
			}
		}
	}

	void NetworkConnection::Update(entt::registry& registry, f32 deltaTime)
	{
		entt::registry::context& ctx = registry.ctx();

		Singletons::GridSingleton& gridSingleton = ctx.get<Singletons::GridSingleton>();
		Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

		// Handle 'SocketConnectedEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketConnectedEvent>& connectedEvents = networkState.server->GetConnectedEvents();

			Network::SocketConnectedEvent connectedEvent;
			while (connectedEvents.try_dequeue(connectedEvent))
			{
				const Network::ConnectionInfo& connectionInfo = connectedEvent.connectionInfo;
				NC_LOG_INFO("Network : Client connected from (SocketID : {0}, \"{1}:{2}\")", static_cast<u32>(connectedEvent.socketID), connectionInfo.ipAddrStr, connectionInfo.port);
			}
		}

		// Handle 'SocketDisconnectedEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketDisconnectedEvent>& disconnectedEvents = networkState.server->GetDisconnectedEvents();

			Network::SocketDisconnectedEvent disconnectedEvent;
			while (disconnectedEvents.try_dequeue(disconnectedEvent))
			{
				NC_LOG_INFO("Network : Client disconnected (SocketID : {0})", static_cast<u32>(disconnectedEvent.socketID));

				networkState.socketIDRequestedToClose.erase(disconnectedEvent.socketID);

				// Start Cleanup here
				{
					if (networkState.socketIDToCharacterID.contains(disconnectedEvent.socketID))
					{
						u32 characterID = networkState.socketIDToCharacterID[disconnectedEvent.socketID];

						if (networkState.characterIDToSocketID.contains(characterID))
						{
							networkState.characterIDToSocketID.erase(characterID);
						}

						networkState.socketIDToCharacterID.erase(disconnectedEvent.socketID);
					}

					if (networkState.socketIDToEntity.contains(disconnectedEvent.socketID))
					{
						entt::entity entity = networkState.socketIDToEntity[disconnectedEvent.socketID];

						// Remove From Grid
						{
							gridSingleton.cell.mutex.lock();
							gridSingleton.cell.players.leaving.insert(entity);
							gridSingleton.cell.mutex.unlock();
						}

						if (networkState.EntityToSocketID.contains(entity))
						{
							networkState.EntityToSocketID.erase(entity);
						}

						networkState.socketIDToEntity.erase(disconnectedEvent.socketID);
						registry.destroy(entity);
					}
				}
			}
		}

		// Handle 'SocketMessageEvent'
		{
			moodycamel::ConcurrentQueue<Network::SocketMessageEvent>& messageEvents = networkState.server->GetMessageEvents();

			Network::SocketMessageEvent messageEvent;
			while (messageEvents.try_dequeue(messageEvent))
			{
				auto* messageHeader = reinterpret_cast<Network::PacketHeader*>(messageEvent.buffer->GetDataPointer());
				NC_LOG_INFO("Network : Message from (SocketID : {0}, Opcode : {1}, Size : {2})", static_cast<std::underlying_type<Network::SocketID>::type>(messageEvent.socketID), static_cast<std::underlying_type<Network::Opcode>::type>(messageHeader->opcode), messageHeader->size);
			
				// Invoke MessageHandler here
				if (networkState.packetHandler->CallHandler(messageEvent.socketID, messageEvent.buffer))
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