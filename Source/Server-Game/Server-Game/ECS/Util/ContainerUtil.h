#pragma once
#include <Server-Common/Database/Definitions.h>

#include <Base/Types.h>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>

namespace ECS
{
    namespace Components
    {
        struct PlayerContainers;
    }

    namespace Singletons
    {
        struct GameCache;
    }
}

namespace Database
{
    struct Container;
}

namespace ECS::Util::Container
{
    bool GetFirstFreeSlot(Database::Container& container, u16& outSlotIndex);
    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, const Database::Container& container, u32 itemID, u16 startIndex, u16 endIndex, u16& outSlotIndex);
    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, const Database::Container& container, u32 itemID, u16& outSlotIndex);

    bool GetFirstFreeSlotInBags(ECS::Components::PlayerContainers& playerContainers, u16& outContainerIndex, u16& outSlotIndex);
    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, robin_hood::unordered_map<u16, Database::Container>& containers, u32 itemID, u16& outContainerIndex, u16& outSlotIndex);
    bool GetFirstItemSlot(ECS::Singletons::GameCache& gameCache, ECS::Components::PlayerContainers& playerContainers, u32 itemID, Database::Container*& outContainer, u64& outContainerID, u16& outContainerIndex, u16& outSlotIndex);
}