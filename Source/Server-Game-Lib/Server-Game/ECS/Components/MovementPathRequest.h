#pragma once

#include <Base/Types.h>

#include <entt/entity/entity.hpp>

#include <limits>

namespace ECS::Components
{
    struct MovementPathRequest
    {
    public:
        f64 dueTime = 0.0;
        u64 sequence = 0;
        u32 intentRevision = 0;
        u32 retryCount = 0;
        u32 heapIndex = std::numeric_limits<u32>::max();
    };
}
