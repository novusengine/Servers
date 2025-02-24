#pragma once
#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Server-Common/Database/Definitions.h>

#include <vector>

namespace ECS
{
    enum class ItemEquipSlot : u8
    {
        Helm,
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
        MainBag,
        Bag1,
        Bag2,
        Bag3,
        Bag4,
        Count
    };

    namespace Components
    {
        struct PlayerContainers
        {
        public:
            Database::Container equipment = Database::Container((u8)ItemEquipSlot::Count);
            std::vector<Database::Container> bags;
        };
    }
}