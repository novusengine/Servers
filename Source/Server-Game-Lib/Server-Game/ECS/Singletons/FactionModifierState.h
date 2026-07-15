#pragma once

#include <Base/Types.h>

namespace ECS::Singletons
{
    struct FactionModifierState
    {
    public:
        u64 nextApplicationSequence = 1;
    };
}
