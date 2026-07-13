#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct MovementPath;
    }
}

namespace ECS::Util::Movement
{
    struct FollowParams
    {
    public:
        f32 speed = 7.1111f;
        f32 repathInterval = 0.25f;
        f32 repathDistance = 2.0f;
    };

    struct PointParams
    {
    public:
        f32 speed = 7.1111f;
        f32 repathDistance = 0.25f;
        bool clearOnReach = true;
    };

    struct WanderParams
    {
    public:
        f32 speed = 3.5555f;
        f32 radius = 5.0f;
        f32 minPause = 3.0f;
        f32 maxPause = 6.0f;
        f32 repathDistance = 0.25f;
    };

    void Follow(World& world, entt::entity entity, entt::entity target, const FollowParams& params = {});
    void MoveTo(World& world, entt::entity entity, const vec3& position, const PointParams& params = {});
    void Wander(World& world, entt::entity entity, const vec3& center, const WanderParams& params = {});
    void Stop(World& world, entt::entity entity);
    void ResetPath(World& world, entt::entity entity, Components::MovementPath& movementPath, u32 intentRevision);

    void QueuePathRequest(World& world, entt::entity entity, f32 delay, u32 retryCount = 0);
    void CancelPathRequest(World& world, entt::entity entity);
    void AdvancePathRequests(World& world, f32 deltaTime);
    bool PopDuePathRequest(World& world, entt::entity& entity, u32& intentRevision, u32& retryCount);
}
