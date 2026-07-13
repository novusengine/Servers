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
    };
}