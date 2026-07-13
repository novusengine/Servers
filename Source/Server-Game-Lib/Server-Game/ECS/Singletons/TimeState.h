#pragma once
#include <Base/Types.h>

namespace ECS
{
    namespace Singletons
    {
        struct TimeState
        {
        public:
            f32 deltaTime = 0.0f;
            u64 epochAtFrameStart = 0;

            u64 tickCount = 0;
        };
    }
}