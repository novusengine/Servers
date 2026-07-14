#pragma once
#include <Base/Types.h>

#include <entt/entity/entity.hpp>

namespace ECS::Components
{
    enum class MovementIntentType : u8
    {
        None,
        Follow,
        Point,
        Wander
    };

    struct MovementFollowIntent
    {
    public:
        entt::entity target = entt::null;
        f32 repathInterval = 0.25f;
        f32 repathDistance = 2.0f;
        f32 stopDistance = 2.0f;
    };

    struct MovementPointIntent
    {
    public:
        vec3 position = vec3(0.0f);
        f32 repathDistance = 0.25f;
        bool clearOnReach = true;
    };

    struct MovementWanderIntent
    {
    public:
        vec3 center = vec3(0.0f);
        f32 radius = 0.0f;
        f32 minPause = 1.0f;
        f32 maxPause = 3.0f;
        f32 repathDistance = 0.25f;
    };

    struct MovementIntent
    {
    public:
        MovementIntentType type = MovementIntentType::None;
        MovementFollowIntent follow;
        MovementPointIntent point;
        MovementWanderIntent wander;
        f32 speed = 7.1111f;
        u32 revision = 0;
    };
}
