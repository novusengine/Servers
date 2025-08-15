#include "ContainerUtil.h"

#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <entt/entt.hpp>

namespace ECS::Util::Container
{
    bool GetFirstFreeSlot(Database::Container& container, u16& outSlotIndex)
    {
        outSlotIndex = Database::Container::INVALID_SLOT;

        u16 nextFreeSlot = container.GetNextFreeSlot();
        if (nextFreeSlot == Database::Container::INVALID_SLOT)
            return false;

        outSlotIndex = nextFreeSlot;
        return true;
    }

    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, const Database::Container& container, u32 itemID, u16 startIndex, u16 endIndex, u16& outSlotIndex)
    {
        outSlotIndex = Database::Container::INVALID_SLOT;

        if (container.IsEmpty())
            return false;

        for (u16 j = startIndex; j <= endIndex; j++)
        {
            const auto& item = container.GetItem(j);
            if (item.IsEmpty())
                continue;

            u64 itemInstanceID = item.objectGuid.GetCounter();

            Database::ItemInstance* itemInstance = nullptr;
            if (!Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
                continue;

            if (itemInstance->itemID == itemID)
            {
                outSlotIndex = j;
                break;
            }
        }

        bool result = outSlotIndex != Database::Container::INVALID_SLOT;
        return result;
    }

    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, const Database::Container& container, u32 itemID, u16& outSlotIndex)
    {
        u16 endIndex = container.GetTotalSlots() - 1;
        return GetFirstItemSlot(gameCache, container, itemID, 0, endIndex, outSlotIndex);
    }

    bool GetFirstFreeSlotInBags(ECS::Components::PlayerContainers& playerContainers, u16& outContainerIndex, u16& outSlotIndex)
    {
        outContainerIndex = Database::Container::INVALID_SLOT;
        outSlotIndex = Database::Container::INVALID_SLOT;

        for (u16 equipmentContainerIndex = PLAYER_BAG_INDEX_START; equipmentContainerIndex < PLAYER_BAG_INDEX_END; equipmentContainerIndex++)
        {
            const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(equipmentContainerIndex);
            if (containerItem.IsEmpty())
                continue;

            auto& container = playerContainers.bags[equipmentContainerIndex];
            if (!GetFirstFreeSlot(container, outSlotIndex))
                continue;

            outContainerIndex = equipmentContainerIndex;
            break;
        }

        bool result = outContainerIndex != Database::Container::INVALID_SLOT && outSlotIndex != Database::Container::INVALID_SLOT;
        return result;
    }

    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, robin_hood::unordered_map<u16, Database::Container>& containers, u32 itemID, u16& outContainerIndex, u16& outSlotIndex)
    {
        outContainerIndex = Database::Container::INVALID_SLOT;
        outSlotIndex = Database::Container::INVALID_SLOT;

        for (auto& [i, container] : containers)
        {
            if (container.IsEmpty())
                continue;

            if (!GetFirstItemSlot(gameCache, container, itemID, outSlotIndex))
                continue;

            outContainerIndex = i;
            break;
        }

        bool result = outContainerIndex != Database::Container::INVALID_SLOT && outSlotIndex != Database::Container::INVALID_SLOT;
        return result;
    }

    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, ECS::Components::PlayerContainers& playerContainers, u32 itemID, Database::Container*& outContainer, u64& outContainerID, u16& outContainerIndex, u16& outSlotIndex)
    {
        outContainerID = 0;

        if (Util::Container::GetFirstItemSlot(gameCache, playerContainers.bags, itemID, outContainerIndex, outSlotIndex))
        {
            outContainer = &playerContainers.bags[outContainerIndex];

            const auto& baseContainerItem = playerContainers.equipment.GetItem(outContainerIndex);
            outContainerID = baseContainerItem.objectGuid.GetCounter();

            return true;
        }
        else if (Util::Container::GetFirstItemSlot(gameCache, playerContainers.equipment, itemID, PLAYER_EQUIPMENT_INDEX_START, PLAYER_EQUIPMENT_INDEX_END, outSlotIndex))
        {
            outContainer = &playerContainers.equipment;
            return true;
        }

        return false;
    }
}