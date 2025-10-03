#pragma once
#include <Base/Types.h>

namespace ECS
{
    struct AuraEffect
    {
    public:
        u8 type;

        i32 value1;
        i32 value2;
        i32 value3;

        i32 miscValue1;
        i32 miscValue2;
        i32 miscValue3;
    };

    struct AuraPeriodicInfo
    {
    public:
        f32 interval;
        f32 timeToNextTick;

        u8 index;
    };

    namespace Components
    {
        struct AuraEffectInfo
        {
        public:
            u64 periodicEffectsMask = 0;
            std::vector<AuraEffect> effects;
            std::vector<AuraPeriodicInfo> periodicEffects;
        };
    }
}