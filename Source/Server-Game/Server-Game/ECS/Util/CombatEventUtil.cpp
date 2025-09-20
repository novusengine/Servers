#include "CombatEventUtil.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"

namespace ECS::Util::CombatEvent
{
    void AddDamageEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 amount, u32 amountAbsorbed, u32 amountResisted, u32 amountBlocked, u32 spellId)
    {
        ECS::CombatEventInfo& eventInfo = combatEventState.combatEvents.emplace_back();

        eventInfo.eventType = ECS::CombatEventType::Damage;
        eventInfo.sourceGUID = sourceGUID;
        eventInfo.targetGUID = targetGUID;
        eventInfo.spellID = spellId;
        eventInfo.amount = amount;
        eventInfo.absorbed = amountAbsorbed;
        eventInfo.resisted = amountResisted;
        eventInfo.blocked = amountBlocked;
    }

    void AddHealEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 amount, u32 amountAbsorbed, u32 spellId)
    {
        ECS::CombatEventInfo& eventInfo = combatEventState.combatEvents.emplace_back();

        eventInfo.eventType = ECS::CombatEventType::Heal;
        eventInfo.sourceGUID = sourceGUID;
        eventInfo.targetGUID = targetGUID;
        eventInfo.spellID = spellId;
        eventInfo.amount = amount;
        eventInfo.absorbed = amountAbsorbed;
    }

    void AddResurrectEvent(Singletons::CombatEventState& combatEventState, ObjectGUID sourceGUID, ObjectGUID targetGUID, u32 spellId)
    {
        ECS::CombatEventInfo& eventInfo = combatEventState.combatEvents.emplace_back();

        eventInfo.eventType = ECS::CombatEventType::Resurrect;
        eventInfo.sourceGUID = sourceGUID;
        eventInfo.targetGUID = targetGUID;
        eventInfo.spellID = spellId;
    }
}
