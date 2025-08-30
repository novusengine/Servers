#include "ProximityTriggerUtil.h"

#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"

namespace ECS::Util::ProximityTrigger
{
    bool AddPlayerToTrigger(entt::registry& registry, entt::entity triggerEntity, entt::entity playerEntity)
    {
        auto& ProximityTrigger = registry.get<Components::ProximityTrigger>(triggerEntity);
        ProximityTrigger.playersEntered.insert(playerEntity);

        return true;
    }

    bool RemovePlayerFromTrigger(entt::registry& registry, entt::entity triggerEntity, entt::entity playerEntity)
    {
        auto& ProximityTrigger = registry.get<Components::ProximityTrigger>(triggerEntity);
        ProximityTrigger.playersExited.insert(playerEntity);

        return true;
    }
}