#include "CharacterUtil.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/ItemUtils.h>

#include <entt/entt.hpp>

namespace ECS::Util::Persistence::Item
{
    bool ItemCreate(pqxx::work& transaction, entt::registry& registry, u32 itemEntryID, u64 ownerID, u64& itemInstanceID)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

        GameDefine::Database::ItemTemplate* itemTemplate = nullptr;
        if (!Cache::GetItemTemplateByID(gameCache, itemEntryID, itemTemplate))
            return false;

        bool result = ::Database::Util::Item::ItemInstanceCreate(transaction, itemEntryID, ownerID, 1, itemTemplate->durability, itemInstanceID);
        if (!result)
            return false;

        Cache::ItemInstanceCreate(gameCache, itemInstanceID, 
        {
            .id = itemInstanceID,
            .ownerID = ownerID,
            .itemID = itemEntryID,
            .count = 1,
            .durability = static_cast<u16>(itemTemplate->durability)
        });
        
        return true;
    }

    bool ItemDelete(pqxx::work& transaction, entt::registry& registry, u64 itemInstanceID)
    {
        auto& gameCache = registry.ctx().get<Singletons::GameCache>();

        if (!Cache::ItemInstanceExistsByID(gameCache, itemInstanceID))
            return false;

        bool result = ::Database::Util::Item::ItemInstanceDestroy(transaction, itemInstanceID);
        if (!result)
            return false;

        Cache::ItemInstanceDelete(gameCache, itemInstanceID);

        return true;
    }
}