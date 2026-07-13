#include "EventHandler.h"
#include "Events/AuraEvents.h"
#include "Events/CharacterEvents.h"
#include "Events/CreatureAIEvents.h"
#include "Events/SpellEvents.h"
#include "Events/UnitEvents.h"

#include <MetaGen/EnumTraits.h>
#include <MetaGen/PacketList.h>
#include <MetaGen/Server/Lua/Lua.h>

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
        zenith->RegisterEventType<MetaGen::Server::Lua::ServerEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::ServerEventDataLoaded>(MetaGen::Server::Lua::ServerEvent::Loaded);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::ServerEventDataUpdated>(MetaGen::Server::Lua::ServerEvent::Updated);

        zenith->RegisterEventType<MetaGen::Server::Lua::CharacterEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CharacterEventDataOnLogin>(MetaGen::Server::Lua::CharacterEvent::OnLogin, &Event::Character::OnCharacterLogin);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CharacterEventDataOnLogout>(MetaGen::Server::Lua::CharacterEvent::OnLogout, &Event::Character::OnCharacterLogout);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CharacterEventDataOnWorldChanged>(MetaGen::Server::Lua::CharacterEvent::OnWorldChanged, &Event::Character::OnCharacterWorldChanged);

        zenith->RegisterEventType<MetaGen::Server::Lua::TriggerEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::TriggerEventDataOnEnter>(MetaGen::Server::Lua::TriggerEvent::OnEnter);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::TriggerEventDataOnExit>(MetaGen::Server::Lua::TriggerEvent::OnExit);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::TriggerEventDataOnStay>(MetaGen::Server::Lua::TriggerEvent::OnStay);

        zenith->RegisterEventType<MetaGen::Server::Lua::SpellEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::SpellEventDataOnPrepare>(MetaGen::Server::Lua::SpellEvent::OnPrepare, &Event::Spell::OnSpellPrepare);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::SpellEventDataOnHandleEffect>(MetaGen::Server::Lua::SpellEvent::OnHandleEffect, &Event::Spell::OnSpellHandleEffect);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::SpellEventDataOnFinish>(MetaGen::Server::Lua::SpellEvent::OnFinish, &Event::Spell::OnSpellFinish);

        zenith->RegisterEventType<MetaGen::Server::Lua::AuraEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::AuraEventDataOnApply>(MetaGen::Server::Lua::AuraEvent::OnApply, &Event::Aura::OnAuraApply);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::AuraEventDataOnRemove>(MetaGen::Server::Lua::AuraEvent::OnRemove, &Event::Aura::OnAuraRemove);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::AuraEventDataOnHandleEffect>(MetaGen::Server::Lua::AuraEvent::OnHandleEffect, &Event::Aura::OnAuraHandleEffect);

        zenith->RegisterEventType<MetaGen::Server::Lua::CreatureAIEvent>();
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnInit>(MetaGen::Server::Lua::CreatureAIEvent::OnInit, &Event::CreatureAIEvents::OnCreatureAIInit);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnDeinit>(MetaGen::Server::Lua::CreatureAIEvent::OnDeinit, &Event::CreatureAIEvents::OnCreatureAIDeinit);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnEnterCombat>(MetaGen::Server::Lua::CreatureAIEvent::OnEnterCombat, &Event::CreatureAIEvents::OnCreatureAIOnEnterCombat);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnLeaveCombat>(MetaGen::Server::Lua::CreatureAIEvent::OnLeaveCombat, &Event::CreatureAIEvents::OnCreatureAIOnLeaveCombat);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnUpdate>(MetaGen::Server::Lua::CreatureAIEvent::OnUpdate, &Event::CreatureAIEvents::OnCreatureAIUpdate);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnResurrect>(MetaGen::Server::Lua::CreatureAIEvent::OnResurrect, &Event::CreatureAIEvents::OnCreatureAIOnResurrect);
        zenith->RegisterEventTypeID<MetaGen::Server::Lua::CreatureAIEventDataOnDied>(MetaGen::Server::Lua::CreatureAIEvent::OnDied, &Event::CreatureAIEvents::OnCreatureAIOnDied);

        zenith->RegisterEventType<MetaGen::PacketListEnum>();
        for (const auto& pair : MetaGen::PacketListEnumMeta::ENUM_FIELD_LIST)
        {
            zenith->RegisterEventTypeIDRaw(MetaGen::PacketListEnumMeta::ENUM_ID, pair.second, 0);
        }
    }
}
