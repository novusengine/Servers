#include "NetworkUtil.h"
#include "Server-Game/Scripting/Handlers/PacketHandler.h"

#include <MetaGen/PacketList.h>

#include <Scripting/Zenith.h>

namespace Scripting::Util::Network
{
    bool HasPacketHandler(Zenith* zenith, ::Network::OpcodeType opcode)
    {
        bool hasEventCallback = zenith->HasEventCallbackRaw(MetaGen::PacketListEnumMeta::ENUM_ID, opcode, 0);
        return hasEventCallback;
    }

    bool CallPacketHandler(Zenith* zenith, ::Network::MessageHeader& header, ::Network::Message& message)
    {
        ZenithEventState& zenithEventState = zenith->eventState;

        u16 eventTypeID = MetaGen::PacketListEnumMeta::ENUM_ID;
        u16 eventDataID = 0;
        u16 eventTypeVal = header.opcode;

        auto& eventTypeMap = zenithEventState.eventTypeToState;
        if (!eventTypeMap.contains(eventTypeID))
            return false;

        EventTypeState& eventTypeState = eventTypeMap[eventTypeID];
        if (!eventTypeState.eventIDToEventState.contains(eventTypeVal))
            return false;

        EventState& eventState = eventTypeState.eventIDToEventState[eventTypeVal];
        if (eventState.eventDataID != eventDataID)
            return false;

        auto itr = eventState.eventVariantToFuncRef.find(0);
        if (itr == eventState.eventVariantToFuncRef.end())
            return false;

        auto& funcRefList = itr->second;
        if (funcRefList.empty())
            return false;

        bool result = true;
        u32 bufferReadPos = static_cast<u32>(message.buffer->readData);
        u32 packedEventID = static_cast<u32>(eventTypeVal) | (static_cast<u32>(eventTypeID) << 16);
        u32 numParametersToPush = 2; // 1 for eventID, 1 for eventData

        zenith->Push((u32)packedEventID);
        zenith->CreateTable();

        zenith->AddTableField("opcode", header.opcode);
        zenith->AddTableField("size", header.size);

        Scripting::Network::Packet::Create(zenith, header, message.buffer, bufferReadPos);
        zenith->SetTableKey("packet");

        result = zenith->CallAllFunctionsBool(funcRefList, numParametersToPush);
        message.buffer->readData = bufferReadPos;

        return result;
    }
}