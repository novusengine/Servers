#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

#include <robinhood/robinhood.h>

namespace ECS
{
    namespace Components
    {
        struct UnitAuraInfo
        {
        public:
            robin_hood::unordered_map<u32, entt::entity> spellIDToAuraEntity;
            robin_hood::unordered_map<u32, u64> procIDToLastProcTime;
        };
    }
}