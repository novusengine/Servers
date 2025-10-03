#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct SpellInfo
    {
    public:
        ObjectGUID caster;
        ObjectGUID target;

        u32 spellID = 0;
        f32 castTime = 0.0f;
        f32 timeToCast = 0.0f;
    };
}