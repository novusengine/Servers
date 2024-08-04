#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GridSingleton.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Server-Common/Database/DBController.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Gameplay/Network/Opcode.h>
#include <Gameplay/Network/GameMessageRouter.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

#include <chrono>

namespace ECS::Systems
{
    bool HandleOnConnected(Network::SocketID socketID, Network::Message& message)
    {
        std::string charName = "";

        if (!message.buffer->GetString(charName))
            return false;

        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
            return false;

        entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        CharacterLoginRequest characterLoginRequest =
        { 
            .socketID = socketID, 
            .nameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length())
        };

        networkState.characterLoginRequest.enqueue(characterLoginRequest);
        return true;
    }

    bool HandleOnPing(Network::SocketID socketID, Network::Message& message)
    {
        u64 clientCurrentTime = 0;
        if (!message.buffer->GetU64(clientCurrentTime))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        auto& timeState = registry->ctx().get<Singletons::TimeState>();

        if (!networkState.socketIDToEntity.contains(socketID))
            return false;

        entt::entity socketEntity = networkState.socketIDToEntity[socketID];
        auto& netInfo = registry->get<Components::NetInfo>(socketEntity);

        auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        u64 timeSinceLastPing = currentTime - netInfo.lastPingTime;

        u16 timeForPingToArrive = static_cast<u16>(message.timestampProcessed - clientCurrentTime);
        u8 networkDiff = static_cast<u8>(message.timeToProcess + message.networkSleepDiff + message.networkUpdateDiff);
        u8 serverDiff = static_cast<u8>(timeState.deltaTime * 1000.0f);

        if (timeSinceLastPing < Components::NetInfo::PING_INTERVAL - 1000)
        {
            netInfo.numEarlyPings++;
        }
        else if (timeSinceLastPing > Components::NetInfo::PING_INTERVAL + 1000)
        {
            netInfo.numLatePings++;
        }

        if (netInfo.numEarlyPings > Components::NetInfo::MAX_EARLY_PINGS ||
            netInfo.numLatePings > Components::NetInfo::MAX_LATE_PINGS)
        {
            networkState.server->CloseSocketID(socketID);
            return true;
        }

        netInfo.ping = timeForPingToArrive;
        netInfo.numEarlyPings = 0;
        netInfo.numLatePings = 0;
        netInfo.lastPingTime = currentTime;

        std::shared_ptr<Bytebuffer> pongMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Heartbeat::BuildPongMessage(pongMessage, timeForPingToArrive, networkDiff, serverDiff))
            return false;

        networkState.server->SendPacket(socketID, pongMessage);
        return true;
    }

    bool HandleOnCheatDamage(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        u32 damage = 0;
        if (!message.buffer->GetU32(damage))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
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

            f32 damageToDeal = glm::min(static_cast<f32>(damage), unitStatsComponent->currentHealth);

            unitStatsComponent->currentHealth -= damageToDeal;
            unitStatsComponent->healthIsDirty = true;

            // Send Grid Message
            {
                std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
                    return false;

                ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
            }
        }

        return true;
    }
    bool HandleOnCheatKill(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

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
                std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
                    return false;

                ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
            }
        }

        return true;
    }
    bool HandleOnCheatResurrect(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

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
                std::shared_ptr<Bytebuffer> ressurectedMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildRessurectedMessage(ressurectedMessage, socketEntity, targetEntity))
                    return false;

                ECS::Util::Grid::SendToGrid(socketEntity, ressurectedMessage, { .SendToSelf = true });
            }
        }

        return true;
    }
    bool HandleOnCheatMorph(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        u32 displayID = 0;
        if (!message.buffer->GetU32(displayID))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

        auto& displayInfo = registry->get<Components::DisplayInfo>(socketEntity);
        displayInfo.displayID = displayID;

        std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, socketEntity, displayID))
            return false;

        ECS::Util::Grid::SendToGrid(socketEntity, displayInfoUpdateMessage, { .SendToSelf = 1 });

        return true;
    }
    bool HandleOnCheatDemorph(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

        auto& displayInfo = registry->get<Components::DisplayInfo>(socketEntity);

        u32 originalDisplayID = displayInfo.displayID;
        displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(displayInfo.race, displayInfo.gender);

        if (originalDisplayID == displayInfo.displayID)
            return true;

        std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, socketEntity, displayInfo.displayID))
            return false;

        ECS::Util::Grid::SendToGrid(socketEntity, displayInfoUpdateMessage, { .SendToSelf = 1 });
        return true;
    }
    bool HandleOnCheatCreateCharacter(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];
        //if (socketCharacterDefinition.permissionLevel <= 3)
        //    return false;

        struct Result
        {
            u8 NameIsInvalid : 1 = 0;
            u8 CharacterAlreadyExists : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 Unused : 5 = 0;
        };

        Result result;
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
        {
            result.NameIsInvalid = 1;
        }
        else if (databaseState.characterTables.nameHashToID.contains(charNameHash))
        {
            result.CharacterAlreadyExists = 1;
        }

        u8 resultAsValue = *reinterpret_cast<u8*>(&result);
        if (resultAsValue != 0)
        {
            std::shared_ptr<Bytebuffer> characterCreateResultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatCreateCharacterResponse(characterCreateResultMessage, resultAsValue))
                return false;

            networkState.server->SendPacket(socketID, characterCreateResultMessage);
            return true;
        }

        // Create Character
        {
            Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();

            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                auto transaction = conn->NewTransaction();
                auto queryResult = transaction.exec_prepared("CreateCharacter", charName, 1);
                if (queryResult.empty())
                {
                    transaction.abort();
                    result.DatabaseTransactionFailed = true;
                }
                else
                {
                    u32 characterID = queryResult[0][0].as<u32>();
                    Database::CharacterDefinition characterDefinition = { .id = characterID, .name = charName, .raceGenderClass = 1 };

                    databaseState.characterTables.idToDefinition[characterID] = characterDefinition;
                    databaseState.characterTables.nameHashToID[charNameHash] = characterID;

                    transaction.commit();
                }
            }
            else
            {
                result.DatabaseTransactionFailed = true;
            }
        }

        // Send Packet
        std::shared_ptr<Bytebuffer> characterCreateResultMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Cheat::BuildCheatCreateCharacterResponse(characterCreateResultMessage, resultAsValue))
            return false;

        networkState.server->SendPacket(socketID, characterCreateResultMessage);
        return true;
    }
    bool HandleOnCheatDeleteCharacter(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];
        //if (socketCharacterDefinition.permissionLevel <= 3)
        //    return false;

        struct Result
        {
            u8 CharacterDoesNotExist : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 InsufficientPermission : 1 = 0;
            u8 Unused : 5 = 0;
        };

        Result result;
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2) || !databaseState.characterTables.nameHashToID.contains(charNameHash))
        {
            // Send back that the character does not exist
            result.CharacterDoesNotExist = 1;

            // Send Packet with Result
            u8 resultAsValue = *reinterpret_cast<u8*>(&result);

            std::shared_ptr<Bytebuffer> characterDeleteResultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatDeleteCharacterResponse(characterDeleteResultMessage, resultAsValue))
                return false;

            networkState.server->SendPacket(socketID, characterDeleteResultMessage);
            return true;
        }

        // Disconnect Character if online
        u64 deleteCharacterID = databaseState.characterTables.nameHashToID[charNameHash];
        auto& characterDefinition = databaseState.characterTables.idToDefinition[deleteCharacterID];

        //if (characterDefinition.permissionLevel >= socketCharacterDefinition.permissionLevel)
        //{
        //    result.InsufficientPermission = 1;
        //
        //    // Send Packet with Result
        //    u8 resultAsValue = *reinterpret_cast<u8*>(&result);
        //
        //    std::shared_ptr<Bytebuffer> characterDeleteResultMessage = Bytebuffer::Borrow<64>();
        //    if (!Util::MessageBuilder::Cheat::BuildCharacterDeleteResultMessage(characterDeleteResultMessage, charName, resultAsValue))
        //        return false;
        //
        //    networkState.server->SendPacket(socketID, characterDeleteResultMessage);
        //    return true;
        //}

        if (networkState.characterIDToSocketID.contains(characterDefinition.id))
        {
            Network::SocketID existingSocketID = networkState.characterIDToSocketID[characterDefinition.id];
            networkState.server->CloseSocketID(existingSocketID);
        }

        // Delete Character
        {
            Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();

            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                auto transaction = conn->NewTransaction();
                auto queryResult = transaction.exec_prepared("DeleteCharacter", characterDefinition.id);

                if (queryResult.affected_rows() == 0)
                {
                    transaction.abort();
                    result.DatabaseTransactionFailed = true;
                }
                else
                {
                    databaseState.characterTables.idToDefinition.erase(characterDefinition.id);
                    databaseState.characterTables.nameHashToID.erase(charNameHash);

                    transaction.commit();
                }
            }
            else
            {
                result.DatabaseTransactionFailed = true;
            }
        }

        // Send Packet
        u8 resultAsValue = *reinterpret_cast<u8*>(&result);

        std::shared_ptr<Bytebuffer> characterDeleteResultMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Cheat::BuildCheatDeleteCharacterResponse(characterDeleteResultMessage, resultAsValue))
            return false;

        networkState.server->SendPacket(socketID, characterDeleteResultMessage);
        return true;
    }

    bool HandleOnCheatSetRace(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        GameDefine::UnitRace race = GameDefine::UnitRace::None;
        if (!message.buffer->Get(race))
            return false;

        if (race == GameDefine::UnitRace::None || race > GameDefine::UnitRace::Troll)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (!networkState.socketIDToCharacterID.contains(socketID))
            return false;

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];

            GameDefine::UnitRace originalRace = socketCharacterDefinition.GetRace();
            socketCharacterDefinition.SetRace(race);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec_prepared("UpdateCharacterSetRaceGenderClass", socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass);
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
                socketCharacterDefinition.SetRace(originalRace);
            }
            else
            {
                transaction.commit();

                auto& displayInfo = registry->get<Components::DisplayInfo>(socketEntity);
                displayInfo.race = race;
                displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(race, socketCharacterDefinition.GetGender());

                std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, socketEntity, displayInfo.displayID))
                    return false;

                ECS::Util::Grid::SendToGrid(socketEntity, displayInfoUpdateMessage, { .SendToSelf = 1 });
            }
        }

        return true;
    }
    bool HandleOnCheatSetGender(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        GameDefine::Gender gender = GameDefine::Gender::None;
        if (!message.buffer->Get(gender))
            return false;

        if (gender == GameDefine::Gender::None || gender > GameDefine::Gender::Other)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (!networkState.socketIDToCharacterID.contains(socketID))
            return false;

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];

            GameDefine::Gender originalGender = socketCharacterDefinition.GetGender();
            socketCharacterDefinition.SetGender(gender);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec_prepared("UpdateCharacterSetRaceGenderClass", socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass);
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
                socketCharacterDefinition.SetGender(originalGender);
            }
            else
            {
                transaction.commit();

                auto& displayInfo = registry->get<Components::DisplayInfo>(socketEntity);
                displayInfo.gender = gender;
                displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(socketCharacterDefinition.GetRace(), gender);

                std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, socketEntity, displayInfo.displayID))
                    return false;

                ECS::Util::Grid::SendToGrid(socketEntity, displayInfoUpdateMessage, { .SendToSelf = 1 });
            }
        }

        return true;
    }
    bool HandleOnCheatSetClass(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        GameDefine::UnitClass unitClass = GameDefine::UnitClass::None;
        if (!message.buffer->Get(unitClass))
            return false;

        if (unitClass == GameDefine::UnitClass::None || unitClass > GameDefine::UnitClass::Druid)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (!networkState.socketIDToCharacterID.contains(socketID))
            return false;

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];

            GameDefine::UnitClass originalGameClass = socketCharacterDefinition.GetGameClass();
            socketCharacterDefinition.SetGameClass(unitClass);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec_prepared("UpdateCharacterSetRaceGenderClass", socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass);
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
                socketCharacterDefinition.SetGameClass(originalGameClass);
            }
            else
            {
                transaction.commit();
            }
        }

        return true;
    }
    bool HandleOnCheatSetLevel(Network::SocketID socketID, entt::entity socketEntity, Network::Message& message)
    {
        u16 level = 0;
        if (!message.buffer->Get(level))
            return false;

        if (level == 0)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (!networkState.socketIDToCharacterID.contains(socketID))
            return false;

        u64 characterID = networkState.socketIDToCharacterID[socketID];
        if (!databaseState.characterTables.idToDefinition.contains(characterID))
            return false;

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& socketCharacterDefinition = databaseState.characterTables.idToDefinition[characterID];

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec_prepared("UpdateCharacterSetLevel", socketCharacterDefinition.id, level);
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();

                socketCharacterDefinition.level = level;
            }
        }

        return true;
    }
    bool HandleOnSendCheatCommand(Network::SocketID socketID, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        if (!networkState.socketIDToEntity.contains(socketID))
            return false;

        entt::entity socketEntity = networkState.socketIDToEntity[socketID];
        if (!registry->valid(socketEntity))
            return false;

        Network::CheatCommands command = Network::CheatCommands::None;
        if (!message.buffer->Get(command))
            return false;

        switch (command)
        {
            case Network::CheatCommands::Damage:
            {
                return HandleOnCheatDamage(socketID, socketEntity, message);
            }

            case Network::CheatCommands::Kill:
            {
                return HandleOnCheatKill(socketID, socketEntity, message);
            }

            case Network::CheatCommands::Resurrect:
            {
                return HandleOnCheatResurrect(socketID, socketEntity, message);
            }

            case Network::CheatCommands::Morph:
            {
                return HandleOnCheatMorph(socketID, socketEntity, message);
            }

            case Network::CheatCommands::Demorph:
            {
                return HandleOnCheatDemorph(socketID, socketEntity, message);
            }

            case Network::CheatCommands::CreateCharacter:
            {
                return HandleOnCheatCreateCharacter(socketID, socketEntity, message);
            }

            case Network::CheatCommands::DeleteCharacter:
            {
                return HandleOnCheatDeleteCharacter(socketID, socketEntity, message);
            }

            case Network::CheatCommands::SetRace:
            {
                return HandleOnCheatSetRace(socketID, socketEntity, message);
            }

            case Network::CheatCommands::SetGender:
            {
                return HandleOnCheatSetGender(socketID, socketEntity, message);
            }

            case Network::CheatCommands::SetClass:
            {
                return HandleOnCheatSetClass(socketID, socketEntity, message);
            }

            case Network::CheatCommands::SetLevel:
            {
                return HandleOnCheatSetLevel(socketID, socketEntity, message);
            }

            default: break;
        }

        return true;
    }

    bool HandleOnEntityMove(Network::SocketID socketID, Network::Message& message)
    {
        vec3 position = vec3(0.0f);
        quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
        Components::MovementFlags movementFlags = {};
        f32 verticalVelocity = 0.0f;

        if (!message.buffer->Get(position))
            return false;

        if (!message.buffer->Get(rotation))
            return false;

        if (!message.buffer->Get(movementFlags))
            return false;

        if (!message.buffer->Get(verticalVelocity))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        if (!networkState.socketIDToEntity.contains(socketID))
            return false;

        entt::entity socketEntity = networkState.socketIDToEntity[socketID];
        Components::Transform& transform = registry->get<Components::Transform>(socketEntity);

        transform.position = position;
        transform.rotation = rotation;

        std::shared_ptr<Bytebuffer> entityMoveMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityMoveMessage(entityMoveMessage, socketEntity, position, rotation, movementFlags, verticalVelocity))
            return false;

        ECS::Util::Grid::SendToGrid(socketEntity, entityMoveMessage);
        return true;
    }
    bool HandleOnEntityTargetUpdate(Network::SocketID socketID, Network::Message& message)
    {
        entt::entity targetEntity = entt::null;

        if (!message.buffer->Get(targetEntity))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        if (!networkState.socketIDToEntity.contains(socketID))
            return false;

        entt::entity socketEntity = networkState.socketIDToEntity[socketID];

        Components::TargetInfo& targetInfo = registry->get<Components::TargetInfo>(socketEntity);
        targetInfo.target = targetEntity;

        std::shared_ptr<Bytebuffer> targetUpdateMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityTargetUpdateMessage(targetUpdateMessage, socketEntity, targetEntity))
            return false;

        ECS::Util::Grid::SendToGrid(socketEntity, targetUpdateMessage);

        return true;
    }

    bool HandleOnRequestSpellCast(Network::SocketID socketID, Network::Message& message)
    {
        u32 spellID = 0;

        if (!message.buffer->GetU32(spellID))
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
            std::shared_ptr<Bytebuffer> castResultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, 0.0f, 0.0f))
                return false;

            networkState.server->SendPacket(socketID, castResultMessage);
            return true;
        }

        Components::CastInfo& castInfo = registry->emplace<Components::CastInfo>(socketEntity);

        castInfo.target = targetInfo->target;
        castInfo.castTime = 1.5f;
        castInfo.duration = 0.0f;

        std::shared_ptr<Bytebuffer> castResultMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, castInfo.castTime, castInfo.duration))
            return false;

        networkState.server->SendPacket(socketID, castResultMessage);

        // Send Grid Message
        {
            std::shared_ptr<Bytebuffer> entityCastSpellMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Entity::BuildEntitySpellCastMessage(entityCastSpellMessage, socketEntity, castInfo.castTime, castInfo.duration))
                return false;

            ECS::Util::Grid::SendToGrid(socketEntity, entityCastSpellMessage);
        }

        return true;
    }

    void NetworkConnection::Init(entt::registry& registry)
    {
        entt::registry::context& ctx = registry.ctx();

        Singletons::NetworkState& networkState = ctx.emplace<Singletons::NetworkState>();

        // Setup NetServer
        {
            networkState.server = std::make_unique<Network::Server>();
            networkState.gameMessageRouter = std::make_unique<Network::GameMessageRouter>();

            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_Connect,                  Network::GameMessageHandler(Network::ConnectionStatus::None,        0u, -1, &HandleOnConnected));
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_Ping,                     Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnPing));

            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_SendCheatCommand,         Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnSendCheatCommand));
            
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Shared_EntityMove,               Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnEntityMove));
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Shared_EntityTargetUpdate,       Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnEntityTargetUpdate));
            
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_LocalRequestSpellCast,    Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnRequestSpellCast));

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
                        u64 characterID = networkState.socketIDToCharacterID[disconnectedEvent.socketID];

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

                        if (networkState.entityToSocketID.contains(entity))
                        {
                            networkState.entityToSocketID.erase(entity);
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
                auto* messageHeader = reinterpret_cast<Network::MessageHeader*>(messageEvent.message.buffer->GetDataPointer());
                NC_LOG_INFO("Network : Message from (SocketID : {0}, Opcode : {1}, Size : {2})", static_cast<std::underlying_type<Network::SocketID>::type>(messageEvent.socketID), static_cast<std::underlying_type<Network::GameOpcode>::type>(messageHeader->opcode), messageHeader->size);

                // Invoke MessageHandler here
                if (networkState.gameMessageRouter->CallHandler(messageEvent.socketID, messageEvent.message))
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