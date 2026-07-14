#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

namespace ECS::Components
{
    struct CreatureInfo
    {
    public:
        u32 templateID = 0;
        std::string name;
        GameDefine::UnitClass unitClass = GameDefine::UnitClass::None;
        u16 level = 1;
        f32 walkSpeed = 0.0f;
        f32 runSpeed = 0.0f;
        f32 wanderDistance = 0.0f;
        u16 movementType = 0;
    };
}
