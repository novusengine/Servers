#include "CharacterUtil.h"
#include "ItemUtil.h"

#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/StringUtils.h>

#include <entt/entt.hpp>

namespace ECS::Util::Persistence::Character
{
    Result CharacterCreate(entt::registry& registry, const std::string& name, u16 raceGenderClass, u64& characterID)
    {
        characterID = std::numeric_limits<u64>::max();

        auto& gameCache = registry.ctx().get<Singletons::GameCache>();
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

    Result CharacterDelete(entt::registry& registry, u64 characterID)
    {
        auto& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        
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

    ECS::Result CharacterSetRace(entt::registry& registry, u64 characterID, GameDefine::UnitRace unitRace)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

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

    ECS::Result CharacterSetGender(entt::registry& registry, u64 characterID, GameDefine::UnitGender unitGender)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

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

    ECS::Result CharacterSetClass(entt::registry& registry, u64 characterID, GameDefine::UnitClass unitClass)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

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

    ECS::Result CharacterSetLevel(entt::registry& registry, u64 characterID, u16 level)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

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

    Result ItemAssign(pqxx::work& transaction, entt::registry& registry, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot)
    {
        auto& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return Result::ItemNotFound;

        bool result = Database::Util::Character::CharacterAddItem(transaction, characterID, itemInstanceID, containerID, slot);
        if (!result)
            return Result::DatabaseError;

        return Result::Success;
    }

    Result ItemAdd(entt::registry& registry, entt::entity characterEntity, u64 characterID, u32 itemID, Database::Container& container, u64 containerID, u16 slotIndex, u64& itemInstanceID)
    {
        itemInstanceID = std::numeric_limits<u64>::max();

        auto& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        if (!Item::ItemCreate(transaction, registry, itemID, characterID, itemInstanceID))
            return Result::GenericError;

        if (ItemAssign(transaction, registry, characterID, itemInstanceID, containerID, slotIndex) != ECS::Result::Success)
        {
            Cache::ItemInstanceDelete(gameCache, itemInstanceID);
            return Result::GenericError;
        }

        transaction.commit();

        GameDefine::ObjectGuid itemGuid = GameDefine::ObjectGuid::CreateItem(itemInstanceID);
        container.AddItemToSlot(itemGuid, slotIndex);
        registry.emplace_or_replace<Tags::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemDelete(entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& container, u64 containerID, u16 slotIndex)
    {
        auto& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        const Database::ContainerItem& containerItem = container.GetItem(slotIndex);
        if (containerItem.IsEmpty())
            return Result::ItemNotFound;

        u64 itemInstanceID = containerItem.objectGuid.GetCounter();
        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return Result::ItemNotFound;

        auto transaction = conn->NewTransaction();

        bool result = Database::Util::Character::CharacterDeleteItem(transaction, characterID, itemInstanceID);
        if (!result)
            return Result::ItemNotFound;

        if (!Item::ItemDelete(transaction, registry, itemInstanceID))
            return Result::DatabaseError;

        transaction.commit();

        container.RemoveItem(slotIndex);
        registry.emplace_or_replace<Tags::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemSwap(entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& srcContainer, u64 srcContainerID, u16 srcSlotIndex, Database::Container& dstContainer, u64 dstContainerID, u16 dstSlotIndex)
    {
        auto& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        const Database::ContainerItem& srcItem = srcContainer.GetItem(srcSlotIndex);
        const Database::ContainerItem& dstItem = dstContainer.GetItem(dstSlotIndex);

        if (srcItem.IsEmpty())
            return Result::ItemNotFound;

        auto conn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        if (!conn)
            return Result::DatabaseNotConnected;

        auto transaction = conn->NewTransaction();

        bool hasDstItem = !dstItem.IsEmpty();
        u64 srcItemInstanceID = srcItem.objectGuid.GetCounter();
        u64 dstItemInstanceID = dstItem.objectGuid.GetCounter();

        if (!Database::Util::Character::CharacterDeleteItem(transaction, characterID, srcItemInstanceID))
            return Result::ItemNotFound;

        if (hasDstItem)
        {
            if (!Database::Util::Character::CharacterDeleteItem(transaction, characterID, dstItemInstanceID))
                return Result::ItemNotFound;
        }

        Result assignSrcItemResult = ItemAssign(transaction, registry, characterID, srcItemInstanceID, dstContainerID, dstSlotIndex);
        if (assignSrcItemResult != Result::Success)
            return assignSrcItemResult;

        if (hasDstItem)
        {
            Result assignDstItemResult = ItemAssign(transaction, registry, characterID, dstItemInstanceID, srcContainerID, srcSlotIndex);
            if (assignDstItemResult != Result::Success)
                return assignDstItemResult;
        }

        if (!srcContainer.SwapItems(dstContainer, srcSlotIndex, dstSlotIndex))
            return Result::GenericError;

        transaction.commit();

        return Result::Success;
    }
}