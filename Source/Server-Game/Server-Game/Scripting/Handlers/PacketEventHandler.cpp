#include "PacketEventHandler.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/Network/Packet.h"
#include "Server-Game/Scripting/Systems/LuaSystemBase.h"
#include "Server-Game/Util/ServiceLocator.h"

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
            SetEventHandler(static_cast<u32>(LuaPacketEvent::OnReceive), std::bind(&PacketEventHandler::OnPacketReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

    void PacketEventHandler::RegisterHandlerCallback(lua_State* state, ::Network::GameOpcode opcode, i32 funcHandle)
    {
        u64 key = reinterpret_cast<u64>(state);
        _luaStateToOpcodeFuncRef[key][opcode] = funcHandle;
    }

    i32 PacketEventHandler::RegisterPacketHandler(lua_State* state)
    {
        LuaState ctx(state);

        u32 opcode = ctx.Get(0u, 1);

        ::Network::GameOpcode gameOpcode = static_cast<::Network::GameOpcode>(opcode);
        if (gameOpcode == ::Network::GameOpcode::Invalid || gameOpcode >= ::Network::GameOpcode::Count)
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
        packetEventHandler->RegisterHandlerCallback(state, gameOpcode, funcHandle);

        return 0;
    }

    bool PacketEventHandler::HasGameOpcodeHandler(lua_State* state, ::Network::GameOpcode opcode) const
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
        auto gameOpcode = static_cast<::Network::GameOpcode>(header.opcode);

        i32 funcRef = _luaStateToOpcodeFuncRef[key][gameOpcode];
        ctx.GetRawI(LUA_REGISTRYINDEX, funcRef);
        ctx.Push((u32)header.opcode);
        ctx.Push((u32)header.size);

        auto* packet = Scripting::Network::Packet::Create(state, header, message.buffer);
        if (!ctx.PCall(3, 1))
            return false;

        bool success = ctx.Get(false);
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

    i32 PacketEventHandler::OnPacketReceived(lua_State* state, u32 eventID, LuaEventData* data)
    {
        //LuaManager* luaManager = ServiceLocator::GetLuaManager();
        //auto eventData = reinterpret_cast<LuaPacketEventOnReceiveData*>(data);
        //
        //u32 id = eventID;
        //u64 key = reinterpret_cast<u64>(state);
        //
        //LuaState ctx(state);
        //
        //std::vector<i32>& funcRefList = _eventToLuaStateFuncRefList[id][key];
        //for (i32 funcHandle : funcRefList)
        //{
        //    ctx.GetRawI(LUA_REGISTRYINDEX, funcHandle);
        //    ctx.Push(id);
        //    //ctx.Push(eventData->motd.c_str());
        //    ctx.PCall(1);
        //}

        return 0;
    }

    void PacketEventHandler::CreatePacketEventTable(lua_State* state)
    {
        LuaState ctx(state);

        ctx.CreateTableAndPopulate("PacketEvent", [&]()
        {
            u32 value = 0;
            ctx.SetTable("Invalid", value++);
            ctx.SetTable("OnHandle", value++);
            ctx.SetTable("OnReceived", value++);
            ctx.SetTable("Count", value);
        });
    }
    void PacketEventHandler::CreateGameOpcodeTable(lua_State* state)
    {
        LuaState ctx(state);

        ctx.CreateTableAndPopulate("GameOpcode", [&]()
        {
            u32 value = 0;
            ctx.SetTable("Invalid", value++);
            ctx.SetTable("Client_Connect", value++);
            ctx.SetTable("Server_Connected", value++);

            ctx.SetTable("Shared_Ping", value++);
            ctx.SetTable("Server_UpdateStats", value++);

            ctx.SetTable("Client_SendCheatCommand", value++);
            ctx.SetTable("Server_SendCheatCommandResult", value++);

            ctx.SetTable("Server_SetMover", value++);
            ctx.SetTable("Server_EntityCreate", value++);
            ctx.SetTable("Server_EntityDestroy", value++);
            ctx.SetTable("Server_EntityDisplayInfoUpdate", value++);
            ctx.SetTable("Shared_EntityMove", value++);
            ctx.SetTable("Shared_EntityMoveStop", value++);
            ctx.SetTable("Server_EntityResourcesUpdate", value++);
            ctx.SetTable("Shared_EntityTargetUpdate", value++);
            ctx.SetTable("Server_EntityCastSpell", value++);

            ctx.SetTable("Server_ItemCreate", value++);
            ctx.SetTable("Server_ContainerCreate", value++);

            ctx.SetTable("Client_ContainerSwapSlots", value++);
            ctx.SetTable("Server_ContainerAddToSlot", value++);
            ctx.SetTable("Server_ContainerRemoveFromSlot", value++);
            ctx.SetTable("Server_ContainerSwapSlots", value++);

            ctx.SetTable("Client_LocalRequestSpellCast", value++);
            ctx.SetTable("Server_SendSpellCastResult", value++);

            ctx.SetTable("Server_SendCombatEvent", value++);

            ctx.SetTable("Count", value);
        });
    }
}
