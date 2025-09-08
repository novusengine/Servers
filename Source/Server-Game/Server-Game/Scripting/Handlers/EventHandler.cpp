#include "EventHandler.h"

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
        if (numArgs != 2)
            return 0;

        u32 packedEventID = zenith->CheckVal<u32>(1);
        if (packedEventID == std::numeric_limits<u16>().max())
            return 0;

        i32 funcRef = zenith->GetRef(2);
        if (funcRef == 0)
            return 0;

        u16 eventTypeID = static_cast<u16>(packedEventID >> 16);
        u16 eventID = static_cast<u16>(packedEventID & 0xFFFF);

        zenith->RegisterEventCallbackRaw(eventTypeID, eventID, funcRef);

        return 0;
    }

    void EventHandler::CreateEventTables(Zenith* zenith)
    {
        zenith->RegisterEventType<Generated::LuaServerEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaServerEventDataLoaded>(Generated::LuaServerEventEnum::Loaded);
        zenith->RegisterEventTypeID<Generated::LuaServerEventDataUpdated>(Generated::LuaServerEventEnum::Updated);

        zenith->RegisterEventType<Generated::LuaTriggerEventEnum>();
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnEnter>(Generated::LuaTriggerEventEnum::OnEnter);
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnExit>(Generated::LuaTriggerEventEnum::OnExit);
        zenith->RegisterEventTypeID<Generated::LuaTriggerEventDataOnStay>(Generated::LuaTriggerEventEnum::OnStay);

        zenith->RegisterEventType<Generated::PacketListEnum>();
        for (const auto& pair : Generated::PacketListEnumMeta::EnumList)
        {
            zenith->RegisterEventTypeIDRaw(Generated::PacketListEnumMeta::EnumID, pair.second, 0);
        }
    }
}
