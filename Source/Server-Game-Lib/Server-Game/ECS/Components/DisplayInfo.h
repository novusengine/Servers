#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

namespace ECS::Components
{
    struct DisplayInfo
    {
    public:
        u32 displayID;

        GameDefine::UnitRace unitRace = GameDefine::UnitRace::None;
        GameDefine::UnitGender unitGender = GameDefine::UnitGender::None;
    };
}