#pragma once
#include <Server-Common/Database/Definitions.h>

#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <MetaGen/Shared/Unit/Unit.h>

#include <vector>

namespace ECS
{
    using ItemEquipSlot_t = std::underlying_type_t<MetaGen::Shared::Unit::ItemEquipSlotEnum>;
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_START = static_cast<ItemEquipSlot_t>(MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentStart);
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_END = static_cast<ItemEquipSlot_t>(MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_START = static_cast<ItemEquipSlot_t>(MetaGen::Shared::Unit::ItemEquipSlotEnum::BagStart);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_END = static_cast<ItemEquipSlot_t>(MetaGen::Shared::Unit::ItemEquipSlotEnum::BagEnd);
    static constexpr ItemEquipSlot_t PLAYER_BASE_CONTAINER_SLOT_COUNT = static_cast<ItemEquipSlot_t>(MetaGen::Shared::Unit::ItemEquipSlotEnum::Count);
    static constexpr u8 PLAYER_BASE_CONTAINER_ID = 0;

    namespace Components
    {
        struct PlayerContainers
        {
        public:
            Database::Container equipment = Database::Container(PLAYER_BASE_CONTAINER_SLOT_COUNT);
            robin_hood::unordered_map<u16, Database::Container> bags;
        };
    }
}