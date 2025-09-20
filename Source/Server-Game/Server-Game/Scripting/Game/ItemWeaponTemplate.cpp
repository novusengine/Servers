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
        void ItemWeaponTemplate::Register(Zenith* zenith)
        {
            LuaMetaTable<ItemWeaponTemplate>::Register(zenith, "ItemWeaponTemplateMetaTable");
            LuaMetaTable<ItemWeaponTemplate>::Set(zenith, itemWeaponTemplateMethods);
        }

        ItemWeaponTemplate* ItemWeaponTemplate::Create(Zenith* zenith, u32 id)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();

            ItemWeaponTemplate* itemWeaponTemplate = zenith->PushUserData<ItemWeaponTemplate>([](void* x) {});
            itemWeaponTemplate->id = id;
            ECS::Util::Cache::GetItemWeaponTemplateByID(gameCache, id, itemWeaponTemplate->definition);

            luaL_getmetatable(zenith->state, "ItemWeaponTemplateMetaTable");
            lua_setmetatable(zenith->state, -2);

            return itemWeaponTemplate;
        }

        namespace ItemWeaponTemplateMethods
        {
            i32 GetID(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate)
            {
                zenith->Push(itemWeaponTemplate->id);
                return 1;
            }

            i32 GetWeaponStyle(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate)
            {
                zenith->Push(itemWeaponTemplate->definition ? itemWeaponTemplate->definition->weaponStyle : 1u);
                return 1;
            }

            i32 GetDamageRange(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate)
            {
                zenith->Push(itemWeaponTemplate->definition ? itemWeaponTemplate->definition->minDamage : 1u);
                zenith->Push(itemWeaponTemplate->definition ? itemWeaponTemplate->definition->maxDamage : 2u);
                return 2;
            }

            i32 GetSpeed(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate)
            {
                zenith->Push(itemWeaponTemplate->definition ? itemWeaponTemplate->definition->speed : 1.0f);
                return 1;
            }
        }
    }
}