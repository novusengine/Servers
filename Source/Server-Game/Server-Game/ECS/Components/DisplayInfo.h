#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

namespace ECS::Components
{
    struct DisplayInfo
    {
    public:
        GameDefine::Race race = GameDefine::Race::None;
        GameDefine::Gender gender = GameDefine::Gender::None;

        u32 displayID;
    };
}