#include "Character.h"
#include "Item.h"
#include "Server-Game/Application/Application.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/Definitions.h>

#include <MetaGen/Shared/Unit/Unit.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>
#include <Server-Game/ECS/Util/Cache/CacheUtil.h>

namespace Scripting
{
    namespace Game
    {
        void Character::Register(Zenith* zenith)
        {
            LuaMetaTable<Character>::Register(zenith, "CharacterMetaTable");
            LuaMetaTable<Character>::Set(zenith, unitMethods);
            LuaMetaTable<Character>::Set(zenith, characterMethods);
        }

        Character* Character::Create(Zenith* zenith, entt::entity entity)
        {
            Character* character = zenith->PushUserData<Character>([](void* x) {});
            character->entity = entity;

            luaL_getmetatable(zenith->state, "CharacterMetaTable");
            lua_setmetatable(zenith->state, -2);

            return character;
        }

        namespace CharacterMethods
        {
            i32 GetEquippedItem(Zenith* zenith, Character* character)
            {
                auto slot = static_cast<MetaGen::Shared::Unit::ItemEquipSlotEnum>(zenith->CheckVal<u16>(2));
                if (slot < MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentStart || slot >= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = character->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(entity))
                    return 0;

                auto& playerContainers = world.Get<ECS::Components::PlayerContainers>(entity);
                auto& containerItem = playerContainers.equipment.GetItem((u16)slot);
                if (containerItem.IsEmpty())
                    return 0;

                ObjectGUID itemGUID = containerItem.objectGUID;

                Database::ItemInstance* itemInstance = nullptr;
                if (!ECS::Util::Cache::GetItemInstanceByID(gameCache, itemGUID.GetCounter(), itemInstance))
                    return 0;

                Item::Create(zenith, itemGUID, itemInstance->itemID);
                return 1;
            }
        }
    }
}
