#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/CreatureInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/UnitVisualItems.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/ProximityTriggers.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CollisionUtil.h"
#include "Server-Game/ECS/Util/ContainerUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/ProximityTriggerUtil.h"
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
#include <Server-Common/Database/Util/MapUtils.h>
#include <Server-Common/Database/Util/ProximityTriggerUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Gameplay/GameDefine.h>
#include <Gameplay/Network/GameMessageRouter.h>

#include <Meta/Generated/Shared/NetworkEnum.h>
#include <Meta/Generated/Shared/NetworkPacket.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

#include <chrono>

namespace ECS::Systems
{
    bool HandleOnCheatCharacterAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

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
            Util::Network::SendPacket(networkState, socketID, Generated::CheatCommandResultPacket{
                .command = static_cast<u8>(Generated::CheatCommandEnum::CharacterAdd),
                .result = resultAsValue,
                .response = "Unknown"
                });
            return true;
        }

        u64 characterID;
        ECS::Result creationResult = Util::Persistence::Character::CharacterCreate(gameCache, charName, 1, characterID);

        Result cheatCommandResult =
        {
            .NameIsInvalid = 0,
            .CharacterAlreadyExists = creationResult == ECS::Result::CharacterAlreadyExists,
            .DatabaseTransactionFailed = creationResult == ECS::Result::DatabaseError,
            .Unused = 0
        };

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&cheatCommandResult);

        Util::Network::SendPacket(networkState, socketID, Generated::CheatCommandResultPacket{
            .command = static_cast<u8>(Generated::CheatCommandEnum::CharacterAdd),
            .result = cheatCommandResultVal,
            .response = "Unknown"
            });
        return true;
    }
    bool HandleOnCheatCharacterRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

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

            Util::Network::SendPacket(networkState, socketID, Generated::CheatCommandResultPacket{
                .command = static_cast<u8>(Generated::CheatCommandEnum::CharacterRemove),
                .result = resultAsValue,
                .response = "Unknown"
                });
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

        ECS::Result deletionResult = Util::Persistence::Character::CharacterDelete(gameCache, characterIDToDelete);
        result.CharacterDoesNotExist = deletionResult == ECS::Result::CharacterNotFound;
        result.DatabaseTransactionFailed = deletionResult == ECS::Result::DatabaseError;

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&result);

        Util::Network::SendPacket(networkState, socketID, Generated::CheatCommandResultPacket{
            .command = static_cast<u8>(Generated::CheatCommandEnum::CharacterRemove),
            .result = cheatCommandResultVal,
            .response = "Unknown"
            });

        return true;
    }

    bool HandleOnCheatCreaureAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 creatureTemplateID = 0;
        if (!message.buffer->GetU32(creatureTemplateID))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
        if (!Util::Cache::GetCreatureTemplateByID(gameCache, creatureTemplateID, creatureTemplate))
            return true;

        auto& playerTransform = world.Get<Components::Transform>(entity);

        entt::entity creaureEntity = world.CreateEntity();
        world.EmplaceOrReplace<Events::CreatureCreate>(creaureEntity, creatureTemplateID, creatureTemplate->displayID, playerTransform.mapID, playerTransform.position, playerTransform.pitchYaw.y);

        return true;
    }
    bool HandleOnCheatCreaureRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID creaureGUID;
        if (!message.buffer->Deserialize(creaureGUID))
            return false;

        entt::entity creaureEntity = world.GetEntity(creaureGUID);
        if (creaureEntity == entt::null)
            return true;

        world.EmplaceOrReplace<Events::CreatureNeedsDeinitialization>(creaureEntity, creaureGUID);
        return true;
    }

    bool HandleOnCheatDamage(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 damage = 0;
        if (!message.buffer->GetU32(damage))
            return false;

        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                targetEntity = targetInfo->target;
            }
        }

        if (auto* unitStatsComponent = world.TryGet<Components::UnitStatsComponent>(targetEntity))
        {
            if (unitStatsComponent->currentHealth <= 0)
                return true;

            f32 damageToDeal = glm::min(static_cast<f32>(damage), unitStatsComponent->currentHealth);

            unitStatsComponent->currentHealth -= damageToDeal;
            unitStatsComponent->healthIsDirty = true;

            // Send Grid Message
            {
                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);
                auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
                auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

                std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, srcObjectInfo.guid, targetObjectInfo.guid, damageToDeal))
                    return false;

                Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, damageDealtMessage);
            }
        }

        return true;
    }
    bool HandleOnCheatKill(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
                targetEntity = targetInfo->target;
        }

        if (auto* unitStatsComponent = world.TryGet<Components::UnitStatsComponent>(targetEntity))
        {
            if (unitStatsComponent->currentHealth <= 0.0f)
                return true;

            f32 damageToDeal = unitStatsComponent->currentHealth;

            unitStatsComponent->currentHealth = 0.0f;
            unitStatsComponent->healthIsDirty = true;

            // Send Grid Message
            {
                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);
                auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
                auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

                std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, srcObjectInfo.guid, targetObjectInfo.guid, damageToDeal))
                    return false;

                Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, damageDealtMessage);
            }
        }

        return true;
    }
    bool HandleOnCheatResurrect(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                targetEntity = targetInfo->target;
            }
        }

        if (auto* unitStatsComponent = world.TryGet<Components::UnitStatsComponent>(targetEntity))
        {
            unitStatsComponent->currentHealth = unitStatsComponent->maxHealth;
            unitStatsComponent->healthIsDirty = true;

            // Send Grid Message
            {
                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);
                auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
                auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);
                std::shared_ptr<Bytebuffer> resurrectedMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildResurrectedMessage(resurrectedMessage, srcObjectInfo.guid, targetObjectInfo.guid))
                    return false;

                Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, resurrectedMessage);
            }
        }

        return true;
    }
    bool HandleOnCheatMorph(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 displayID = 0;
        if (!message.buffer->GetU32(displayID))
            return false;

        auto& displayInfo = world.Get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayID(*world.registry, entity, displayInfo, displayID);

        return true;
    }
    bool HandleOnCheatDemorph(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& displayInfo = registry->get<Components::DisplayInfo>(entity);

        u32 nativeDisplayID = Util::Unit::GetDisplayIDFromRaceGender(displayInfo.unitRace, displayInfo.unitGender);
        Util::Unit::UpdateDisplayID(*world.registry, entity, displayInfo, nativeDisplayID);

        return true;
    }
    bool HandleOnCheatUnitSetRace(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitRace unitRace = GameDefine::UnitRace::None;
        if (!message.buffer->Get(unitRace))
            return false;

        if (unitRace == GameDefine::UnitRace::None || unitRace > GameDefine::UnitRace::Troll)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetRace(gameCache, *registry, characterID, unitRace) != ECS::Result::Success)
            return false;

        auto& displayInfo = world.Get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayRace(*world.registry, entity, displayInfo, unitRace);

        return true;
    }
    bool HandleOnCheatUnitSetGender(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitGender unitGender = GameDefine::UnitGender::None;
        if (!message.buffer->Get(unitGender))
            return false;

        if (unitGender == GameDefine::UnitGender::None || unitGender > GameDefine::UnitGender::Other)
            return false;

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetGender(gameCache, *world.registry, characterID, unitGender) != ECS::Result::Success)
            return false;

        auto& displayInfo = world.Get<Components::DisplayInfo>(entity);
        Util::Unit::UpdateDisplayGender(*world.registry, entity, displayInfo, unitGender);
        
        return true;
    }
    bool HandleOnCheatUnitSetClass(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitClass unitClass = GameDefine::UnitClass::None;
        if (!message.buffer->Get(unitClass))
            return false;

        if (unitClass == GameDefine::UnitClass::None || unitClass > GameDefine::UnitClass::Druid)
            return false;

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetClass(gameCache, *world.registry, characterID, unitClass) != ECS::Result::Success)
            return false;

        return true;
    }
    bool HandleOnCheatUnitSetLevel(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u16 level = 0;
        if (!message.buffer->Get(level))
            return false;

        if (level == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetLevel(gameCache, *world.registry, characterID, level) != ECS::Result::Success)
            return false;

        return true;
    }

    bool HandleOnCheatItemAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
        auto& networkState = ctx.get<Singletons::NetworkState>();

        bool isValidItemID = Util::Cache::ItemTemplateExistsByID(gameCache, itemID);
        bool playerHasBags = world.AllOf<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = world.Get<Components::PlayerContainers>(entity);

        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;
        bool canAddItem = Util::Container::GetFirstFreeSlotInBags(playerContainers, containerIndex, slotIndex);
        if (!canAddItem)
            return true;

        u64 characterID = world.Get<Components::ObjectInfo>(entity).guid.GetCounter();

        const auto& baseContainerItem = playerContainers.equipment.GetItem(containerIndex);

        u64 containerID = baseContainerItem.objectGUID.GetCounter();
        Database::Container& container = playerContainers.bags[containerIndex];

        u64 itemInstanceID;
        if (Util::Persistence::Character::ItemAdd(gameCache, *world.registry, entity, characterID, itemID, container, containerID, slotIndex, itemInstanceID) == ECS::Result::Success)
        {
            Database::ItemInstance* itemInstance = nullptr;
            if (Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
            {
                ObjectGUID itemInstanceGUID = ObjectGUID::CreateItem(itemInstanceID);
                Util::Network::SendPacket(networkState, socketID, Generated::ItemAddPacket{
                    .guid = itemInstanceGUID,
                    .itemID = itemInstance->itemID,
                    .count = itemInstance->count,
                    .durability = itemInstance->durability
                    });
            }
        }

        return true;
    }
    bool HandleOnCheatItemRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
        bool playerHasBags = world.AllOf<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = world.Get<Components::PlayerContainers>(entity);
        Database::Container* container = nullptr;
        u64 containerID = 0;
        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;

        bool foundItem = Util::Container::GetFirstItemSlot(gameCache, playerContainers, itemID, container, containerID, containerIndex, slotIndex);
        if (!foundItem)
            return true;

        u64 characterID = world.Get<Components::ObjectInfo>(entity).guid.GetCounter();
        Util::Persistence::Character::ItemDelete(gameCache, *world.registry, entity, characterID, *container, containerID, slotIndex);

        return true;
    }
    bool HandleOnCheatItemSetTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
    bool HandleOnCheatSetItemStatTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
    bool HandleOnCheatSetItemArmorTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
    bool HandleOnCheatSetItemWeaponTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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
    bool HandleOnCheatSetItemShieldTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
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

    bool HandleOnCheatMapAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::Map map;
        if (!GameDefine::Database::Map::Read(message.buffer, map))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            auto queryResult = transaction.exec(pqxx::prepped("SetMap"), pqxx::params{ map.id, map.flags, map.name, map.type, map.maxPlayers });
            if (queryResult.affected_rows() == 0)
            {
                transaction.abort();
            }
            else
            {
                transaction.commit();
                gameCache.mapTables.idToDefinition[map.id] = map;
            }
        }

        return true;
    }
    bool HandleOnCheatGotoAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        u32 mapID = 0;
        vec3 position = vec3(0.0f);
        f32 orientation = 0.0f;

        if (!message.buffer->GetString(locationName))
            return false;

        if (!message.buffer->GetU32(mapID))
            return false;

        if (!message.buffer->Get(position))
            return false;

        if (!message.buffer->GetF32(orientation))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();
            u32 locationID;
            if (Database::Util::Map::MapLocationCreate(transaction, locationName, mapID, position, orientation, locationID))
            {
                transaction.commit();
                gameCache.mapTables.locationIDToDefinition[locationID] = GameDefine::Database::MapLocation{ locationID, locationName, mapID, position.x, position.y, position.z, orientation };
                gameCache.mapTables.locationNameHashToID[locationNameHash] = locationID;
            }
        }

        return true;
    }
    bool HandleOnCheatGotoAddHere(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        auto& playerTransform = world.Get<Components::Transform>(entity);
        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();

            u32 locationID;
            if (Database::Util::Map::MapLocationCreate(transaction, locationName, playerTransform.mapID, playerTransform.position, playerTransform.pitchYaw.y, locationID))
            {
                transaction.commit();
                gameCache.mapTables.locationIDToDefinition[locationID] = GameDefine::Database::MapLocation{ locationID, locationName, playerTransform.mapID, playerTransform.position.x, playerTransform.position.y, playerTransform.position.z, playerTransform.pitchYaw.y };
                gameCache.mapTables.locationNameHashToID[locationNameHash] = locationID;
            }
        }

        return true;
    }
    bool HandleOnCheatGotoRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || !gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        if (auto conn = gameCache.dbController->GetConnection(Database::DBType::Character))
        {
            auto transaction = conn->NewTransaction();

            u32 locationID = gameCache.mapTables.locationNameHashToID[locationNameHash];
            if (Database::Util::Map::MapLocationDelete(transaction, locationID))
            {
                transaction.commit();
                gameCache.mapTables.locationIDToDefinition.erase(locationID);
                gameCache.mapTables.locationNameHashToID.erase(locationNameHash);
            }
        }

        return true;
    }
    bool HandleOnCheatGotoMap(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 mapID = 0;
        if (!message.buffer->GetU32(mapID))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        GameDefine::Database::Map* mapDefinition = nullptr;
        if (!Util::Cache::GetMapByID(gameCache, mapID, mapDefinition))
            return true;

        std::string locationName = mapDefinition->name;
        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || !gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        u32 locationID = gameCache.mapTables.locationNameHashToID[locationNameHash];
        //Util::World::TeleportToLocation(*registry, world, entity, locationID);

        return true;
    }
    bool HandleOnCheatGotoLocation(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

        if (locationName.length() < 2 || locationName[0] == ' ' || !gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& transform = world.Get<Components::Transform>(entity);
        auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);

        u32 locationID = gameCache.mapTables.locationNameHashToID[locationNameHash];
        auto& location = gameCache.mapTables.locationIDToDefinition[locationID];
        vec3 position = vec3(location.positionX, location.positionY, location.positionZ);

        Util::Unit::TeleportToLocation(worldState, world, gameCache, networkState, entity, objectInfo, transform, visibilityInfo, location.mapID, position, location.orientation);

        return true;
    }
    bool HandleOnCheatGotoXYZ(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        vec3 position = vec3(0.0f);
        if (!message.buffer->Get(position))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& transform = world.Get<Components::Transform>(entity);
        auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);

        Util::Unit::TeleportToXYZ(world, networkState, entity, objectInfo, transform, visibilityInfo, position, transform.pitchYaw.y);
        return true;
    }

    bool HandleOnCheatTriggerAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string name;
        u16 flags;
        u16 mapID;
        vec3 position;
        vec3 extents;

        if (!message.buffer->GetString(name))
            return false;

        if (!message.buffer->GetU16(flags))
            return false;

        if (!message.buffer->GetU16(mapID))
            return false;

        if (!message.buffer->Get(position))
            return false;

        if (!message.buffer->Get(extents))
            return false;

        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        entt::entity playerEntity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, playerEntity))
            return false;

        if (!world.ValidEntity(playerEntity))
            return false;

        const auto& playerTransform = world.Get<Components::Transform>(playerEntity);
        if (playerTransform.mapID != mapID)
            return false;

        entt::entity triggerEntity = world.CreateEntity();

        Events::ProximityTriggerCreate event =
        {
            .name = name,
            .flags = static_cast<Generated::ProximityTriggerFlagEnum>(flags),

            .mapID = mapID,
            .position = position,
            .extents = extents
        };

        world.Emplace<Events::ProximityTriggerCreate>(triggerEntity, event);

        return true;
    }
    bool HandleOnCheatTriggerRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 triggerID;

        if (!message.buffer->GetU32(triggerID))
            return false;

        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        entt::entity triggerEntity;
        if (!Util::ProximityTrigger::ProximityTriggerGetByID(proximityTriggers, triggerID, triggerEntity))
            return true;

        world.EmplaceOrReplace<Events::ProximityTriggerNeedsDeinitialization>(triggerEntity, triggerID);

        return true;
    }

    bool HandleOnSendCheatCommand(Network::SocketID socketID, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        
        auto command = Generated::CheatCommandEnum::None;
        if (!message.buffer->Get(command))
            return false;

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;
        
        if (!world->ValidEntity(entity))
            return false;

        switch (command)
        {
            case Generated::CheatCommandEnum::CharacterAdd:
            {
                return HandleOnCheatCharacterAdd(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::CharacterRemove:
            {
                return HandleOnCheatCharacterRemove(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::CreatureAdd:
            {
                return HandleOnCheatCreaureAdd(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::CreatureRemove:
            {
                return HandleOnCheatCreaureRemove(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::Damage:
            {
                return HandleOnCheatDamage(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::Kill:
            {
                return HandleOnCheatKill(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::Resurrect:
            {
                return HandleOnCheatResurrect(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitMorph:
            {
                return HandleOnCheatMorph(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitDemorph:
            {
                return HandleOnCheatDemorph(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitSetRace:
            {
                return HandleOnCheatUnitSetRace(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitSetGender:
            {
                return HandleOnCheatUnitSetGender(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitSetClass:
            {
                return HandleOnCheatUnitSetClass(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::UnitSetLevel:
            {
                return HandleOnCheatUnitSetLevel(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::ItemAdd:
            {
                return HandleOnCheatItemAdd(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemRemove:
            {
                return HandleOnCheatItemRemove(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemSetTemplate:
            {
                return HandleOnCheatItemSetTemplate(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemSetStatTemplate:
            {
                return HandleOnCheatSetItemStatTemplate(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemSetArmorTemplate:
            {
                return HandleOnCheatSetItemArmorTemplate(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemSetWeaponTemplate:
            {
                return HandleOnCheatSetItemWeaponTemplate(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::ItemSetShieldTemplate:
            {
                return HandleOnCheatSetItemShieldTemplate(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::MapAdd:
            {
                return HandleOnCheatMapAdd(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::GotoAddHere:
            {
                return HandleOnCheatGotoAddHere(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::GotoRemove:
            {
                return HandleOnCheatGotoRemove(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::GotoMap:
            {
                return HandleOnCheatGotoMap(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::GotoLocation:
            {
                return HandleOnCheatGotoLocation(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::GotoXYZ:
            {
                return HandleOnCheatGotoXYZ(registry, *world, socketID, entity, message);
            }

            case Generated::CheatCommandEnum::TriggerAdd:
            {
                return HandleOnCheatTriggerAdd(registry, *world, socketID, entity, message);
            }
            case Generated::CheatCommandEnum::TriggerRemove:
            {
                return HandleOnCheatTriggerRemove(registry, *world, socketID, entity, message);
            }

            default: break;
        }

        return true;
    }

    bool HandleOnConnect(Network::SocketID socketID, Generated::ConnectPacket& packet)
    {
        if (!StringUtils::StringIsAlphaAndAtLeastLength(packet.characterName, 2))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();

        CharacterLoginRequest characterLoginRequest =
        {
            .socketID = socketID,
            .name = packet.characterName
        };
        networkState.characterLoginRequest.enqueue(characterLoginRequest);

        return true;
    }
    bool HandleOnPing(Network::SocketID socketID, Generated::PingPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& timeState = ctx.get<Singletons::TimeState>();

        entt::entity socketEntity = entt::null;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, socketEntity))
            return false;

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        auto& netInfo = world->Get<Components::NetInfo>(socketEntity);
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

        netInfo.ping = packet.ping;
        netInfo.numEarlyPings = 0;
        netInfo.numLatePings = 0;
        netInfo.lastPingTime = currentTime;

        Util::Network::SendPacket(networkState, socketID, Generated::ServerUpdateStatsPacket{
            .serverTickTime = serverDiff
        });

        return true;
    }

    bool HandleOnClientUnitTargetUpdate(Network::SocketID socketID, Generated::ClientUnitTargetUpdatePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& worldState = registry->ctx().get<Singletons::WorldState>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        entt::entity socketEntity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, socketEntity) ||
            !world->ValidEntity(socketEntity))
            return false;

        entt::entity targetEntity = world->GetEntity(packet.targetGUID);
        if (!world->ValidEntity(targetEntity))
            return true;

        auto& objectInfo = world->Get<Components::ObjectInfo>(targetEntity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(targetEntity);

        auto& targetInfo = world->Get<Components::TargetInfo>(socketEntity);
        targetInfo.target = targetEntity;

        ECS::Util::Network::SendToNearby(networkState, *world, socketEntity, visibilityInfo, false, Generated::ServerUnitTargetUpdatePacket{
            .guid = objectInfo.guid,
            .targetGUID = packet.targetGUID
        });

        return true;
    }
    bool HandleOnClientUnitMove(Network::SocketID socketID, Generated::ClientUnitMovePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& transform = world->Get<Components::Transform>(entity);
        transform.position = packet.position;
        transform.pitchYaw = packet.pitchYaw;

        world->playerVisData.Update(objectInfo.guid, packet.position.x, packet.position.z);

        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, false, Generated::ServerUnitMovePacket{
            .guid = objectInfo.guid,
            .movementFlags = packet.movementFlags,
            .position = packet.position,
            .pitchYaw = packet.pitchYaw,
            .verticalVelocity = packet.verticalVelocity
        });

        return true;
    }
    bool HandleOnClientContainerSwapSlots(Network::SocketID socketID, Generated::ClientContainerSwapSlotsPacket& packet)
    {
        if (packet.srcContainer == packet.dstContainer && packet.srcSlot == packet.dstSlot)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        entt::entity entity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, entity))
            return false;

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        if (!world->ValidEntity(entity) || !world->AllOf<Components::PlayerContainers>(entity))
            return false;

        auto& playerContainers = world->Get<Components::PlayerContainers>(entity);

        Database::Container* srcContainer = nullptr;
        u64 srcContainerID = 0;

        Database::Container* dstContainer = nullptr;
        u64 dstContainerID = 0;

        bool isSameContainer = packet.srcContainer == packet.dstContainer;
        if (isSameContainer)
        {
            if (packet.srcContainer == 0)
            {
                srcContainer = &playerContainers.equipment;
                srcContainerID = 0;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.srcContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            u32 numContainerSlots = srcContainer->GetTotalSlots();
            if (packet.srcSlot >= numContainerSlots || packet.dstSlot >= numContainerSlots)
                return false;

            auto& srcItem = srcContainer->GetItem(packet.srcSlot);
            if (srcItem.IsEmpty())
                return false;
        }
        else
        {
            if (packet.srcContainer == 0)
            {
                srcContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.srcContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            if (packet.dstContainer == 0)
            {
                dstContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.dstContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                dstContainer = &playerContainers.bags[bagIndex];
                dstContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            if (packet.srcSlot >= srcContainer->GetTotalSlots() || packet.dstSlot >= dstContainer->GetTotalSlots())
                return false;

            auto& srcItem = srcContainer->GetItem(packet.srcSlot);
            if (srcItem.IsEmpty())
                return false;
        }

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        auto& visualItems = world->Get<Components::UnitVisualItems>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::ItemSwap(gameCache, characterID, *srcContainer, srcContainerID, packet.srcSlot, *dstContainer, dstContainerID, packet.dstSlot) == ECS::Result::Success)
        {
            Util::Network::SendPacket(networkState, socketID, Generated::ServerContainerSwapSlotsPacket{
                .srcContainer = packet.srcContainer,
                .dstContainer = packet.dstContainer,
                .srcSlot = packet.srcSlot,
                .dstSlot = packet.dstSlot
            });

            if (srcContainerID == 0)
            {
                const Database::ContainerItem& containerItem = srcContainer->GetItem(packet.srcSlot);
                bool isContainerItemEmpty = containerItem.IsEmpty();
                u32 itemID = 0;

                if (!isContainerItemEmpty)
                {
                    Database::ItemInstance* itemInstance = nullptr;
                    if (Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                        itemID = itemInstance->itemID;
                }

                ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, true, Generated::ServerUnitEquippedItemUpdatePacket{
                    .guid = objectInfo.guid,
                    .slot = static_cast<u8>(packet.srcSlot),
                    .itemID = itemID
                });

                visualItems.equippedItemIDs[packet.srcSlot] = itemID;
                visualItems.dirtyItemIDs.insert(packet.srcSlot);
                world->EmplaceOrReplace<Events::CharacterNeedsVisualItemUpdate>(entity);
            }

            if (dstContainerID == 0)
            {
                const Database::ContainerItem& containerItem = dstContainer->GetItem(packet.dstSlot);
                bool isContainerItemEmpty = containerItem.IsEmpty();
                u32 itemID = 0;

                if (!isContainerItemEmpty)
                {
                    Database::ItemInstance* itemInstance = nullptr;
                    if (Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                        itemID = itemInstance->itemID;
                }

                ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, true, Generated::ServerUnitEquippedItemUpdatePacket{
                    .guid = objectInfo.guid,
                    .slot = static_cast<u8>(packet.dstSlot),
                    .itemID = itemID
                });

                visualItems.equippedItemIDs[packet.dstSlot] = itemID;
                visualItems.dirtyItemIDs.insert(packet.dstSlot);
                world->EmplaceOrReplace<Events::CharacterNeedsVisualItemUpdate>(entity);
            }
        }

        return true;
    }

    bool HandleOnClientTriggerEnter(Network::SocketID socketID, Generated::ClientTriggerEnterPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        auto& proximityTriggers = world->GetSingleton<Singletons::ProximityTriggers>();

        entt::entity playerEntity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, playerEntity))
            return false;

        entt::entity triggerEntity;
        if (!Util::ProximityTrigger::ProximityTriggerGetByID(proximityTriggers, packet.triggerID, triggerEntity))
            return true;

        auto& proximityTrigger = world->Get<Components::ProximityTrigger>(triggerEntity);

        // Only process if the server is authoritative for this trigger
        bool isServerAuthoritative = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerAuthorative) != Generated::ProximityTriggerFlagEnum::None;
        if (!isServerAuthoritative)
            return false;

        auto& playerTransform = world->Get<Components::Transform>(playerEntity);
        auto& playerAABB = world->Get<Components::AABB>(playerEntity);
        auto& triggerTransform = world->Get<Components::Transform>(triggerEntity);
        auto& triggerAABB = world->Get<Components::AABB>(triggerEntity);

        if (!Util::Collision::Overlaps(playerTransform, playerAABB, triggerTransform, triggerAABB))
            return true;

        Util::ProximityTrigger::AddPlayerToTriggerEntered(*world, proximityTrigger, triggerEntity, playerEntity);

        return true;
    }
    bool HandleOnClientSendChatMessage(Network::SocketID socketID, Generated::ClientSendChatMessagePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;

        entt::entity socketEntity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, socketEntity))
            return false;

        if (!world->ValidEntity(socketEntity))
            return false;

        auto& objectInfo = world->Get<Components::ObjectInfo>(socketEntity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(socketEntity);
        ECS::Util::Network::SendToNearby(networkState, *world, socketEntity, visibilityInfo, true, Generated::ServerSendChatMessagePacket{
            .guid = objectInfo.guid,
            .message = packet.message
            });

        return true;
    }
    bool HandleOnClientSpellCast(Network::SocketID socketID, Generated::ClientSpellCastPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& worldState = registry->ctx().get<Singletons::WorldState>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        World* world = nullptr;
        if (!worldState.GetWorldFromSocket(socketID, world))
            return false;
        entt::entity socketEntity;
        if (!Util::Network::GetEntityIDFromSocketID(networkState, socketID, socketEntity))
            return false;
        if (!world->ValidEntity(socketEntity))
            return false;

        u8 result = 0;
        auto* targetInfo = world->TryGet<Components::TargetInfo>(socketEntity);

        if (world->AllOf<Components::CastInfo>(socketEntity))
        {
            result = 0x1;
        }
        else if (!targetInfo || !world->ValidEntity(targetInfo->target))
        {
            result = 0x2;
        }

        if (result != 0x0)
        {
            Util::Network::SendPacket(networkState, socketID, Generated::ServerSpellCastResultPacket{
                .result = result,
            });

            return true;
        }

        auto& casterObjectInfo = world->Get<Components::ObjectInfo>(socketEntity);
        auto& targetObjectInfo = world->Get<Components::ObjectInfo>(targetInfo->target);
        auto& castInfo = world->Emplace<Components::CastInfo>(socketEntity);

        castInfo.caster = casterObjectInfo.guid;
        castInfo.target = targetObjectInfo.guid;
        castInfo.castTime = 1.5f;
        castInfo.duration = 0.0f;

        Util::Network::SendPacket(networkState, socketID, Generated::ServerSpellCastResultPacket{
            .result = result,
        });

        // Send Grid Message
        {
            auto& visibilityInfo = world->Get<Components::VisibilityInfo>(socketEntity);
            ECS::Util::Network::SendToNearby(networkState, *world, socketEntity, visibilityInfo, false, Generated::UnitCastSpellPacket{
                .guid = casterObjectInfo.guid,
                .castTime = castInfo.castTime,
                .castDuration = castInfo.duration
            });
        }

        return true;
    }

    void NetworkConnection::Init(entt::registry& registry)
    {
        entt::registry::context& ctx = registry.ctx();

        auto& networkState = ctx.emplace<Singletons::NetworkState>();

        // Setup NetServer
        {
            u16 port = 4000;
            networkState.server = std::make_unique<Network::Server>(port);
            networkState.gameMessageRouter = std::make_unique<Network::GameMessageRouter>();

            networkState.gameMessageRouter->SetMessageHandler(Generated::SendCheatCommandPacket::PACKET_ID, Network::GameMessageHandler(Network::ConnectionStatus::Connected, 0u, -1, &HandleOnSendCheatCommand));

            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::None, HandleOnConnect);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnPing);

            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientUnitTargetUpdate);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientUnitMove);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientContainerSwapSlots);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientTriggerEnter);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientSendChatMessage);
            networkState.gameMessageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientSpellCast);

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

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();

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

                u16 mapID;
                if (!worldState.GetMapIDFromSocket(socketID, mapID))
                    continue;

                worldState.ClearMapIDForSocket(socketID);
                World& world = worldState.GetWorld(mapID);

                if (!world.ValidEntity(entity))
                    continue;

                world.EmplaceOrReplace<Events::CharacterNeedsDeinitialization>(entity);
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
                    bool hasLuaHandlerForOpcode = packetEventHandler->HasGameOpcodeHandler(luaManager->GetInternalState(), messageHeader.opcode);
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