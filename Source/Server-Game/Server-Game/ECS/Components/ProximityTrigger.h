#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Game/ProximityTriggerEnum.h>
DECLARE_GENERIC_BITWISE_OPERATORS(Generated::ProximityTriggerFlagEnum);

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct ProximityTrigger
    {
    public:
        u32 triggerID;
        std::string name;
        Generated::ProximityTriggerFlagEnum flags = Generated::ProximityTriggerFlagEnum::None;
        robin_hood::unordered_set<entt::entity> playersInside; // Entities currently inside the trigger
        robin_hood::unordered_set<entt::entity> playersEntered; // Entities that just entered
        robin_hood::unordered_set<entt::entity> playersExited; // Entities that just entered
    };
}