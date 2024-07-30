#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    enum class CombatLogEvents : u16
    {
        DamageDealt = 0,
        HealingDone = 1,
        Ressurected = 2,
    };
}