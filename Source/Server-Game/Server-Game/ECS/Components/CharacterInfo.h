#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

namespace ECS::Components
{
    struct CharacterInfo
    {
    public:
        std::string name;

        u32 accountID;
        u32 totalTime;
        u32 levelTime;
        u32 logoutTime;
        u32 flags;
        
        GameDefine::UnitClass unitClass;
        u16 level;
        u64 experiencePoints;
    };
}