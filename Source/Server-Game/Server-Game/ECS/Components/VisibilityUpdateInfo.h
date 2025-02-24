#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct VisibilityUpdateInfo
    {
    public:
        f32 updateInterval = 0.0f;
        f32 timeSinceLastUpdate = 0.0f;
    };
}