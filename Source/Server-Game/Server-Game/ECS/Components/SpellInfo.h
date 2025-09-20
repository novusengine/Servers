#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct SpellInfo
    {
    public:
        u32 spellID = 0;

        ObjectGUID caster;
        ObjectGUID target;

        f32 castTime = 0.0f;
        f32 timeToCast = 0.0f;
    };
}