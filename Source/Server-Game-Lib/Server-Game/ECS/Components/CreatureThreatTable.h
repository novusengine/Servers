#pragma once
#include <Base/Types.h>

#include <robinhood/robinhood.h>

namespace ECS
{
    struct ThreatEntry
    {
        ObjectGUID guid = ObjectGUID::Empty;
        f64 threatValue = 0.0f;
    };

    namespace Components
    {
        struct CreatureThreatTable
        {
        public:
            bool allowDropCombat = true;

            std::vector<ThreatEntry> threatList;
            robin_hood::unordered_map<ObjectGUID, u32> guidToIndex;
        };
    }
}