#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>
#include <pqxx/transaction>

namespace ECS::Util::Persistence::Item
{
    bool ItemCreate(pqxx::work& transaction, entt::registry& registry, u32 itemEntryID, u64 ownerID, u64& itemInstanceID);
    bool ItemDelete(pqxx::work& transaction, entt::registry& registry, u64 itemInstanceID);
}