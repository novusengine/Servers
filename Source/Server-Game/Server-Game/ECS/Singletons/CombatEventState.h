#pragma once
#include <Base/Types.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>
#include <RTree/RTree.h>

namespace ECS
{
    enum class CombatEventType : u8
    {
        None = 0,
        Damage = 1,
        Heal = 2,
        Resurrect = 3
    };

    struct CombatEventInfo
    {
    public:
        u64 timestamp = 0;
        CombatEventType eventType = CombatEventType::None;
        ObjectGUID sourceGUID = ObjectGUID::Empty;
        ObjectGUID targetGUID = ObjectGUID::Empty;
        
        u32 spellID = 0;
        u64 amount = 0;
        u64 absorbed = 0;
        u64 resisted = 0;
        u64 blocked = 0;
    };

    namespace Singletons
    {
        struct CombatEventState
        {
        public:
            std::deque<CombatEventInfo> combatEvents;
        };
    }
}