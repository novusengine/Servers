#pragma once
#include <Base/Types.h>

namespace ECS
{
    namespace Singletons
    {
        struct CombatEventState;
    }

    namespace Util::CombatEvent
    {
        void AddDamageEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 amount, u32 amountAbsorbed = 0, u32 amountResisted = 0, u32 amountBlocked = 0, u32 spellId = 0);
        void AddHealEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 amount, u32 amountAbsorbed = 0, u32 spellId = 0);
        void AddResurrectEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 spellId = 0);
    }
}