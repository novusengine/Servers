#pragma once
#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Server-Common/Database/Definitions.h>

#include <vector>

namespace ECS
{
    enum class ItemEquipSlot : u8
    {
        EquipmentStart = 0,
        Helm = EquipmentStart,
        Necklace,
        Shoulders,
        Cloak,
        Chest,
        Shirt,
        Tabard,
        Bracers,
        Gloves,
        Belt,
        Pants,
        Boots,
        Ring1,
        Ring2,
        Trinket1,
        Trinket2,
        MainHand,
        OffHand,
        Ranged,
        EquipmentEnd = Ranged,
        MainBag,
        BagStart = MainBag,
        Bag1,
        Bag2,
        Bag3,
        Bag4,
        BagEnd = Bag4,
        Count
    };
    using ItemEquipSlot_t = std::underlying_type_t<ItemEquipSlot>;
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_START = std::underlying_type_t<ItemEquipSlot>(ItemEquipSlot::EquipmentStart);
    static constexpr ItemEquipSlot_t PLAYER_EQUIPMENT_INDEX_END = std::underlying_type_t<ItemEquipSlot>(ItemEquipSlot::EquipmentEnd);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_START = std::underlying_type_t<ItemEquipSlot>(ItemEquipSlot::BagStart);
    static constexpr ItemEquipSlot_t PLAYER_BAG_INDEX_END = std::underlying_type_t<ItemEquipSlot>(ItemEquipSlot::BagEnd);
    static constexpr ItemEquipSlot_t PLAYER_BAG_SLOT_COUNT = std::underlying_type_t<ItemEquipSlot>(ItemEquipSlot::Count);
    static constexpr u8 PLAYER_BASE_CONTAINER_ID = 0;

    namespace Components
    {
        struct PlayerContainers
        {
        public:
            Database::Container equipment = Database::Container(PLAYER_BAG_SLOT_COUNT);
            robin_hood::unordered_map<u16, Database::Container> bags;
        };
    }
}