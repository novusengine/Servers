#include "EventHandler.h"
#include "Events/AuraEvents.h"
#include "Events/CharacterEvents.h"
#include "Events/CreatureAIEvents.h"
#include "Events/SpellEvents.h"
#include "Events/UnitEvents.h"

#include <Meta/Generated/Server/LuaEvent.h>
#include <Meta/Generated/Shared/PacketList.h>

#include <Scripting/LuaManager.h>
#include <Scripting/Zenith.h>

namespace Scripting
{
    void EventHandler::Register(Zenith* zenith)
    {
        // Register Functions
        {
            LuaMethodTable::Set(zenith, EventHandlerGlobalMethods);
        }

        CreateEventTables(zenith);
    }

    i32 EventHandler::RegisterEventHandler(Zenith* zenith)
    {
        u32 numArgs = zenith->GetTop();
        if (numArgs < 2 || numArgs > 3)
            return 0;

        u32 packedEventID = zenith->CheckVal<u32>(1);
        if (packedEventID == std::numeric_limits<u16>().max())
            return 0;

        u32 variant = 0;
        i32 funcRef = 0;

        if (numArgs == 2)
        {
            if (zenith->IsFunction(2))
                funcRef = zenith->GetRef(2);
        }
        else
        {
            variant = zenith->CheckVal<u32>(2);

            if (zenith->IsFunction(3))
                funcRef = zenith->GetRef(3);
        }

        if (funcRef == 0)
            return 0;

        u16 eventTypeID = static_cast<u16>(packedEventID >> 16);
        u16 eventID = static_cast<u16>(packedEventID & 0xFFFF);
        u16 variantID = static_cast<u16>(variant);

        zenith->RegisterEventCallbackRaw(eventTypeID, eventID, variantID, funcRef);

        return 0;
    }

    void EventHandler::CreateEventTables(Zenith* zenith)
    {
        zenith->RegisterEventType<Generated::LuaServerEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaServerEventDataLoaded>(Generated::LuaServerEventEnum::Loaded);
        zenith->RegisterEventTypeID<Generated::LuaServerEventDataUpdated>(Generated::LuaServerEventEnum::Updated);

        zenith->RegisterEventType<Generated::LuaCharacterEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaCharacterEventDataOnLogin>(Generated::LuaCharacterEventEnum::OnLogin, &Event::Character::OnCharacterLogin);
        zenith->RegisterEventTypeID<Generated::LuaCharacterEventDataOnLogout>(Generated::LuaCharacterEventEnum::OnLogout, &Event::Character::OnCharacterLogout);
        zenith->RegisterEventTypeID<Generated::LuaCharacterEventDataOnWorldChanged>(Generated::LuaCharacterEventEnum::OnWorldChanged, &Event::Character::OnCharacterWorldChanged);

        zenith->RegisterEventType<Generated::LuaTriggerEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnEnter>(Generated::LuaTriggerEventEnum::OnEnter);
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnExit>(Generated::LuaTriggerEventEnum::OnExit);
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnStay>(Generated::LuaTriggerEventEnum::OnStay);

        zenith->RegisterEventType<Generated::LuaSpellEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaSpellEventDataOnPrepare>(Generated::LuaSpellEventEnum::OnPrepare, &Event::Spell::OnSpellPrepare);
        zenith->RegisterEventTypeID<Generated::LuaSpellEventDataOnHandleEffect>(Generated::LuaSpellEventEnum::OnHandleEffect, &Event::Spell::OnSpellHandleEffect);
        zenith->RegisterEventTypeID<Generated::LuaSpellEventDataOnFinish>(Generated::LuaSpellEventEnum::OnFinish, &Event::Spell::OnSpellFinish);

        zenith->RegisterEventType<Generated::LuaAuraEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaAuraEventDataOnApply>(Generated::LuaAuraEventEnum::OnApply, &Event::Aura::OnAuraApply);
        zenith->RegisterEventTypeID<Generated::LuaAuraEventDataOnRemove>(Generated::LuaAuraEventEnum::OnRemove, &Event::Aura::OnAuraRemove);
        zenith->RegisterEventTypeID<Generated::LuaAuraEventDataOnHandleEffect>(Generated::LuaAuraEventEnum::OnHandleEffect, &Event::Aura::OnAuraHandleEffect);

        zenith->RegisterEventType<Generated::LuaCreatureAIEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnInit>(Generated::LuaCreatureAIEventEnum::OnInit, &Event::CreatureAIEvents::OnCreatureAIInit);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnDeinit>(Generated::LuaCreatureAIEventEnum::OnDeinit, &Event::CreatureAIEvents::OnCreatureAIDeinit);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnEnterCombat>(Generated::LuaCreatureAIEventEnum::OnEnterCombat, &Event::CreatureAIEvents::OnCreatureAIOnEnterCombat);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnLeaveCombat>(Generated::LuaCreatureAIEventEnum::OnLeaveCombat, &Event::CreatureAIEvents::OnCreatureAIOnLeaveCombat);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnUpdate>(Generated::LuaCreatureAIEventEnum::OnUpdate, &Event::CreatureAIEvents::OnCreatureAIUpdate);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnResurrect>(Generated::LuaCreatureAIEventEnum::OnResurrect, &Event::CreatureAIEvents::OnCreatureAIOnResurrect);
        zenith->RegisterEventTypeID<Generated::LuaCreatureAIEventDataOnDied>(Generated::LuaCreatureAIEventEnum::OnDied, &Event::CreatureAIEvents::OnCreatureAIOnDied);

        zenith->RegisterEventType<Generated::PacketListEnum>();
        for (const auto& pair : Generated::PacketListEnumMeta::EnumList)
        {
            zenith->RegisterEventTypeIDRaw(Generated::PacketListEnumMeta::EnumID, pair.second, 0);
        }
    }
}
