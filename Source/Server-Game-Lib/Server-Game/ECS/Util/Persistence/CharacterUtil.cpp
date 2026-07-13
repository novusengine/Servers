#include "CharacterUtil.h"

#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/StringUtils.h>

#include <entt/entt.hpp>

namespace ECS::Util::Persistence::Character
{
    namespace
    {
        Result DatabaseFailureResult(Database::OperationFailure failure, Database::OperationFailure* output = nullptr)
        {
            if (output)
                *output = failure;
            return failure == Database::OperationFailure::Unavailable ? Result::DatabaseNotConnected : Result::DatabaseError;
        }
    }

    Result CharacterCreate(Singletons::GameCache& gameCache, u64 accountID, const std::string& name, u16 raceGenderClass, u64& characterID)
    {
        characterID = std::numeric_limits<u64>::max();

        if (Util::Cache::CharacterExistsByName(gameCache, name))
            return Result::CharacterAlreadyExists;

        auto result = gameCache.database->CreateCharacter(accountID, name, raceGenderClass);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        characterID = result.Value().id;
        return Result::Success;
    }

    Result CharacterDelete(Singletons::GameCache& gameCache, u64 characterID)
    {
        if (!Util::Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto result = gameCache.database->DeleteCharacter(characterID);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        return Result::Success;
    }

    ECS::Result CharacterSetRace(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitRace unitRace)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(unitRace) | (static_cast<u16>(displayInfo.unitGender) << 7) | (static_cast<u16>(characterInfo.unitClass) << 9);
        auto result = gameCache.database->SetCharacterRaceGenderClass(characterID, raceGenderClass);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        displayInfo.unitRace = unitRace;
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetGender(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitGender unitGender)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(displayInfo.unitRace) | (static_cast<u16>(unitGender) << 7) | (static_cast<u16>(characterInfo.unitClass) << 9);
        auto result = gameCache.database->SetCharacterRaceGenderClass(characterID, raceGenderClass);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        displayInfo.unitGender = unitGender;
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetClass(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitClass unitClass)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        auto& displayInfo = registry.get<Components::DisplayInfo>(characterEntity);

        u16 raceGenderClass = static_cast<u16>(displayInfo.unitRace) | (static_cast<u16>(displayInfo.unitGender) << 7) | (static_cast<u16>(unitClass) << 9);
        auto result = gameCache.database->SetCharacterRaceGenderClass(characterID, raceGenderClass);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        Util::Cache::CharacterSetClass(characterInfo, unitClass);
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetLevel(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, u16 level)
    {
        entt::entity characterEntity = entt::null;
        if (!Util::Cache::GetEntityByCharacterID(gameCache, characterID, characterEntity))
            return Result::CharacterNotFound;

        auto result = gameCache.database->SetCharacterLevel(characterID, level);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        auto& characterInfo = registry.get<Components::CharacterInfo>(characterEntity);
        Util::Cache::CharacterSetLevel(characterInfo, level);
        return ECS::Result::Success;
    }

    ECS::Result CharacterSetMapID(Singletons::GameCache& gameCache, u64 characterID, u32 mapID)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto result = gameCache.database->SetCharacterMap(characterID, mapID);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        return ECS::Result::Success;
    }

    ECS::Result CharacterSetPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, const vec3& position, f32 orientation)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto result = gameCache.database->SetCharacterPosition(characterID, position, orientation);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        return ECS::Result::Success;
    }

    ECS::Result CharacterSetMapIDPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, u32 mapID, const vec3& position, f32 orientation)
    {
        if (!Cache::CharacterExistsByID(gameCache, characterID))
            return Result::CharacterNotFound;

        auto result = gameCache.database->SetCharacterMapAndPosition(characterID, mapID, position, orientation);
        if (!result)
            return DatabaseFailureResult(result.Failure());
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::DatabaseError;

        return ECS::Result::Success;
    }

    Result ItemAdd(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity, u64 characterID,
        u32 itemID, Database::Container& container, u64 containerID, u16 slotIndex, u64& itemInstanceID,
        Database::OperationFailure* databaseFailure)
    {
        if (databaseFailure) *databaseFailure = Database::OperationFailure::None;
        itemInstanceID = std::numeric_limits<u64>::max();

        GameDefine::Database::ItemTemplate* itemTemplate = nullptr;
        if (!Cache::GetItemTemplateByID(gameCache, itemID, itemTemplate))
            return Result::ItemNotFound;

        auto result = gameCache.database->AddCharacterItem(characterID, itemID, containerID, slotIndex, 1,
            static_cast<u16>(itemTemplate->durability));
        if (!result)
            return DatabaseFailureResult(result.Failure(), databaseFailure);

        const auto& item = result.Value();
        itemInstanceID = item.id;
        Cache::ItemInstanceCreate(gameCache, itemInstanceID,
        {
            .id = item.id,
            .ownerID = item.ownerId,
            .itemID = item.itemId,
            .count = item.count,
            .durability = item.durability
        });

        ObjectGUID itemGUID = ObjectGUID::CreateItem(itemInstanceID);
        container.AddItemToSlot(itemGUID, slotIndex);
        registry.emplace_or_replace<Events::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemDelete(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity,
        u64 characterID, Database::Container& container, u64 containerID, u16 slotIndex,
        Database::OperationFailure* databaseFailure)
    {
        if (databaseFailure) *databaseFailure = Database::OperationFailure::None;
        const Database::ContainerItem& containerItem = container.GetItem(slotIndex);
        if (containerItem.IsEmpty())
            return Result::ItemNotFound;

        u64 itemInstanceID = containerItem.objectGUID.GetCounter();
        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return Result::ItemNotFound;

        auto result = gameCache.database->DeleteCharacterItem(characterID, itemInstanceID);
        if (!result)
            return DatabaseFailureResult(result.Failure(), databaseFailure);
        if (result.Value() == Database::DeleteOutcome::AlreadyAbsent)
            return Result::ItemNotFound;

        Cache::ItemInstanceDelete(gameCache, itemInstanceID);
        container.RemoveItem(slotIndex);
        registry.emplace_or_replace<Events::CharacterNeedsContainerUpdate>(characterEntity);

        return Result::Success;
    }

    ECS::Result ItemSwap(Singletons::GameCache& gameCache, u64 characterID, Database::Container& srcContainer,
        u64 srcContainerID, u16 srcSlotIndex, Database::Container& dstContainer, u64 dstContainerID,
        u16 dstSlotIndex, Database::OperationFailure* databaseFailure)
    {
        if (databaseFailure) *databaseFailure = Database::OperationFailure::None;
        if (srcSlotIndex >= srcContainer.GetTotalSlots() || dstSlotIndex >= dstContainer.GetTotalSlots())
            return Result::GenericError;

        const Database::ContainerItem& srcItem = srcContainer.GetItem(srcSlotIndex);
        const Database::ContainerItem& dstItem = dstContainer.GetItem(dstSlotIndex);

        if (srcItem.IsEmpty())
            return Result::ItemNotFound;

        bool hasDstItem = !dstItem.IsEmpty();
        u64 srcItemInstanceID = srcItem.objectGUID.GetCounter();
        u64 dstItemInstanceID = dstItem.objectGUID.GetCounter();

        auto result = gameCache.database->SwapCharacterItems(characterID, srcItemInstanceID, dstItemInstanceID,
            hasDstItem, srcContainerID, dstContainerID, srcSlotIndex, dstSlotIndex);
        if (!result)
            return DatabaseFailureResult(result.Failure(), databaseFailure);
        if (result.Value() != Database::UpdateOutcome::Updated)
            return Result::ItemNotFound;

        if (!srcContainer.SwapItems(dstContainer, srcSlotIndex, dstSlotIndex))
            return Result::GenericError;

        return Result::Success;
    }
}
