#pragma once

#include "Server-Game/Gameplay/Faction/CreatureFactionPolicyDefines.h"

namespace ECS::Components
{
    struct CreatureFactionPolicy
    {
    public:
        Gameplay::Faction::CreatureAggressionPolicy aggression = Gameplay::Faction::CreatureAggressionPolicy::Defensive;
        Gameplay::Faction::CreatureAssistancePolicy assistance = Gameplay::Faction::CreatureAssistancePolicy::None;
        f32 detectionRange = 20.0f;
        f32 assistanceRange = 20.0f;
        f32 timeToNextAcquisition = 0.0f;
    };
}
