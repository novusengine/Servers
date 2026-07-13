#include "ItemTemplate.h"
#include "ItemWeaponTemplate.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void ItemTemplate::Register(Zenith* zenith)
        {
            LuaMetaTable<ItemTemplate>::Register(zenith, "ItemTemplateMetaTable");
            LuaMetaTable<ItemTemplate>::Set(zenith, itemTemplateMethods);
        }

        ItemTemplate* ItemTemplate::Create(Zenith* zenith, u32 id)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();

            ItemTemplate* itemTemplateTemplate = zenith->PushUserData<ItemTemplate>([](void* x) {});
            itemTemplateTemplate->id = id;
            ECS::Util::Cache::GetItemTemplateByID(gameCache, id, itemTemplateTemplate->definition);

            luaL_getmetatable(zenith->state, "ItemTemplateMetaTable");
            lua_setmetatable(zenith->state, -2);

            return itemTemplateTemplate;
        }

        namespace ItemTemplateMethods
        {
            i32 GetID(Zenith* zenith, ItemTemplate* itemTemplateTemplate)
            {
                zenith->Push(itemTemplateTemplate->id);
                return 1;
            }

            i32 GetDisplayID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->displayID : 0);
                return 1;
            }
            i32 GetBind(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->bind : 1);
                return 1;
            }
            i32 GetRarity(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->rarity : 1);
                return 1;
            }
            i32 GetCategory(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->category : 1);
                return 1;
            }
            i32 GetType(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->type : 1);
                return 1;
            }
            i32 GetVirtualLevel(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->virtualLevel : 1);
                return 1;
            }
            i32 GetRequiredLevel(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->requiredLevel : 1);
                return 1;
            }
            i32 GetDurability(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->durability : 0);
                return 1;
            }
            i32 GetIconID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->iconID : 0);
                return 1;
            }
            i32 GetName(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->name.c_str() : "");
                return 1;
            }
            i32 GetDescription(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->description.c_str() : "");
                return 1;
            }
            i32 GetArmor(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->armor : 0);
                return 1;
            }
            i32 GetStatTemplateID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->statTemplateID : 0);
                return 1;
            }
            i32 GetArmorTemplateID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->armorTemplateID : 0);
                return 1;
            }
            i32 GetWeaponTemplateID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->weaponTemplateID : 0);
                return 1;
            }
            i32 GetShieldTemplateID(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                zenith->Push(itemTemplate->definition ? itemTemplate->definition->shieldTemplateID : 0);
                return 1;
            }
            i32 GetWeaponTemplate(Zenith* zenith, ItemTemplate* itemTemplate)
            {
                u32 weaponTemplateID = itemTemplate->definition ? itemTemplate->definition->weaponTemplateID : 0;
                
                ItemWeaponTemplate::Create(zenith, weaponTemplateID);
                return 1;
            }
        }
    }
}