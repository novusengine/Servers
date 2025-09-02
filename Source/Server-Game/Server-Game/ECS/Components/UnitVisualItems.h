#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Shared/UnitEnum.h>

namespace ECS::Components
{
    struct UnitVisualItems
    {
    public:
        u32 equippedItemIDs[static_cast<u32>(Generated::ItemEquipSlotEnum::EquipmentEnd) + 1] = { 0 };
        robin_hood::unordered_set<u32> dirtyItemIDs;
    };
}