#include "CharacterUtil.h"
#include "ItemUtil.h"

#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/StringUtils.h>

#include <entt/entt.hpp>

namespace ECS::Util::Persistence::Character
{
    Result CharacterCreate(Singletons::GameCache& gameCache, const std::string& name, u16 raceGenderClass, u64& characterID)
    {
        characterID = std::numeric_limits<u64>::max();

        u32 charNameHash = StringUtils::fnv1a_32(name.c_str(), name.length());

        if (Util::Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterAlreadyExists;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();
        bool result = ::Database::Util::Character::CharacterCreate(transaction, name, raceGenderClass, characterID);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();
        return Result::Success;
    }

    Result CharacterDelete(Singletons::GameCache& gameCache, u64 characterID)
    {
        if (!Util::Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();
        bool result = ::Database::Util::Character::CharacterDelete(transaction, characterID);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();
        return Result::Success;
    }

    ECS::Result CharacterSetRace(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitRace unitRace)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(unitRace) | (static_cast<u16>(displayInfo.unitRace) << 7) | (static_cast<u16>(characterInfo.unitClass) << 9);
        bool result = Database::Util::Character::CharacterSetRaceGenderClass(transaction, characterID, raceGenderClass);
        if (!result)
        {
            return Result::DatabaseError;
        }

        transaction.commit();

        displayInfo.unitRace = unitRace;
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetGender(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitGender unitGender)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(displayInfo.unitRace) | (static_cast<u16>(unitGender) << 7) | (static_cast<u16>(characterInfo.unitClass) << 9);
        bool result = Database::Util::Character::CharacterSetRaceGenderClass(transaction, characterID, raceGenderClass);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();

        displayInfo.unitGender = unitGender;
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetClass(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitClass unitClass)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(displayInfo.unitRace) | (static_cast<u16>(displayInfo.unitGender) << 7) | (static_cast<u16>(unitClass) << 9);
        bool result = Database::Util::Character::CharacterSetRaceGenderClass(transaction, characterID, raceGenderClass);
        if (!result)
        {
            return Result::DatabaseError;
        }

        transaction.commit();

        Util::Cache::CharacterSetClass(characterInfo, unitClass);
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetLevel(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, u16 level)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        bool result = Database::Util::Character::CharacterSetLevel(transaction, characterID, level);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        Util::Cache::CharacterSetLevel(characterInfo, level);
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetMapID(Singletons::GameCache& gameCache, u64 characterID, u32 mapID)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        bool result = Database::Util::Character::CharacterSetMapID(transaction, characterID, mapID);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();

        return ECS::Result::Success;
    }

    ECS::Result CharacterSetPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, const vec3& position, f32 orientation)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        bool result = Database::Util::Character::CharacterSetPositionOrientation(transaction, characterID, position, orientation);
        if (!result)
            return Result::DatabaseError;

        transaction.commit();

        return ECS::Result::Success;
    }

    ECS::Result CharacterSetMapIDPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, u32 mapID, const vec3& position, f32 orientation)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        if (!Database::Util::Character::CharacterSetMapID(transaction, characterID, mapID))
            return Result::DatabaseError;

        if (!Database::Util::Character::CharacterSetPositionOrientation(transaction, characterID, position, orientation))
            return Result::DatabaseError;

        transaction.commit();

        return ECS::Result::Success;
    }

    Result ItemAssign(pqxx::work& transaction, Singletons::GameCache& gameCache, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return Result::ItemNotFound;

        bool result = Database::Util::Character::CharacterAddItem(transaction, characterID, itemInstanceID, containerID, slot);
        if (!result)
            return Result::DatabaseError;

        return Result::Success;
    }

    Result ItemAdd(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity, u64 characterID, u32 itemID, Database::Container& container, u64 containerID, u16 slotIndex, u64& itemInstanceID)
    {
        itemInstanceID = std::numeric_limits<u64>::max();

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        if (!Item::ItemCreate(transaction, gameCache, itemID, characterID, itemInstanceID))
            return Result::GenericError;

        if (ItemAssign(transaction, gameCache, characterID, itemInstanceID, containerID, slotIndex) != ECS::Result::Success)
        {
            Cache::ItemInstanceDelete(gameCache, itemInstanceID);
            return Result::GenericError;
        }

        transaction.commit();

        ObjectGUID itemGUID = ObjectGUID::CreateItem(itemInstanceID);
        container.AddItemToSlot(itemGUID, slotIndex);
        registry.emplace_or_replace<Events::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemDelete(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& container, u64 containerID, u16 slotIndex)
    {
        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        const Database::ContainerItem& containerItem = container.GetItem(slotIndex);
        if (containerItem.IsEmpty())
            return Result::ItemNotFound;

        u64 itemInstanceID = containerItem.objectGUID.GetCounter();
        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return Result::ItemNotFound;

        auto transaction = conn->NewTransaction();

        bool result = Database::Util::Character::CharacterDeleteItem(transaction, characterID, itemInstanceID);
        if (!result)
            return Result::ItemNotFound;

        if (!Item::ItemDelete(transaction, gameCache, itemInstanceID))
            return Result::DatabaseError;

        transaction.commit();

        container.RemoveItem(slotIndex);
        registry.emplace_or_replace<Events::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemSwap(Singletons::GameCache& gameCache, u64 characterID, Database::Container& srcContainer, u64 srcContainerID, u16 srcSlotIndex, Database::Container& dstContainer, u64 dstContainerID, u16 dstSlotIndex)
    {
        const Database::ContainerItem& srcItem = srcContainer.GetItem(srcSlotIndex);
        const Database::ContainerItem& dstItem = dstContainer.GetItem(dstSlotIndex);

        if (srcItem.IsEmpty())
            return Result::ItemNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        bool hasDstItem = !dstItem.IsEmpty();
        u64 srcItemInstanceID = srcItem.objectGUID.GetCounter();
        u64 dstItemInstanceID = dstItem.objectGUID.GetCounter();

        if (!Database::Util::Character::CharacterDeleteItem(transaction, characterID, srcItemInstanceID))
            return Result::ItemNotFound;

        if (hasDstItem)
        {
            if (!Database::Util::Character::CharacterDeleteItem(transaction, characterID, dstItemInstanceID))
                return Result::ItemNotFound;
        }

        Result assignSrcItemResult = ItemAssign(transaction, gameCache, characterID, srcItemInstanceID, dstContainerID, dstSlotIndex);
        if (assignSrcItemResult != Result::Success)
            return assignSrcItemResult;

        if (hasDstItem)
        {
            Result assignDstItemResult = ItemAssign(transaction, gameCache, characterID, dstItemInstanceID, srcContainerID, srcSlotIndex);
            if (assignDstItemResult != Result::Success)
                return assignDstItemResult;
        }

        if (!srcContainer.SwapItems(dstContainer, srcSlotIndex, dstSlotIndex))
            return Result::GenericError;

        transaction.commit();

        return Result::Success;
    }
}