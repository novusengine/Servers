#pragma once
#include <Base/Types.h>

#include <MetaGen/Shared/ProximityTrigger/ProximityTrigger.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct ProximityTrigger
    {
    public:
        u32 triggerID;
        std::string name;
        MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum flags = MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::None;
        robin_hood::unordered_set<entt::entity> playersInside; // Entities currently inside the trigger
        robin_hood::unordered_set<entt::entity> playersEntered; // Entities that just entered
        robin_hood::unordered_set<entt::entity> playersExited; // Entities that just entered
    };
}