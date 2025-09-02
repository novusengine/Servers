#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct CreatureInfo
    {
    public:
        u32 templateID = 0;
        std::string name;
    };
}