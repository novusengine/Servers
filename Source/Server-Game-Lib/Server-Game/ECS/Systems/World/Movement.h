#pragma once
#include <Base/Types.h>

namespace ECS
{
    struct World;

    namespace Singletons
    {
        struct NetworkState;
        struct TimeState;
    }

    namespace Systems
    {
        class Movement
        {
        public:
            static void HandleUpdate(World& world, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
        };
    }
}
