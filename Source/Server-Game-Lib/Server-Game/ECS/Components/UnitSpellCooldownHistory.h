#pragma once
#include <Base/Types.h>

#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct UnitSpellCooldownHistory
    {
    public:
        robin_hood::unordered_map<u32, f32> spellIDToCooldown;
    };
}