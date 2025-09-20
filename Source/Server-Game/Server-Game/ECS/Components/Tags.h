#pragma once
#include <Base/Types.h>

namespace ECS::Tags
{
    struct IsPlayer {};
    struct IsCreature {};
    struct IsInCombat {};
    struct IsMissingHealth {};
    struct IsAlive {};
    struct IsDead {};
    struct IsUnpreparedSpell {};
    struct IsActiveSpell {};
    struct IsCompleteSpell {};
    struct CharacterWasWorldTransferred {};
    struct ProximityTriggerIsServerSideOnly {};
    struct ProximityTriggerIsClientSide {};
    struct ProximityTriggerHasEnteredPlayers {};
}