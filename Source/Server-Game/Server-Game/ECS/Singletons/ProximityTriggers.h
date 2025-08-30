#pragma once
#include <Base/Types.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>
#include <RTree/RTree.h>

namespace ECS
{
    namespace Singletons
    {
        struct ProximityTriggers
        {
        public:
            robin_hood::unordered_map<u32, entt::entity> triggerIDToEntity;
            robin_hood::unordered_set<u32> triggerIDsToDestroy;

            robin_hood::unordered_set<u32> activeTriggers;
            RTree<u32, f32, 3> activeTriggersVisTree;
        };
    }
}