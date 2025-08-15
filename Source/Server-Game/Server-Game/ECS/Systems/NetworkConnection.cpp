#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Util/ContainerUtil.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/ECS/Util/Persistence/CharacterUtil.h"
#include "Server-Game/ECS/Util/Persistence/ItemUtil.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/Handlers/PacketEventHandler.h"
#include "Server-Game/Util/ServiceLocator.h"

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
            .name = charName
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

    bool HandleOnCheatDamage(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        //u32 damage = 0;
        //if (!message.buffer->GetU32(damage))
        //    return false;
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
    bool HandleOnCheatKill(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
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
    bool HandleOnCheatResurrect(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
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
    bool HandleOnCheatMorph(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 displayID = 0;
        if (!message.buffer->GetU32(displayID))
            return false;

        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayID(*registry, entity, displayInfo, displayID);

        return true;
    }
    bool HandleOnCheatDemorph(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);

        u32 nativeDisplayID = Util::Unit::GetDisplayIDFromRaceGender(displayInfo.unitRace, displayInfo.unitGender);
        Util::Unit::UpdateDisplayID(*registry, entity, displayInfo, nativeDisplayID);

        return true;
    }
    bool HandleOnCheatCreateCharacter(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        Result result = { 0 };
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
        {
            result.NameIsInvalid = 1;
        }
        else if (Util::Cache::CharacterExistsByNameHash(gameCache, charNameHash))
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

        u64 characterID;
        ECS::Result creationResult = Util::Persistence::Character::CharacterCreate(*registry, charName, 1, characterID);
        
        Result cheatCommandResult =
        {
            .NameIsInvalid = 0,
            .CharacterAlreadyExists = creationResult == ECS::Result::CharacterAlreadyExists,
            .DatabaseTransactionFailed = creationResult == ECS::Result::DatabaseError,
            .Unused = 0
        };

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&cheatCommandResult);

        std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Cheat::BuildCheatCreateCharacterResponse(resultMessage, cheatCommandResultVal))
            return false;

        networkState.server->SendPacket(socketID, resultMessage);
        return true;
    }
    bool HandleOnCheatDeleteCharacter(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        Result result = { 0 };
        u32 charNameHash = StringUtils::fnv1a_32(charName.c_str(), charName.length());

        u64 characterIDToDelete;
        bool characterExists = Util::Cache::GetCharacterIDByNameHash(gameCache, charNameHash, characterIDToDelete);

        if (!characterExists || !StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
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
        {
            Network::SocketID characterSocketID;
            if (Util::Network::GetSocketIDFromCharacterID(networkState, characterIDToDelete, characterSocketID))
            {
                networkState.server->CloseSocketID(characterSocketID);
            }
        }

        ECS::Result deletionResult = Util::Persistence::Character::CharacterDelete(*registry, characterIDToDelete);
        result.CharacterDoesNotExist = deletionResult == ECS::Result::CharacterNotFound;
        result.DatabaseTransactionFailed = deletionResult == ECS::Result::DatabaseError;

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&result);

        std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Cheat::BuildCheatDeleteCharacterResponse(resultMessage, cheatCommandResultVal))
            return false;

        networkState.server->SendPacket(socketID, resultMessage);

        return true;
    }

    bool HandleOnCheatSetRace(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitRace unitRace = GameDefine::UnitRace::None;
        if (!message.buffer->Get(unitRace))
            return false;

        if (unitRace == GameDefine::UnitRace::None || unitRace > GameDefine::UnitRace::Troll)
            return false;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetRace(*registry, characterID, unitRace) != ECS::Result::Success)
            return false;

        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayRace(*registry, entity, displayInfo, unitRace);

        return true;
    }
    bool HandleOnCheatSetGender(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitGender unitGender = GameDefine::UnitGender::None;
        if (!message.buffer->Get(unitGender))
            return false;

        if (unitGender == GameDefine::UnitGender::None || unitGender > GameDefine::UnitGender::Other)
            return false;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetGender(*registry, characterID, unitGender) != ECS::Result::Success)
            return false;

        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayGender(*registry, entity, displayInfo, unitGender);
        
        return true;
    }
    bool HandleOnCheatSetClass(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitClass unitClass = GameDefine::UnitClass::None;
        if (!message.buffer->Get(unitClass))
            return false;

        if (unitClass == GameDefine::UnitClass::None || unitClass > GameDefine::UnitClass::Druid)
            return false;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetClass(*registry, characterID, unitClass) != ECS::Result::Success)
            return false;

        return true;
    }
    bool HandleOnCheatSetLevel(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u16 level = 0;
        if (!message.buffer->Get(level))
            return false;

        if (level == 0)
            return false;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetLevel(*registry, characterID, level) != ECS::Result::Success)
            return false;

        return true;
    }

    bool HandleOnCheatSetItemTemplate(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemTemplate itemTemplate;
        if (!GameDefine::Database::ItemTemplate::Read(message.buffer, itemTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
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
                gameCache.itemTables.templateIDToTemplateDefinition[itemTemplate.id] = itemTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemStatTemplate(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemStatTemplate itemStatTemplate;
        if (!GameDefine::Database::ItemStatTemplate::Read(message.buffer, itemStatTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
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
                gameCache.itemTables.statTemplateIDToTemplateDefinition[itemStatTemplate.id] = itemStatTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemArmorTemplate(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemArmorTemplate itemArmorTemplate;
        if (!GameDefine::Database::ItemArmorTemplate::Read(message.buffer, itemArmorTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
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
                gameCache.itemTables.armorTemplateIDToTemplateDefinition[itemArmorTemplate.id] = itemArmorTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemWeaponTemplate(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemWeaponTemplate itemWeaponTemplate;
        if (!GameDefine::Database::ItemWeaponTemplate::Read(message.buffer, itemWeaponTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
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
                gameCache.itemTables.weaponTemplateIDToTemplateDefinition[itemWeaponTemplate.id] = itemWeaponTemplate;
            }
        }

        return true;
    }
    bool HandleOnCheatSetItemShieldTemplate(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemShieldTemplate itemShieldTemplate;
        if (!GameDefine::Database::ItemShieldTemplate::Read(message.buffer, itemShieldTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
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
                gameCache.itemTables.shieldTemplateIDToTemplateDefinition[itemShieldTemplate.id] = itemShieldTemplate;
            }
        }

        return true;
    }

    bool HandleOnCheatAddItem(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        bool isValidItemID = Util::Cache::ItemTemplateExistsByID(gameCache, itemID);
        bool playerHasBags = registry->all_of<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = registry->get<Components::PlayerContainers>(entity);

        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;
        bool canAddItem = Util::Container::GetFirstFreeSlotInBags(playerContainers, containerIndex, slotIndex);
        if (!canAddItem)
            return true;

        u64 characterID = registry->get<Components::ObjectInfo>(entity).guid.GetCounter();

        const auto& baseContainerItem = playerContainers.equipment.GetItem(containerIndex);

        u64 containerID = baseContainerItem.objectGuid.GetCounter();
        Database::Container& container = playerContainers.bags[containerIndex];

        u64 itemInstanceID;
        if (Util::Persistence::Character::ItemAdd(*registry, entity, characterID, itemID, container, containerID, slotIndex, itemInstanceID) == ECS::Result::Success)
        {
            Database::ItemInstance* itemInstance = nullptr;
            if (Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
            {
                GameDefine::ObjectGuid itemInstanceGuid = GameDefine::ObjectGuid::CreateItem(itemInstanceID);

                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                if (Util::MessageBuilder::Item::BuildItemCreate(buffer, itemInstanceGuid, *itemInstance))
                {
                    Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();
                    networkState.server->SendPacket(socketID, buffer);
                }
            }
        }

        return true;
    }
    bool HandleOnCheatRemoveItem(entt::registry* registry, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        bool isValidItemID = Util::Cache::ItemTemplateExistsByID(gameCache, itemID);
        bool playerHasBags = registry->all_of<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = registry->get<Components::PlayerContainers>(entity);
        Database::Container* container = nullptr;
        u64 containerID = 0;
        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;

        bool foundItem = Util::Container::GetFirstItemSlot(gameCache, playerContainers, itemID, container, containerID, containerIndex, slotIndex);
        if (!foundItem)
            return true;

        u64 characterID = registry->get<Components::ObjectInfo>(entity).guid.GetCounter();
        Util::Persistence::Character::ItemDelete(*registry, entity, characterID, *container, containerID, slotIndex);

        return true;
    }

    bool HandleOnSendCheatCommand(Network::SocketID socketID, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        Network::CheatCommands command = Network::CheatCommands::None;
        if (!message.buffer->Get(command))
            return false;

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;
        
        if (!registry->valid(entity))
            return false;

        switch (command)
        {
            case Network::CheatCommands::Damage:
            {
                return HandleOnCheatDamage(registry, socketID, entity, message);
            }

            case Network::CheatCommands::Kill:
            {
                return HandleOnCheatKill(registry, socketID, entity, message);
            }

            case Network::CheatCommands::Resurrect:
            {
                return HandleOnCheatResurrect(registry, socketID, entity, message);
            }

            case Network::CheatCommands::Morph:
            {
                return HandleOnCheatMorph(registry, socketID, entity, message);
            }

            case Network::CheatCommands::Demorph:
            {
                return HandleOnCheatDemorph(registry, socketID, entity, message);
            }

            case Network::CheatCommands::CreateCharacter:
            {
                return HandleOnCheatCreateCharacter(registry, socketID, entity, message);
            }

            case Network::CheatCommands::DeleteCharacter:
            {
                return HandleOnCheatDeleteCharacter(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetRace:
            {
                return HandleOnCheatSetRace(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetGender:
            {
                return HandleOnCheatSetGender(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetClass:
            {
                return HandleOnCheatSetClass(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetLevel:
            {
                return HandleOnCheatSetLevel(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetItemTemplate:
            {
                return HandleOnCheatSetItemTemplate(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetItemStatTemplate:
            {
                return HandleOnCheatSetItemStatTemplate(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetItemArmorTemplate:
            {
                return HandleOnCheatSetItemArmorTemplate(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetItemWeaponTemplate:
            {
                return HandleOnCheatSetItemWeaponTemplate(registry, socketID, entity, message);
            }

            case Network::CheatCommands::SetItemShieldTemplate:
            {
                return HandleOnCheatSetItemShieldTemplate(registry, socketID, entity, message);
            }

            case Network::CheatCommands::AddItem:
            {
                return HandleOnCheatAddItem(registry, socketID, entity, message);
            }

            case Network::CheatCommands::RemoveItem:
            {
                return HandleOnCheatRemoveItem(registry, socketID, entity, message);
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

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;

        Components::Transform& transform = registry->get<Components::Transform>(entity);
        transform.position = position;
        transform.rotation = rotation;

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);

        std::shared_ptr<Bytebuffer> entityMoveMessage = Bytebuffer::Borrow<64>();
        if (!Util::MessageBuilder::Entity::BuildEntityMoveMessage(entityMoveMessage, objectInfo.guid, position, rotation, movementFlags, verticalVelocity))
            return false;

        Singletons::WorldState& worldState = registry->ctx().get<Singletons::WorldState>();

        World& world = worldState.GetWorld(transform.mapID);
        world.UpdatePlayer(objectInfo.guid, position.x, position.z);

        auto& netInfo = registry->get<Components::NetInfo>(entity);
        ECS::Util::Grid::SendToNearby(*registry, entity, netInfo, entityMoveMessage);
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
        u16 srcContainerIndex = 0;
        u16 dstContainerIndex = 0;
        u16 srcSlotIndex = 0;
        u16 dstSlotIndex = 0;

        bool didFail = false;

        didFail |= !message.buffer->GetU16(srcContainerIndex);
        didFail |= !message.buffer->GetU16(dstContainerIndex);
        didFail |= !message.buffer->GetU16(srcSlotIndex);
        didFail |= !message.buffer->GetU16(dstSlotIndex);

        if (didFail)
            return false;

        if (srcContainerIndex == dstContainerIndex && srcSlotIndex == dstSlotIndex)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::NetworkState& networkState = registry->ctx().get<Singletons::NetworkState>();

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;

        if (!registry->valid(entity) || !registry->all_of<Components::PlayerContainers>(entity))
            return false;

        auto& playerContainers = registry->get<Components::PlayerContainers>(entity);

        Database::Container* srcContainer = nullptr;
        u64 srcContainerID = 0;

        Database::Container* dstContainer = nullptr;
        u64 dstContainerID = 0;
       

        bool isSameContainer = srcContainerIndex == dstContainerIndex;
        if (isSameContainer)
        {
            if (srcContainerIndex == 0)
            {
                srcContainer = &playerContainers.equipment;
                srcContainerID = 0;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + srcContainerIndex - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGuid.GetCounter();
            }

            u32 numContainerSlots = srcContainer->GetTotalSlots();
            if (srcSlotIndex >= numContainerSlots || dstSlotIndex >= numContainerSlots)
                return false;

            auto& srcItem = srcContainer->GetItem(srcSlotIndex);
            if (srcItem.IsEmpty())
                return false;

            dstContainer = srcContainer; // Same container, so destination is the same as source
            dstContainerID = srcContainerID;
        }
        else
        {
            if (srcContainerIndex == 0)
            {
                srcContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + srcContainerIndex - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGuid.GetCounter();
            }

            if (dstContainerIndex == 0)
            {
                dstContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + dstContainerIndex - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                dstContainer = &playerContainers.bags[bagIndex];
                dstContainerID = bagContainerItem.objectGuid.GetCounter();
            }

            if (srcSlotIndex >= srcContainer->GetTotalSlots() || dstSlotIndex >= dstContainer->GetTotalSlots())
                return false;

            auto& srcItem = srcContainer->GetItem(srcSlotIndex);
            if (srcItem.IsEmpty())
                return false;
        }

        auto& objectInfo = registry->get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::ItemSwap(*registry, entity, characterID, *srcContainer, srcContainerID, srcSlotIndex, *dstContainer, dstContainerID, dstSlotIndex) == ECS::Result::Success)
        {
            std::shared_ptr<Bytebuffer> resultMessage = Bytebuffer::Borrow<64>();
            if (!Util::MessageBuilder::Container::BuildSwapSlots(resultMessage, srcContainerIndex, dstContainerIndex, srcSlotIndex, dstSlotIndex))
                return false;

            networkState.server->SendPacket(socketID, resultMessage);
        }

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

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        // Handle 'SocketConnectedEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketConnectedEvent>& connectedEvents = networkState.server->GetConnectedEvents();

            Network::SocketConnectedEvent connectedEvent;
            while (connectedEvents.try_dequeue(connectedEvent))
            {
                const Network::ConnectionInfo& connectionInfo = connectedEvent.connectionInfo;
                NC_LOG_INFO("Network : Client connected from (SocketID : {0}, \"{1}:{2}\")", static_cast<u32>(connectedEvent.socketID), connectionInfo.ipAddr, connectionInfo.port);

                Util::Network::ActivateSocket(networkState, connectedEvent.socketID);
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

                Util::Network::DeactivateSocket(networkState, socketID);

                entt::entity entity;
                if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
                    continue;

                if (!registry.valid(entity))
                    continue;

                registry.emplace<Tags::CharacterNeedsDeinitialization>(entity);
            }
        }

        // Handle 'SocketMessageEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketMessageEvent>& messageEvents = networkState.server->GetMessageEvents();
            Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
            auto* packetEventHandler = luaManager->GetLuaHandler<Scripting::PacketEventHandler>(Scripting::LuaHandlerType::PacketEvent);
            bool hasOnReceivedListener = packetEventHandler->HasListeners(Scripting::LuaPacketEvent::OnReceive);

            Network::SocketMessageEvent messageEvent;
            while (messageEvents.try_dequeue(messageEvent))
            {
                if (!Util::Network::IsSocketActive(networkState, messageEvent.socketID))
                    continue;

                Network::MessageHeader messageHeader;
                if (networkState.gameMessageRouter->GetMessageHeader(messageEvent.message, messageHeader))
                {
                    auto gameOpcode = static_cast<Network::GameOpcode>(messageHeader.opcode);

                    bool hasLuaHandlerForOpcode = packetEventHandler->HasGameOpcodeHandler(luaManager->GetInternalState(), gameOpcode);
                    if (hasLuaHandlerForOpcode)
                    {
                        u32 messageReadOffset = static_cast<u32>(messageEvent.message.buffer->readData);
                        if (packetEventHandler->CallGameOpcodeHandler(luaManager->GetInternalState(), messageHeader, messageEvent.message))
                        {
                            if (!networkState.gameMessageRouter->HasValidHandlerForHeader(messageHeader))
                                continue;

                            messageEvent.message.buffer->readData = messageReadOffset;

                            if (networkState.gameMessageRouter->CallHandler(messageEvent.socketID, messageHeader, messageEvent.message))
                                continue;
                        }
                    }
                    else
                    {
                        if (networkState.gameMessageRouter->HasValidHandlerForHeader(messageHeader))
                        {
                            if (networkState.gameMessageRouter->CallHandler(messageEvent.socketID, messageHeader, messageEvent.message))
                                continue;
                        }
                    }
                }

                // Failed to Call Handler, Close Socket
                {
                    Util::Network::RequestSocketToClose(networkState, messageEvent.socketID);
                }
            }
        }

        networkState.server->Update();
    }
}