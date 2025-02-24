#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/GameCommands/CharacterCommands.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GameCommandQueue.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/ItemUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Gameplay/GameDefine.h>
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
        u16 ping = 0;
        if (!message.buffer->GetU16(ping))
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

        netInfo.ping = ping;
        netInfo.numEarlyPings = 0;
        netInfo.numLatePings = 0;
        netInfo.lastPingTime = currentTime;

        std::shared_ptr<Bytebuffer> updateStatsMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Heartbeat::BuildUpdateStatsMessage(updateStatsMessage, serverDiff))
            return false;

        networkState.server->SendPacket(socketID, updateStatsMessage);

        return true;
    }

    bool HandleOnCheatDamage(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 damage = 0;
        if (!message.buffer->GetU32(damage))
            return false;

        //entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        //entt::entity targetEntity = socketEntity;
        //if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
        //{
        //    if (registry->valid(targetInfo->target))
        //    {
        //        targetEntity = targetInfo->target;
        //    }
        //}
        //
        //if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
        //{
        //    if (unitStatsComponent->currentHealth <= 0)
        //        return true;
        //
        //    f32 damageToDeal = glm::min(static_cast<f32>(damage), unitStatsComponent->currentHealth);
        //
        //    unitStatsComponent->currentHealth -= damageToDeal;
        //    unitStatsComponent->healthIsDirty = true;
        //
        //    // Send Grid Message
        //    {
        //        std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
        //        if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
        //            return false;
        //
        //        ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
        //    }
        //}

        return true;
    }
    bool HandleOnCheatKill(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        //entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        //
        //entt::entity targetEntity = socketEntity;
        //if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
        //{
        //    if (registry->valid(targetInfo->target))
        //    {
        //        targetEntity = targetInfo->target;
        //    }
        //}
        //
        //if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
        //{
        //    if (unitStatsComponent->currentHealth <= 0.0f)
        //        return true;
        //
        //    f32 damageToDeal = unitStatsComponent->currentHealth;
        //
        //    unitStatsComponent->currentHealth = 0.0f;
        //    unitStatsComponent->healthIsDirty = true;
        //
        //    // Send Grid Message
        //    {
        //        std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
        //        if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, socketEntity, targetEntity, damageToDeal))
        //            return false;
        //
        //        ECS::Util::Grid::SendToGrid(socketEntity, damageDealtMessage, { .SendToSelf = true });
        //    }
        //}

        return true;
    }
    bool HandleOnCheatResurrect(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        //entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        //
        //entt::entity targetEntity = socketEntity;
        //if (auto* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity))
        //{
        //    if (registry->valid(targetInfo->target))
        //    {
        //        targetEntity = targetInfo->target;
        //    }
        //}
        //
        //if (auto* unitStatsComponent = registry->try_get<Components::UnitStatsComponent>(targetEntity))
        //{
        //    unitStatsComponent->currentHealth = unitStatsComponent->maxHealth;
        //    unitStatsComponent->healthIsDirty = true;
        //
        //    // Send Grid Message
        //    {
        //        std::shared_ptr<Bytebuffer> ressurectedMessage = Bytebuffer::Borrow<64>();
        //        if (!Util::MessageBuilder::CombatLog::BuildRessurectedMessage(ressurectedMessage, socketEntity, targetEntity))
        //            return false;
        //
        //        ECS::Util::Grid::SendToGrid(socketEntity, ressurectedMessage, { .SendToSelf = true });
        //    }
        //}

        return true;
    }
    bool HandleOnCheatMorph(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 displayID = 0;
        if (!message.buffer->GetU32(displayID))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
        if (displayID == displayInfo.displayID)
            return true;

        displayInfo.displayID = displayID;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, objectInfo.guid, displayInfo))
            return false;

        ECS::Util::Grid::SendToNearby(entity, displayInfoUpdateMessage, true);

        return true;
    }
    bool HandleOnCheatDemorph(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;

        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);

        u32 nativeDisplayID = UnitUtils::GetDisplayIDFromRaceGender(displayInfo.race, displayInfo.gender);
        if (nativeDisplayID == displayInfo.displayID)
            return true;

        displayInfo.displayID = nativeDisplayID;
        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);

        std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, objectInfo.guid, displayInfo))
            return false;

        ECS::Util::Grid::SendToNearby(entity, displayInfoUpdateMessage, true);
        return true;
    }
    bool HandleOnCheatCreateCharacter(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        struct Result
        {
            u8 NameIsInvalid : 1 = 0;
            u8 CharacterAlreadyExists : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 Unused : 5 = 0;
        };

        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        Result result;
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
        {
            result.NameIsInvalid = 1;
        }
        else if (databaseState.characterTables.charNameHashToCharID.contains(charNameHash))
        {
            result.CharacterAlreadyExists = 1;
        }

        u8 resultAsValue = *reinterpret_cast<u8*>(&result);
        if (resultAsValue != 0)
        {
            std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatCreateCharacterResponse(resultMessage, resultAsValue))
                return false;

            networkState.server->SendPacket(socketID, resultMessage);
            return true;
        }

        auto& gameCommandQueue = ctx.get<Singletons::GameCommandQueue>();
        auto* createCharacterCommand = GameCommands::Character::CharacterCreate::Create(charName, 1, [socketID](entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase, bool result)
        {
            Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
            if (!networkState.activeSocketIDs.contains(socketID))
                return;

            Result cheatCommandResult =
            {
                .NameIsInvalid = 0,
                .CharacterAlreadyExists = 0,
                .DatabaseTransactionFailed = !result,
                .Unused = 0
            };
            u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&cheatCommandResult);

            std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatCreateCharacterResponse(resultMessage, cheatCommandResultVal))
                return;

            networkState.server->SendPacket(socketID, resultMessage);
        });

        gameCommandQueue.Push(createCharacterCommand);
        return true;
    }
    bool HandleOnCheatDeleteCharacter(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        struct Result
        {
            u8 CharacterDoesNotExist : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 InsufficientPermission : 1 = 0;
            u8 Unused : 5 = 0;
        };

        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = ServiceLocator::GetEnttRegistries()->gameRegistry->ctx();
        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        Result result;
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2) || !databaseState.characterTables.charNameHashToCharID.contains(charNameHash))
        {
            // Send back that the character does not exist
            result.CharacterDoesNotExist = 1;

            // Send Packet with Result
            u8 resultAsValue = *reinterpret_cast<u8*>(&result);

            std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatDeleteCharacterResponse(resultMessage, resultAsValue))
                return false;

            networkState.server->SendPacket(socketID, resultMessage);
            return true;
        }

        // Disconnect Character if online
        u64 characterIDToDelete = databaseState.characterTables.charNameHashToCharID[charNameHash];

        auto& gameCommandQueue = ctx.get<Singletons::GameCommandQueue>();
        auto* characterDeleteCommand = GameCommands::Character::CharacterDelete::Create(characterIDToDelete, [socketID](entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase, bool result)
        {
            Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
            if (!networkState.activeSocketIDs.contains(socketID))
                return;

            Result cheatCommandResult =
            {
                .CharacterDoesNotExist = 0,
                .DatabaseTransactionFailed = !result,
                .InsufficientPermission = 0,
                .Unused = 0
            };
            u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&cheatCommandResult);

            std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Cheat::BuildCheatDeleteCharacterResponse(resultMessage, cheatCommandResultVal))
                return;

            networkState.server->SendPacket(socketID, resultMessage);
        });

        gameCommandQueue.Push(characterDeleteCommand);
        return true;
    }

    bool HandleOnCheatSetRace(Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
            u64 characterID = objectInfo.guid.GetCounter();

            auto& socketCharacterDefinition = databaseState.characterTables.charIDToDefinition[characterID];

            GameDefine::UnitRace originalRace = socketCharacterDefinition.GetRace();
            socketCharacterDefinition.SetRace(race);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetRaceGenderClass"), pqxx::params{ socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
                socketCharacterDefinition.SetRace(originalRace);
            }
            else
            {
                transaction.commit();

                auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
                displayInfo.race = race;
                displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(race, socketCharacterDefinition.GetGender());

                std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID), displayInfo))
                    return false;
                
                ECS::Util::Grid::SendToNearby(entity, displayInfoUpdateMessage, true);
            }
        }

        return true;
    }
    bool HandleOnCheatSetGender(Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
            u64 characterID = objectInfo.guid.GetCounter();

            auto& socketCharacterDefinition = databaseState.characterTables.charIDToDefinition[characterID];

            GameDefine::Gender originalGender = socketCharacterDefinition.GetGender();
            socketCharacterDefinition.SetGender(gender);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped{ "CharacterSetRaceGenderClass" }, pqxx::params{ socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
                socketCharacterDefinition.SetGender(originalGender);
            }
            else
            {
                transaction.commit();

                auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
                displayInfo.gender = gender;
                displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(socketCharacterDefinition.GetRace(), gender);

                std::shared_ptr<Bytebuffer> displayInfoUpdateMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(displayInfoUpdateMessage, GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID), displayInfo))
                    return false;
                
                ECS::Util::Grid::SendToNearby(entity, displayInfoUpdateMessage, true);
            }
        }

        return true;
    }
    bool HandleOnCheatSetClass(Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
            u64 characterID = objectInfo.guid.GetCounter();

            auto& socketCharacterDefinition = databaseState.characterTables.charIDToDefinition[characterID];

            GameDefine::UnitClass originalGameClass = socketCharacterDefinition.GetGameClass();
            socketCharacterDefinition.SetGameClass(unitClass);

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetRaceGenderClass"), pqxx::params{ socketCharacterDefinition.id, socketCharacterDefinition.raceGenderClass });
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
    bool HandleOnCheatSetLevel(Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
            u64 characterID = objectInfo.guid.GetCounter();

            auto& socketCharacterDefinition = databaseState.characterTables.charIDToDefinition[characterID];

            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetLevel"), pqxx::params{ socketCharacterDefinition.id, level });
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

    bool HandleOnCheatSetItemTemplate(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemTemplate itemTemplate;
        if (!GameDefine::Database::ItemTemplate::Read(message.buffer, itemTemplate))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetItemTemplate"), pqxx::params{ itemTemplate.id, itemTemplate.displayID, (u16)itemTemplate.bind, (u16)itemTemplate.rarity, (u16)itemTemplate.category, (u16)itemTemplate.type, itemTemplate.virtualLevel, itemTemplate.requiredLevel, itemTemplate.durability, itemTemplate.iconID, itemTemplate.name, itemTemplate.description, itemTemplate.armor, itemTemplate.statTemplateID, itemTemplate.armorTemplateID, itemTemplate.weaponTemplateID, itemTemplate.shieldTemplateID });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                databaseState.itemTables.templateIDToTemplateDefinition[itemTemplate.id] = itemTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemStatTemplate(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemStatTemplate itemStatTemplate;
        if (!GameDefine::Database::ItemStatTemplate::Read(message.buffer, itemStatTemplate))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetItemStatTemplate"), pqxx::params{ itemStatTemplate.id, (u16)itemStatTemplate.statTypes[0], (u16)itemStatTemplate.statTypes[1], (u16)itemStatTemplate.statTypes[2], (u16)itemStatTemplate.statTypes[3], (u16)itemStatTemplate.statTypes[4], (u16)itemStatTemplate.statTypes[5], (u16)itemStatTemplate.statTypes[6], (u16)itemStatTemplate.statTypes[7], itemStatTemplate.statValues[0], itemStatTemplate.statValues[1], itemStatTemplate.statValues[2], itemStatTemplate.statValues[3], itemStatTemplate.statValues[4], itemStatTemplate.statValues[5], itemStatTemplate.statValues[6], itemStatTemplate.statValues[7] });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                databaseState.itemTables.statTemplateIDToTemplateDefinition[itemStatTemplate.id] = itemStatTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemArmorTemplate(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemArmorTemplate itemArmorTemplate;
        if (!GameDefine::Database::ItemArmorTemplate::Read(message.buffer, itemArmorTemplate))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetItemArmorTemplate"), pqxx::params{ itemArmorTemplate.id, (u16)itemArmorTemplate.equipType, itemArmorTemplate.bonusArmor });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                databaseState.itemTables.armorTemplateIDToTemplateDefinition[itemArmorTemplate.id] = itemArmorTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemWeaponTemplate(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemWeaponTemplate itemWeaponTemplate;
        if (!GameDefine::Database::ItemWeaponTemplate::Read(message.buffer, itemWeaponTemplate))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetItemWeaponTemplate"), pqxx::params{ itemWeaponTemplate.id, (u16)itemWeaponTemplate.weaponStyle, itemWeaponTemplate.minDamage, itemWeaponTemplate.maxDamage, itemWeaponTemplate.speed });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                databaseState.itemTables.weaponTemplateIDToTemplateDefinition[itemWeaponTemplate.id] = itemWeaponTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemShieldTemplate(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemShieldTemplate itemShieldTemplate;
        if (!GameDefine::Database::ItemShieldTemplate::Read(message.buffer, itemShieldTemplate))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetItemShieldTemplate"), pqxx::params{ itemShieldTemplate.id, itemShieldTemplate.bonusArmor, itemShieldTemplate.block });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                databaseState.itemTables.shieldTemplateIDToTemplateDefinition[itemShieldTemplate.id] = itemShieldTemplate;
            }
        }

        return true;
    }

    bool HandleOnCheatAddItem(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        bool isValidItemID = databaseState.itemTables.templateIDToTemplateDefinition.contains(itemID);
        bool playerHasBags = registry->all_of<Components::PlayerContainers>(entity);

        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;

        if (isValidItemID && playerHasBags)
        {
            auto& playerContainers = registry->get<Components::PlayerContainers>(entity);
            u32 numBags = static_cast<u32>(playerContainers.bags.size());

            for (u32 i = 0; i < numBags; i++)
            {
                auto& bag = playerContainers.bags[i];

                u16 nextFreeSlot = bag.GetNextFreeSlot();
                if (nextFreeSlot == Database::Container::INVALID_SLOT)
                    continue;

                containerIndex = i;
                slotIndex = nextFreeSlot;
                break;
            }

            bool canAddItem = containerIndex != Database::Container::INVALID_SLOT && slotIndex != Database::Container::INVALID_SLOT;
            if (canAddItem)
            {
                u64 characterID = registry->get<Components::ObjectInfo>(entity).guid.GetCounter();

                const auto& baseContainerItem = playerContainers.equipment.GetItem(19 + containerIndex);
                GameDefine::ObjectGuid containerGuid = baseContainerItem.objectGuid;
                u64 containerID = containerGuid.GetCounter();

                auto& gameCommandQueue = ctx.get<Singletons::GameCommandQueue>();
                auto* createItemCommand = GameCommands::Character::CharacterCreateItem::Create(characterID, itemID, 1, containerID, (u8)slotIndex, [socketID, containerIndex](entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase, bool result, u64 itemInstanceID)
                {
                    Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
                    if (!networkState.activeSocketIDs.contains(socketID))
                        return;

                    auto* createItemCommand = static_cast<GameCommands::Character::CharacterCreateItem*>(gameCommandBase);

                    if (result)
                    {
                        entt::entity entity = networkState.socketIDToEntity[socketID];
                        auto& databaseState = registry.ctx().get<Singletons::DatabaseState>();
                        auto& playerContainers = registry.get<Components::PlayerContainers>(entity);
                        auto& bag = playerContainers.bags[containerIndex];

                        const auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];
                        GameDefine::ObjectGuid itemInstanceGuid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Item, itemInstanceID);
                        bag.AddItemToSlot(itemInstanceGuid, createItemCommand->slot);

                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                        if (Util::MessageBuilder::Item::BuildItemCreate(buffer, itemInstanceGuid, itemInstance))
                        {
                            if (Util::MessageBuilder::Container::BuildAddToSlot(buffer, containerIndex + 1, createItemCommand->slot, itemInstanceGuid))
                            {
                                networkState.server->SendPacket(socketID, buffer);
                            }
                        }

                        return;
                    }

                    // Send back that the item could not be added
                });
                gameCommandQueue.Push(createItemCommand);
                return true;
            }
        }

        // Send back that the item could not be added
        
        return true;
    }
    bool HandleOnCheatRemoveItem(Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();
        auto& databaseState = ctx.get<Singletons::DatabaseState>();

        bool isValidItemID = databaseState.itemTables.templateIDToTemplateDefinition.contains(itemID);
        bool playerHasBags = registry->all_of<Components::PlayerContainers>(entity);

        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;

        if (isValidItemID && playerHasBags)
        {
            auto& playerContainers = registry->get<Components::PlayerContainers>(entity);
            u32 numBags = static_cast<u32>(playerContainers.bags.size());
            bool foundItem = false;

            for (u32 i = 0; i < numBags; i++)
            {
                auto& bag = playerContainers.bags[i];

                if (bag.IsEmpty())
                    continue;

                u8 numSlots = bag.GetTotalSlots();
                for (u8 j = 0; j < numSlots; j++)
                {
                    const auto& item = bag.GetItem(j);
                    if (item.IsEmpty())
                        continue;

                    u64 itemInstanceID = item.objectGuid.GetCounter();
                    auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];

                    if (itemInstance.itemID == itemID)
                    {
                        containerIndex = i + 1;
                        slotIndex = j;

                        foundItem = true;
                        break;
                    }
                }

                if (foundItem)
                    break;
            }

            if (!foundItem)
            {
                // Check Equipped Items
                for (u32 i = 0; i < 19; i++)
                {
                    const auto& item = playerContainers.equipment.GetItem(i);
                    if (item.IsEmpty())
                        continue;

                    u64 itemInstanceID = item.objectGuid.GetCounter();
                    auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];

                    if (itemInstance.itemID == itemID)
                    {
                        containerIndex = 0;
                        slotIndex = i;
                        break;
                    }
                }
            }
            
            bool canRemoveItem = containerIndex != Database::Container::INVALID_SLOT && slotIndex != Database::Container::INVALID_SLOT;
            if (canRemoveItem)
            {
                u64 characterID = registry->get<Components::ObjectInfo>(entity).guid.GetCounter();

                u64 containerID = 0;
                if (containerIndex > 0)
                {
                    const auto& baseContainerItem = playerContainers.equipment.GetItem(19 + (containerIndex - 1));
                    GameDefine::ObjectGuid containerGuid = baseContainerItem.objectGuid;
                    containerID = containerGuid.GetCounter();
                }

                auto& gameCommandQueue = ctx.get<Singletons::GameCommandQueue>();
                auto* deleteItemCommand = GameCommands::Character::CharacterDeleteItem::Create(characterID, itemID, itemCount, containerID, (u8)slotIndex, [socketID, containerIndex, slotIndex](entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase, bool result, u64 itemInstanceID)
                {
                    Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
                    if (!networkState.activeSocketIDs.contains(socketID))
                        return;

                    if (result)
                    {
                        entt::entity entity = networkState.socketIDToEntity[socketID];
                        auto& playerContainers = registry.get<Components::PlayerContainers>(entity);

                        if (containerIndex == 0)
                        {
                            auto& bag = playerContainers.equipment;
                            bag.RemoveItem((u8)slotIndex);
                        }
                        else
                        {
                            auto& bag = playerContainers.bags[containerIndex - 1];
                            bag.RemoveItem((u8)slotIndex);
                        }

                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                        if (Util::MessageBuilder::Container::BuildRemoveFromSlot(buffer, (u8)containerIndex, (u8)slotIndex))
                        {
                            networkState.server->SendPacket(socketID, buffer);
                        }

                        return;
                    }

                    // Send back that the item could not be removed
                });
                gameCommandQueue.Push(deleteItemCommand);
            }
        }

        return true;
    }

    bool HandleOnSendCheatCommand(Network::SocketID socketID, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        Network::CheatCommands command = Network::CheatCommands::None;
        if (!message.buffer->Get(command))
            return false;

        entt::entity entity = networkState.socketIDToEntity[socketID];
        if (!registry->valid(entity))
            return false;

        switch (command)
        {
            case Network::CheatCommands::Damage:
            {
                return HandleOnCheatDamage(socketID, entity, message);
            }

            case Network::CheatCommands::Kill:
            {
                return HandleOnCheatKill(socketID, entity, message);
            }

            case Network::CheatCommands::Resurrect:
            {
                return HandleOnCheatResurrect(socketID, entity, message);
            }

            case Network::CheatCommands::Morph:
            {
                return HandleOnCheatMorph(socketID, entity, message);
            }

            case Network::CheatCommands::Demorph:
            {
                return HandleOnCheatDemorph(socketID, entity, message);
            }

            case Network::CheatCommands::CreateCharacter:
            {
                return HandleOnCheatCreateCharacter(socketID, entity, message);
            }

            case Network::CheatCommands::DeleteCharacter:
            {
                return HandleOnCheatDeleteCharacter(socketID, entity, message);
            }

            case Network::CheatCommands::SetRace:
            {
                return HandleOnCheatSetRace(socketID, entity, message);
            }

            case Network::CheatCommands::SetGender:
            {
                return HandleOnCheatSetGender(socketID, entity, message);
            }

            case Network::CheatCommands::SetClass:
            {
                return HandleOnCheatSetClass(socketID, entity, message);
            }

            case Network::CheatCommands::SetLevel:
            {
                return HandleOnCheatSetLevel(socketID, entity, message);
            }

            case Network::CheatCommands::SetItemTemplate:
            {
                return HandleOnCheatSetItemTemplate(socketID, entity, message);
            }

            case Network::CheatCommands::SetItemStatTemplate:
            {
                return HandleOnCheatSetItemStatTemplate(socketID, entity, message);
            }

            case Network::CheatCommands::SetItemArmorTemplate:
            {
                return HandleOnCheatSetItemArmorTemplate(socketID, entity, message);
            }

            case Network::CheatCommands::SetItemWeaponTemplate:
            {
                return HandleOnCheatSetItemWeaponTemplate(socketID, entity, message);
            }

            case Network::CheatCommands::SetItemShieldTemplate:
            {
                return HandleOnCheatSetItemShieldTemplate(socketID, entity, message);
            }

            case Network::CheatCommands::AddItem:
            {
                return HandleOnCheatAddItem(socketID, entity, message);
            }

            case Network::CheatCommands::RemoveItem:
            {
                return HandleOnCheatRemoveItem(socketID, entity, message);
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

        entt::entity entity = networkState.socketIDToEntity[socketID];

        Components::Transform& transform = registry->get<Components::Transform>(entity);
        transform.position = position;
        transform.rotation = rotation;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);

        std::shared_ptr<Bytebuffer> entityMoveMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityMoveMessage(entityMoveMessage, objectInfo.guid, position, rotation, movementFlags, verticalVelocity))
            return false;

        Singletons::WorldState& worldState = registry->ctx().get<Singletons::WorldState>();
        u32 mapID = 0; // Should ideally be in the transform?

        World& world = worldState.GetWorld(mapID);
        world.UpdatePlayer(objectInfo.guid, position.x, position.z);

        ECS::Util::Grid::SendToNearby(entity, entityMoveMessage);
        return true;
    }
    bool HandleOnEntityTargetUpdate(Network::SocketID socketID, Network::Message& message)
    {
        entt::entity targetEntity = entt::null;

        if (!message.buffer->Get(targetEntity))
            return false;

        //entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        //Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();
        //
        //if (!networkState.socketIDToEntity.contains(socketID))
        //    return false;
        //
        //entt::entity socketEntity = networkState.socketIDToEntity[socketID];
        //
        //Components::TargetInfo& targetInfo = registry->get<Components::TargetInfo>(socketEntity);
        //targetInfo.target = targetEntity;
        //
        //std::shared_ptr<Bytebuffer> targetUpdateMessage = Bytebuffer::Borrow<64>();
        //if (!Util::MessageBuilder::Entity::BuildEntityTargetUpdateMessage(targetUpdateMessage, socketEntity, targetEntity))
        //    return false;
        //
        //ECS::Util::Grid::SendToGrid(socketEntity, targetUpdateMessage);

        return true;
    }

    bool HandleOnContainerSwapSlots(Network::SocketID socketID, Network::Message& message)
    {
        u8 srcContainerIndex = 0;
        u8 destContainerIndex = 0;
        u8 srcSlotIndex = 0;
        u8 dstSlotIndex = 0;

        bool didFail = false;

        didFail |= !message.buffer->GetU8(srcContainerIndex);
        didFail |= !message.buffer->GetU8(destContainerIndex);
        didFail |= !message.buffer->GetU8(srcSlotIndex);
        didFail |= !message.buffer->GetU8(dstSlotIndex);

        if (didFail)
            return false;

        if (srcContainerIndex == destContainerIndex && srcSlotIndex == dstSlotIndex)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        if (!networkState.socketIDToEntity.contains(socketID))
            return false;

        entt::entity entity = networkState.socketIDToEntity[socketID];
        if (!registry->all_of<Components::PlayerContainers>(entity))
            return false;

        auto& playerContainers = registry->get<Components::PlayerContainers>(entity);

        bool isSameContainer = srcContainerIndex == destContainerIndex;
        if (isSameContainer)
        {
            Database::Container* container = nullptr;

            if (srcContainerIndex == 0)
            {
                container = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = srcContainerIndex - 1;
                u32 numContainers = static_cast<u32>(playerContainers.bags.size());
                if (bagIndex >= numContainers)
                    return false;

                container = &playerContainers.bags[bagIndex];
            }

            u32 numContainerSlots = container->GetTotalSlots();
            if (srcSlotIndex >= numContainerSlots || dstSlotIndex >= numContainerSlots)
                return false;

            auto& srcItem = container->GetItem(srcSlotIndex);
            if (srcItem.IsEmpty())
                return false;
        }
        else
        {
            Database::Container* srcContainer = nullptr;
            Database::Container* destContainer = nullptr;

            if (srcContainerIndex == 0)
            {
                srcContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = srcContainerIndex - 1;
                u32 numContainers = static_cast<u32>(playerContainers.bags.size());
                if (bagIndex >= numContainers)
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
            }

            if (destContainerIndex == 0)
            {
                destContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = destContainerIndex - 1;
                u32 numContainers = static_cast<u32>(playerContainers.bags.size());
                if (bagIndex >= numContainers)
                    return false;

                destContainer = &playerContainers.bags[bagIndex];
            }

            if (srcSlotIndex >= srcContainer->GetTotalSlots() || dstSlotIndex >= destContainer->GetTotalSlots())
                return false;

            auto& srcItem = srcContainer->GetItem(srcSlotIndex);
            if (srcItem.IsEmpty())
                return false;
        }

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);

        auto& gameCommandQueue = registry->ctx().get<Singletons::GameCommandQueue>();
        auto* swapContainerSlotsCommand = GameCommands::Character::CharacterSwapContainerSlots::Create(objectInfo.guid.GetCounter(), srcContainerIndex, destContainerIndex, srcSlotIndex, dstSlotIndex, [socketID](entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase, bool result)
        {
            Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
            if (!networkState.activeSocketIDs.contains(socketID))
                return;

            auto* characterSwapContainerSlots = static_cast<ECS::GameCommands::Character::CharacterSwapContainerSlots*>(gameCommandBase);

            if (result)
            {
                entt::entity charEntity = networkState.characterIDToEntity[characterSwapContainerSlots->characterID];
                auto& playerContainers = registry.get<Components::PlayerContainers>(charEntity);

                bool isSameContainer = characterSwapContainerSlots->srcContainerIndex == characterSwapContainerSlots->destContainerIndex;
                if (isSameContainer)
                {
                    Database::Container* container = nullptr;

                    if (characterSwapContainerSlots->srcContainerIndex == 0)
                    {
                        container = &playerContainers.equipment;
                    }
                    else
                    {
                        u32 bagIndex = characterSwapContainerSlots->srcContainerIndex - 1;
                        container = &playerContainers.bags[bagIndex];
                    }

                    container->SwapItems(characterSwapContainerSlots->srcSlotIndex, characterSwapContainerSlots->destSlotIndex);
                }
                else
                {
                    Database::Container* srcContainer = nullptr;
                    Database::Container* destContainer = nullptr;

                    if (characterSwapContainerSlots->srcContainerIndex == 0)
                    {
                        srcContainer = &playerContainers.equipment;
                    }
                    else
                    {
                        u32 bagIndex = characterSwapContainerSlots->srcContainerIndex - 1;
                        srcContainer = &playerContainers.bags[bagIndex];
                    }

                    if (characterSwapContainerSlots->destContainerIndex == 0)
                    {
                        destContainer = &playerContainers.equipment;
                    }
                    else
                    {
                        u32 bagIndex = characterSwapContainerSlots->destContainerIndex - 1;
                        destContainer = &playerContainers.bags[bagIndex];
                    }

                    srcContainer->SwapItems(*destContainer, characterSwapContainerSlots->srcSlotIndex, characterSwapContainerSlots->destSlotIndex);
                }

                std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Container::BuildSwapSlots(resultMessage, characterSwapContainerSlots->srcContainerIndex, characterSwapContainerSlots->destContainerIndex, characterSwapContainerSlots->srcSlotIndex, characterSwapContainerSlots->destSlotIndex))
                    return;

                networkState.server->SendPacket(socketID, resultMessage);
            }
            else
            {
                // Send Swap Failed Command
            }
        });

        gameCommandQueue.Push(swapContainerSlotsCommand);
        return true;
    }

    bool HandleOnRequestSpellCast(Network::SocketID socketID, Network::Message& message)
    {
        u32 spellID = 0;

        if (!message.buffer->GetU32(spellID))
            return false;

        //entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        //Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();
        //
        //if (!networkState.socketIDToEntity.contains(socketID))
        //    return false;
        //
        //entt::entity socketEntity = networkState.socketIDToEntity[socketID];
        //if (!registry->valid(socketEntity))
        //    return false;
        //
        //u8 result = 0;
        //Components::TargetInfo* targetInfo = registry->try_get<Components::TargetInfo>(socketEntity);
        //
        //if (registry->all_of<Components::CastInfo>(socketEntity))
        //{
        //    result = 0x1;
        //}
        //else if (!targetInfo || !registry->valid(targetInfo->target))
        //{
        //    result = 0x2;
        //}
        //
        //if (result != 0x0)
        //{
        //    std::shared_ptr<Bytebuffer> castResultMessage = Bytebuffer::Borrow<64>();
        //    if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, 0.0f, 0.0f))
        //        return false;
        //
        //    networkState.server->SendPacket(socketID, castResultMessage);
        //    return true;
        //}
        //
        //Components::CastInfo& castInfo = registry->emplace<Components::CastInfo>(socketEntity);
        //
        //castInfo.target = targetInfo->target;
        //castInfo.castTime = 1.5f;
        //castInfo.duration = 0.0f;
        //
        //std::shared_ptr<Bytebuffer> castResultMessage = Bytebuffer::Borrow<64>();
        //if (!Util::MessageBuilder::Spell::BuildSpellCastResultMessage(castResultMessage, socketEntity, result, castInfo.castTime, castInfo.duration))
        //    return false;
        //
        //networkState.server->SendPacket(socketID, castResultMessage);
        //
        //// Send Grid Message
        //{
        //    std::shared_ptr<Bytebuffer> entityCastSpellMessage = Bytebuffer::Borrow<64>();
        //    if (!Util::MessageBuilder::Entity::BuildEntitySpellCastMessage(entityCastSpellMessage, socketEntity, castInfo.castTime, castInfo.duration))
        //        return false;
        //
        //    ECS::Util::Grid::SendToGrid(socketEntity, entityCastSpellMessage);
        //}

        return true;
    }

    void NetworkConnection::Init(entt::registry& registry)
    {
        entt::registry::context& ctx = registry.ctx();

        Singletons::NetworkState& networkState = ctx.emplace<Singletons::NetworkState>();

        // Setup NetServer
        {
            u16 port = 4000;
            networkState.server = std::make_unique<Network::Server>(port);
            networkState.gameMessageRouter = std::make_unique<Network::GameMessageRouter>();

            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_Connect,                  Network::GameMessageHandler(Network::ConnectionStatus::None,        0u, -1, &HandleOnConnected));
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Shared_Ping,                     Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnPing));

            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_SendCheatCommand,         Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnSendCheatCommand));
            
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Shared_EntityMove,               Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnEntityMove));
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Shared_EntityTargetUpdate,       Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnEntityTargetUpdate));

            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_ContainerSwapSlots,       Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnContainerSwapSlots));
            
            networkState.gameMessageRouter->SetMessageHandler(Network::GameOpcode::Client_LocalRequestSpellCast,    Network::GameMessageHandler(Network::ConnectionStatus::Connected,   0u, -1, &HandleOnRequestSpellCast));

            // Bind to IP/Port
            std::string ipAddress = "0.0.0.0";
            
            if (networkState.server->Start())
            {
                NC_LOG_INFO("Network : Listening on ({0}, {1})", ipAddress, port);
            }
        }
    }

    void NetworkConnection::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();

        //Singletons::GridSingleton& gridSingleton = ctx.get<Singletons::GridSingleton>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();
        Singletons::WorldState& worldState = ctx.get<Singletons::WorldState>();

        // Handle 'SocketConnectedEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketConnectedEvent>& connectedEvents = networkState.server->GetConnectedEvents();

            Network::SocketConnectedEvent connectedEvent;
            while (connectedEvents.try_dequeue(connectedEvent))
            {
                const Network::ConnectionInfo& connectionInfo = connectedEvent.connectionInfo;
                NC_LOG_INFO("Network : Client connected from (SocketID : {0}, \"{1}:{2}\")", static_cast<u32>(connectedEvent.socketID), connectionInfo.ipAddr, connectionInfo.port);

                networkState.activeSocketIDs.insert(connectedEvent.socketID);
            }
        }

        // Handle 'SocketDisconnectedEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketDisconnectedEvent>& disconnectedEvents = networkState.server->GetDisconnectedEvents();

            Network::SocketDisconnectedEvent disconnectedEvent;
            while (disconnectedEvents.try_dequeue(disconnectedEvent))
            {
                Network::SocketID socketID = disconnectedEvent.socketID;
                NC_LOG_INFO("Network : Client disconnected (SocketID : {0})", static_cast<u32>(socketID));

                networkState.socketIDRequestedToClose.erase(socketID);
                networkState.activeSocketIDs.erase(socketID);

                if (!networkState.socketIDToEntity.contains(socketID))
                    continue;

                entt::entity entity = networkState.socketIDToEntity[socketID];
                NC_ASSERT(registry.valid(entity), "Network : Attempting to cleanup Invalid Entity during DisconnectedEvent");

                Components::NetInfo& netInfo = registry.get<Components::NetInfo>(entity);
                Components::ObjectInfo& objectInfo = registry.get<Components::ObjectInfo>(entity);

                // Start Cleanup here
                {
                    networkState.socketIDToEntity.erase(socketID);

                    u64 characterID = objectInfo.guid.GetCounter();
                    networkState.characterIDToEntity.erase(characterID);
                    networkState.characterIDToSocketID.erase(characterID);

                    u32 mapID = 0; // TODO : Get from Transform
                    World& world = worldState.GetWorld(mapID);
                    world.guidToEntity.erase(objectInfo.guid);
                    world.RemovePlayer(objectInfo.guid);

                    registry.destroy(entity);
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
                //NC_LOG_INFO("Network : Message from (SocketID : {0}, Opcode : {1}, Size : {2})", static_cast<std::underlying_type<Network::SocketID>::type>(messageEvent.socketID), static_cast<std::underlying_type<Network::GameOpcode>::type>(messageHeader->opcode), messageHeader->size);

                if (!networkState.activeSocketIDs.contains(messageEvent.socketID))
                    continue;

                // Invoke MessageHandler here
                if (networkState.gameMessageRouter->CallHandler(messageEvent.socketID, messageEvent.message))
                    continue;

                // Failed to Call Handler, Close Socket
                {
                    if (!networkState.socketIDRequestedToClose.contains(messageEvent.socketID))
                    {
                        networkState.server->CloseSocketID(messageEvent.socketID);
                        networkState.socketIDRequestedToClose.emplace(messageEvent.socketID);
                    }
                }
            }
        }
    }
}