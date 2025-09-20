#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct CreatureAIInfo
    {
    public:
        f32 timeToNextUpdate = 0.0f;
        std::string scriptName;
    };
}