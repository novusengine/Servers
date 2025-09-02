#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct CastInfo
    {
        ObjectGUID caster;
        ObjectGUID target;

        f32 castTime = 0.0f;
        f32 duration = 0.0f;
    };
}