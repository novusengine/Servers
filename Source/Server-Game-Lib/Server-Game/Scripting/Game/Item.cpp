#include "Item.h"
#include "ItemTemplate.h"
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
        void Item::Register(Zenith* zenith)
        {
            LuaMetaTable<Item>::Register(zenith, "ItemMetaTable");
            LuaMetaTable<Item>::Set(zenith, itemMethods);
        }

        Item* Item::Create(Zenith* zenith, ObjectGUID guid, u32 templateID)
        {
            Item* item = zenith->PushUserData<Item>([](void* x) {});
            item->guid = guid;
            item->templateID = templateID;

            luaL_getmetatable(zenith->state, "ItemMetaTable");
            lua_setmetatable(zenith->state, -2);

            return item;
        }

        namespace ItemMethods
        {
            i32 GetID(Zenith* zenith, Item* item)
            {
                u64 itemID = item->guid.GetCounter();
                zenith->Push(itemID);
                return 1;
            }
            i32 GetTemplateID(Zenith* zenith, Item* item)
            {
                zenith->Push(item->templateID);
                return 1;
            }

            i32 GetTemplate(Zenith* zenith, Item* item)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();

                GameDefine::Database::ItemTemplate* itemTemplate = nullptr;
                if (!ECS::Util::Cache::GetItemTemplateByID(gameCache, item->templateID, itemTemplate))
                    return 0;

                ItemTemplate::Create(zenith, itemTemplate->id);
                return 1;
            }
        }
    }
}