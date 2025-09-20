#pragma once
#include <Base/Types.h>
#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Meta/Generated/Shared/ProximityTriggerEnum.h>

#include <robinhood/robinhood.h>

#include <entt/fwd.hpp>

namespace Database
{
    static constexpr u32 CHARACTER_BASE_CONTAINER_SIZE = 24;

    struct CharacterCurrency
    {
    public:
        u16 currencyID = 0;
        u64 value = 0;
    };

    struct Permission
    {
    public:
        u16 id = 0;
        std::string name;
    };
    struct PermissionGroup
    {
    public:
        u16 id = 0;
        std::string name;
    };
    struct PermissionGroupData
    {
    public:
        u16 groupID = 0;
        u16 permissionID = 0;
    };

    struct Currency
    {
    public:
        u16 id = 0;
        std::string name;
    };

    struct ContainerItem
    {
    public:
        bool IsEmpty() const { return !objectGUID.IsValid(); }
        void Clear()
        {
            objectGUID = ObjectGUID::Empty;
        }

    public:
        ObjectGUID objectGUID = ObjectGUID::Empty;
    };

    struct Container
    {
    public:
        Container() = default;
        Container(u16 numSlots) : _slots(numSlots), _freeSlots(numSlots)
        {
            _items.resize(numSlots);
        }

        /**
        * Finds the next available free slot in the container
        * @return Index of next free slot, or INVALID_SLOT if container is full
        */
        u16 GetNextFreeSlot() const
        {
            if (IsFull()) return INVALID_SLOT;

            for (u16 i = 0; i < _slots; ++i)
            {
                if (_items[i].IsEmpty())
                    return i;
            }

            return INVALID_SLOT;
        }

        /**
        * Checks if a specific slot is empty
        * @param slotIndex The slot to check
        * @return true if the slot is empty
        */
        bool IsSlotEmpty(u16 slotIndex) const
        {
#if defined(NC_DEBUG)
            NC_ASSERT(slotIndex < _slots, "Container::IsSlotEmpty - Called with slotIndex ({0}) when the container can only hold {1} items", slotIndex, _slots);
#endif
            if (slotIndex >= _slots)
                return true;

            return _items[slotIndex].IsEmpty();
        }

        /**
        * Attempts to add an item to the next available slot
        * @param itemGUID The item to add
        * @return Slot where item was added, or INVALID_SLOT if container is full
        */
        u16 AddItem(ObjectGUID itemGUID)
        {
            u16 slot = GetNextFreeSlot();
            if (slot == INVALID_SLOT)
                return INVALID_SLOT;

            return AddItemToSlot(itemGUID, slot);
        }

        /**
        * Attempts to add an item to a specific slot
        * @param itemGUID The item to add
        * @param slotIndex The desired slot
        * @return The slot where item was added, or INVALID_SLOT if slot was occupied/invalid
        */
        u16 AddItemToSlot(ObjectGUID itemGUID, u16 slotIndex)
        {
#if defined(NC_DEBUG)
            NC_ASSERT(slotIndex < _slots, "Container::AddItemToSlot - Called with slotIndex ({0}) when the container can only hold {1} items", slotIndex, _slots);
#endif
            if (slotIndex >= _slots || !_items[slotIndex].IsEmpty())
                return INVALID_SLOT;

            --_freeSlots;
            _items[slotIndex] = { itemGUID };
            _dirtySlots.insert(slotIndex);

            return slotIndex;
        }

        /**
        * Removes an item from a specific slot
        * @param slotIndex The slot to remove from
        * @return A boolean specifying if there was an item to remove
        */
        bool RemoveItem(u16 slotIndex)
        {
#if defined(NC_DEBUG)
            NC_ASSERT(slotIndex < _slots, "Container::RemoveItem - Called with slotIndex ({0}) when the container can only hold {1} items", slotIndex, _slots);
#endif
            if (slotIndex >= _slots || _items[slotIndex].IsEmpty())
                return false;

            ++_freeSlots;

            _items[slotIndex].Clear();
            _dirtySlots.insert(slotIndex);

            return true;
        }

        bool SwapItems(u16 srcSlotIndex, u16 destSlotIndex)
        {
#if defined(NC_DEBUG)
            NC_ASSERT(srcSlotIndex < _slots, "Container::SwapItems - Called with srcSlotIndex ({0}) when the container can only hold {1} items", srcSlotIndex, _slots);
            NC_ASSERT(destSlotIndex < _slots, "Container::SwapItems - Called with destSlotIndex ({0}) when the container can only hold {1} items", destSlotIndex, _slots);
#endif
            if (srcSlotIndex >= _slots || destSlotIndex >= _slots)
                return false;

            std::swap(_items[srcSlotIndex], _items[destSlotIndex]);

            return true;
        }

        bool SwapItems(Container& destContainer, u16 srcSlotIndex, u16 destSlotIndex)
        {
            u16 numSrcSlots = _slots;
            u16 numDestSlots = destContainer._slots;
#if defined(NC_DEBUG)
            NC_ASSERT(srcSlotIndex < numSrcSlots, "Container::SwapItems - Called with srcSlotIndex ({0}) when the container can only hold {1} items", srcSlotIndex, numSrcSlots);
            NC_ASSERT(destSlotIndex < numDestSlots, "Container::SwapItems - Called with destSlotIndex ({0}) when the container can only hold {1} items", destSlotIndex, numDestSlots);
#endif
            if (srcSlotIndex >= numSrcSlots || destSlotIndex >= numDestSlots)
                return false;
            
            bool srcSlotEmpty = _items[srcSlotIndex].IsEmpty();
            bool destSlotEmpty = destContainer._items[destSlotIndex].IsEmpty();
            bool isSameContainer = this == &destContainer;

            std::swap(_items[srcSlotIndex], destContainer._items[destSlotIndex]);

            if (!isSameContainer)
            {
                if (srcSlotEmpty && !destSlotEmpty)
                {
                    --_freeSlots;
                    ++destContainer._freeSlots;
                }
                else if (!srcSlotEmpty && destSlotEmpty)
                {
                    ++_freeSlots;
                    --destContainer._freeSlots;
                }
            }

            return true;
        }

        /**
        * Gets the item GUID at the specified slot
        * @param slotIndex The slot to check
        * @return The GUID of the item in the slot
        */
        const ContainerItem& GetItem(u16 slotIndex) const
        {
#if defined(NC_DEBUG)
            NC_ASSERT(slotIndex < _slots, "Container::GetItemGUID - Called with slotIndex ({0}) when the container can only hold {1} items", slotIndex, _slots);
#endif
            return _items[slotIndex];
        }

        /**
        * Gets the backing vector for all items
        * @return const std::vector& with all ContainerItem
        */
        const std::vector<ContainerItem>& GetItems() const { return _items; }

        /**
        * Checks if the container is full
        * @return true if no slots are available
        */
        bool IsFull() const { return _freeSlots == 0; }

        /**
        * Checks if the container is empty
        * @return true if all slots are available
        */
        bool IsEmpty() const { return _freeSlots == _slots; }

        /**
         * Gets the total number of slots in the container
         * @return Total slot count
         */
        u16 GetTotalSlots() const { return _slots; }

        /**
        * Gets the number of free slots in the container
        * @return Number of available slots
        */
        u16 GetFreeSlots() const { return _freeSlots; }

        bool IsUninitialized() const { return _isUninitialized; }
        robin_hood::unordered_set<u16>& GetDirtySlots() { return _dirtySlots; }
        void SetSlotAsDirty(u16 slotIndex)
        {
            _dirtySlots.insert(slotIndex);
        }
        void ClearDirtySlots()
        {
            _isUninitialized = false;
            _dirtySlots.clear();
        }

    public:
        static constexpr u16 INVALID_SLOT = 0xFFFF;

    private:
        u16 _slots = 0;
        u16 _freeSlots = 0;

        std::vector<ContainerItem> _items;

        bool _isUninitialized = true;
        robin_hood::unordered_set<u16> _dirtySlots;
    };

    struct ItemInstance
    {
    public:
        u64 id = 0;
        u64 ownerID = 0;
        u32 itemID = 0;
        u16 count = 0;
        u16 durability = 0;
    };

    struct CharacterItem
    {
    public:
        u64 charid = 0;
        u64 itemInstanceId = 0;
        u16 containerID = 0;
        u16 slot = 0;
    };

    struct ProximityTrigger
    {
    public:
        u32 id = 0;
        std::string name = "";
        Generated::ProximityTriggerFlagEnum flags = Generated::ProximityTriggerFlagEnum::None;
        u16 mapID = 0;
        vec3 position = vec3(0.0f, 0.0f, 0.0f);
        vec3 extents = vec3(1.0f, 1.0f, 1.0f);
    };

    struct PermissionTables
    {
    public:
        robin_hood::unordered_map<u16, Database::Permission> idToDefinition;
        robin_hood::unordered_map<u16, Database::PermissionGroup> groupIDToDefinition;
        robin_hood::unordered_map<u16, std::vector<Database::PermissionGroupData>> groupIDToData;
    };

    struct CurrencyTables
    {
    public:
        robin_hood::unordered_map<u16, Database::Currency> idToDefinition;
    };

    struct ItemTables
    {
    public:
        robin_hood::unordered_map<u32, GameDefine::Database::ItemTemplate> templateIDToTemplateDefinition;
        robin_hood::unordered_map<u32, GameDefine::Database::ItemStatTemplate> statTemplateIDToTemplateDefinition;
        robin_hood::unordered_map<u32, GameDefine::Database::ItemArmorTemplate> armorTemplateIDToTemplateDefinition;
        robin_hood::unordered_map<u32, GameDefine::Database::ItemWeaponTemplate> weaponTemplateIDToTemplateDefinition;
        robin_hood::unordered_map<u32, GameDefine::Database::ItemShieldTemplate> shieldTemplateIDToTemplateDefinition;
        robin_hood::unordered_map<u64, ItemInstance> itemInstanceIDToDefinition;
    };

    struct CharacterTables
    {
    public:
        robin_hood::unordered_map<u64, entt::entity> charIDToEntity;
        robin_hood::unordered_map<u32, u64> charNameHashToCharID;
        robin_hood::unordered_map<u64, std::vector<u16>> charIDToPermissions;
        robin_hood::unordered_map<u64, std::vector<u16>> charIDToPermissionGroups;
        robin_hood::unordered_map<u64, std::vector<CharacterCurrency>> charIDToCurrency;
    };

    struct CreatureTables
    {
    public:
        robin_hood::unordered_map<u32, GameDefine::Database::CreatureTemplate> templateIDToDefinition;
    };
    
    struct MapTables
    {
    public:
        robin_hood::unordered_map<u32, GameDefine::Database::Map> idToDefinition;
        robin_hood::unordered_map<u32, GameDefine::Database::MapLocation> locationIDToDefinition;
        robin_hood::unordered_map<u32, u32> locationNameHashToID;
    };

    struct SpellTables
    {
    public:
        robin_hood::unordered_map<u32, GameDefine::Database::Spell> idToDefinition;
        robin_hood::unordered_map<u32, std::vector<GameDefine::Database::SpellEffect>> spellIDToEffects;
    };
}