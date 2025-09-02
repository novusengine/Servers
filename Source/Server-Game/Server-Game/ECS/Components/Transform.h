#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct MovementFlags
    {
    public:
        u32 forward : 1 = 0;
        u32 backward : 1 = 0;
        u32 left : 1 = 0;
        u32 right : 1 = 0;
        u32 flying : 1 = 0;
        u32 grounded : 1 = 0;
        u32 jumping : 1 = 0;
        u32 justGrounded : 1 = 0;
        u32 justEndedJump : 1 = 0;
    };

    struct Transform
    {
    public:
        u16 mapID = 0;
        vec3 position = vec3(0.0f, 0.0f, 0.0f);
        vec2 pitchYaw = vec2(0.0f, 0.0f); // Pitch and Yaw in radians
        vec3 scale = vec3(1.0f, 1.0f, 1.0f);
    };
}