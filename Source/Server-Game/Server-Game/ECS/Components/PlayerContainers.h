#pragma once
#include <Server-Common/Database/Definitions.h>

#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Meta/Generated/Shared/UnitEnum.h>

#include <vector>

namespace ECS
{
    using ItemEquipSlot_t = std::underlying_type_t<Generated::ItemEquipSlotEnum>;
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_START = std::underlying_type_t<Generated::ItemEquipSlotEnum>(Generated::ItemEquipSlotEnum::EquipmentStart);
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_END = std::underlying_type_t<Generated::ItemEquipSlotEnum>(Generated::ItemEquipSlotEnum::EquipmentEnd);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_START = std::underlying_type_t<Generated::ItemEquipSlotEnum>(Generated::ItemEquipSlotEnum::BagStart);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_END = std::underlying_type_t<Generated::ItemEquipSlotEnum>(Generated::ItemEquipSlotEnum::BagEnd);
    static constexpr ItemEquipSlot_t PLAYER_BASE_CONTAINER_SLOT_COUNT = std::underlying_type_t<Generated::ItemEquipSlotEnum>(Generated::ItemEquipSlotEnum::Count);
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