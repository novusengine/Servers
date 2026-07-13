#pragma once
#include <Base/Types.h>

#include <MetaGen/Shared/Unit/Unit.h>

namespace ECS::Components
{
    struct UnitVisualItems
    {
    public:
        u32 equippedItemIDs[static_cast<u32>(MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd) + 1] = { 0 };
        robin_hood::unordered_set<u32> dirtyItemIDs;
    };
}