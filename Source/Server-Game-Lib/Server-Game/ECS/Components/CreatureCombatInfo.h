#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct CreatureCombatInfo
    {
    public:
        vec3 homePosition = vec3(0.0f);
        f32 homeOrientation = 0.0f;
        f32 leashRange = 40.0f;
        f32 meleeDamage = 1.0f;
        f32 meleeAttackTime = 2.0f;
        f32 timeToNextMeleeAttack = 0.0f;
        u16 damageSchool = 0;
        bool isEvading = false;
    };
}
