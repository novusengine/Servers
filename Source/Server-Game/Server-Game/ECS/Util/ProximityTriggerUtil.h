#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS
{
    namespace Components
    {
        struct Transform;
        struct AABB;
    }
}

namespace ECS::Util::ProximityTrigger
{
    bool AddPlayerToTrigger(entt::registry& registry, entt::entity triggerEntity, entt::entity playerEntity);
    bool RemovePlayerFromTrigger(entt::registry& registry, entt::entity triggerEntity, entt::entity playerEntity);
}