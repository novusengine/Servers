#include "PacketEventHandler.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/Network/Packet.h"
#include "Server-Game/Scripting/Systems/LuaSystemBase.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Meta/Generated/Shared/NetworkPacket.h>

#include <lualib.h>

namespace Scripting
{
    void PacketEventHandler::Register(lua_State* state)
    {
        // Register Functions
        {
            LuaMethodTable::Set(state, packetEventMethods);
        }

        // Set Event Handlers
        {
        }

        CreatePacketEventTable(state);
        CreateGameOpcodeTable(state);

        Network::Packet::Register(state);
    }

    void PacketEventHandler::SetEventHandler(u32 eventID, EventHandlerFn fn)
    {
        LuaPacketEvent packetEventID = static_cast<LuaPacketEvent>(eventID);

        if (packetEventID == LuaPacketEvent::Invalid || packetEventID >= LuaPacketEvent::Count)
        {
            return;
        }

        _eventToFuncHandlerList[eventID] = fn;
    }
    void PacketEventHandler::CallEvent(lua_State* state, u32 eventID, LuaEventData* data)
    {
        if (eventID >= _eventToFuncHandlerList.size())
            return;

        LuaPacketEvent packetEventID = static_cast<LuaPacketEvent>(eventID);

        if (packetEventID == LuaPacketEvent::Invalid || packetEventID >= LuaPacketEvent::Count)
        {
            return;
        }

        EventHandlerFn& fn = _eventToFuncHandlerList[eventID];
        if (fn)
            fn(state, eventID, data);
    }
    void PacketEventHandler::RegisterEventCallback(lua_State* state, u32 eventID, i32 funcHandle)
    {
        LuaManager* luaManager = ServiceLocator::GetLuaManager();

        u32 id = eventID;
        u64 key = reinterpret_cast<u64>(state);

        std::vector<i32>& funcRefList = _eventToLuaStateFuncRefList[id][key];
        funcRefList.push_back(funcHandle);
    }

    void PacketEventHandler::RegisterHandlerCallback(lua_State* state, ::Network::OpcodeType opcode, i32 funcHandle)
    {
        u64 key = reinterpret_cast<u64>(state);
        _luaStateToOpcodeFuncRef[key][opcode] = funcHandle;
    }

    i32 PacketEventHandler::RegisterPacketHandler(lua_State* state)
    {
        LuaState ctx(state);

        ::Network::OpcodeType opcode = ctx.Get(0u, 1);

        if (opcode == ::Generated::InvalidPacket::PACKET_ID || opcode >= 1024)
        {
            return 0;
        }

        if (!lua_isfunction(ctx.RawState(), 2))
        {
            return 0;
        }

        i32 funcHandle = ctx.GetRef(2);

        LuaManager* luaManager = ServiceLocator::GetLuaManager();
        auto packetEventHandler = luaManager->GetLuaHandler<PacketEventHandler>(LuaHandlerType::PacketEvent);
        packetEventHandler->RegisterHandlerCallback(state, opcode, funcHandle);

        return 0;
    }

    bool PacketEventHandler::HasGameOpcodeHandler(lua_State* state, ::Network::OpcodeType opcode) const
    {
        u64 key = reinterpret_cast<u64>(state);
        if (!_luaStateToOpcodeFuncRef.contains(key))
            return false;

        const auto& opcodeFuncRef = _luaStateToOpcodeFuncRef.at(key);
        return opcodeFuncRef.contains(opcode);
    }

    bool PacketEventHandler::CallGameOpcodeHandler(lua_State* state, ::Network::MessageHeader& header, ::Network::Message& message)
    {
        LuaState ctx(state);

        u64 key = reinterpret_cast<u64>(state);

        i32 funcRef = _luaStateToOpcodeFuncRef[key][header.opcode];
        ctx.GetRawI(LUA_REGISTRYINDEX, funcRef);
        ctx.Push((u32)header.opcode);
        ctx.Push((u32)header.size);

        auto* packet = Scripting::Network::Packet::Create(state, header, message.buffer);
        if (!ctx.PCall(3, 1))
            return false;

        bool success = ctx.Get(false);
        ctx.Pop();

        return success;
    }
    
    i32 PacketEventHandler::RegisterPacketEvent(lua_State* state)
    {
        LuaState ctx(state);

        u32 eventID = ctx.Get(0u, 1);

        LuaPacketEvent packetEventID = static_cast<LuaPacketEvent>(eventID);
        if (packetEventID == LuaPacketEvent::Invalid || packetEventID >= LuaPacketEvent::Count)
        {
            return 0;
        }

        if (!lua_isfunction(ctx.RawState(), 2))
        {
            return 0;
        }

        i32 funcHandle = ctx.GetRef(2);

        LuaManager* luaManager = ServiceLocator::GetLuaManager();
        auto packetEventHandler = luaManager->GetLuaHandler<PacketEventHandler>(LuaHandlerType::PacketEvent);
        packetEventHandler->RegisterEventCallback(state, eventID, funcHandle);

        return 0;
    }

    void PacketEventHandler::CreatePacketEventTable(lua_State* state)
    {
        LuaState ctx(state);

        ctx.CreateTableAndPopulate("PacketEvent", [&]()
        {
            u32 value = 0;
            ctx.SetTable("Invalid", value++);
            ctx.SetTable("OnReceived", value++);
            ctx.SetTable("Count", value);
        });
    }
    void PacketEventHandler::CreateGameOpcodeTable(lua_State* state)
    {
        LuaState ctx(state);

        //ctx.CreateTableAndPopulate(Generated::GameOpcodeEnumMeta::EnumName.data(), [&]()
        //{
        //    for (const auto& pair : Generated::GameOpcodeEnumMeta::EnumList)
        //    {
        //        ctx.SetTable(pair.first.data(), pair.second);
        //    }
        //});
    }
}
