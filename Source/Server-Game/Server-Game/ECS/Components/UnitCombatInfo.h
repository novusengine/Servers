#pragma once
#include <Base/Types.h>

#include <robinhood/robinhood.h>

namespace ECS
{
    struct ThreatTableEntry
    {
    public:
        f32 timeSinceLastActivity = 0.0f;
    };

    namespace Components
    {
        struct UnitCombatInfo
        {
        public:
            f32 threatModifier = 1.0f;
            robin_hood::unordered_map<ObjectGUID, ThreatTableEntry> threatTables;
        };
    }
}