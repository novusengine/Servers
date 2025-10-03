#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct AuraInfo
    {
    public:
        struct Flags
        {
            u16 isServerSide    : 1;
            u16 isPassive       : 1;
            u16 isBuff          : 1;
            u16 isUncancellable : 1;
            u16 reserved        : 12;
        };

    public:
        ObjectGUID caster;
        ObjectGUID target;

        Flags flags;
        u16 stacks;

        u32 spellID;
        f32 duration;
        f32 timeRemaining;
    };
}