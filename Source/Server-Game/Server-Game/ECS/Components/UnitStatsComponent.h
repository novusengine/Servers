#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Shared/UnitEnum.h>

#include <robinhood/robinhood.h>

namespace ECS
{
    struct UnitStat
    {
    public:
        f64 base = 0.0;
        f64 current = 0.0;
    };

    namespace Components
    {
        struct UnitStatsComponent
        {
        public:
            robin_hood::unordered_map<Generated::StatTypeEnum, UnitStat> statTypeToValue;
            robin_hood::unordered_set<Generated::StatTypeEnum> dirtyStatTypes;
        };
    }
}