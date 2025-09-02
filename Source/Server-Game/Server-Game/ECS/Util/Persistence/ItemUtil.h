#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>
#include <pqxx/transaction>

namespace ECS
{
    namespace Singletons
    {
        struct GameCache;
    }

    namespace Util::Persistence::Item
    {
        bool ItemCreate(pqxx::work& transaction, Singletons::GameCache& gameCache, u32 itemEntryID, u64 ownerID, u64& itemInstanceID);
        bool ItemDelete(pqxx::work& transaction, Singletons::GameCache& gameCache, u64 itemInstanceID);
    }
}