#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct MovementFlags
    {
        u32 forward : 1;
        u32 backward : 1;
        u32 left : 1;
        u32 right : 1;
        u32 grounded : 1;
    };

    struct Transform
    {
    public:
        vec3 position = vec3(0.0f, 0.0f, 0.0f);
        quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
        vec3 scale = vec3(1.0f, 1.0f, 1.0f);
    };
}