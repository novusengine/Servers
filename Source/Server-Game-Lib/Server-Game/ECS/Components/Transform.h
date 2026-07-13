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

        constexpr u32 ToBitMask() const
        {
            return (forward << 0) |
                (backward << 1) |
                (left << 2) |
                (right << 3) |
                (flying << 4) |
                (grounded << 5) |
                (jumping << 6) |
                (justGrounded << 7) |
                (justEndedJump << 8);
        }
    };

    struct Transform
    {
    public:
        u32 mapID = 0;
        vec3 position = vec3(0.0f, 0.0f, 0.0f);
        vec2 pitchYaw = vec2(0.0f, 0.0f); // Pitch and Yaw in radians
        vec3 scale = vec3(1.0f, 1.0f, 1.0f);
    };
}
