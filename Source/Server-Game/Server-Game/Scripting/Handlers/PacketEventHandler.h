#pragma once
#include "LuaEventHandlerBase.h"
#include "Server-Game/Scripting/LuaDefines.h"
#include "Server-Game/Scripting/LuaMethodTable.h"

#include <Base/Types.h>

#include <Gameplay/Network/Opcode.h>

#include <robinhood/robinhood.h>

namespace Scripting
{
    class PacketEventHandler : public LuaEventHandlerBase
    {
    public:
        PacketEventHandler() : LuaEventHandlerBase(static_cast<u32>(LuaPacketEvent::Count)) { }

        void Clear() override
        {
            LuaEventHandlerBase::Clear();
            _luaStateToOpcodeFuncRef.clear();
        }

    public:
        void SetEventHandler(u32 eventID, EventHandlerFn fn);
        void CallEvent(lua_State* state, u32 eventID, LuaEventData* data);
        void RegisterEventCallback(lua_State* state, u32 eventID, i32 funcHandle);
        void RegisterHandlerCallback(lua_State* state, ::Network::GameOpcode opcode, i32 funcHandle);

        bool HasGameOpcodeHandler(lua_State* state, ::Network::GameOpcode opcode) const;
        bool CallGameOpcodeHandler(lua_State* state, ::Network::MessageHeader& header, ::Network::Message& message);

        bool HasListeners(LuaPacketEvent eventID) const { return _eventToLuaStateFuncRefList[(u32)eventID].size() > 0; }

    public: // Registered Functions
        static i32 RegisterPacketHandler(lua_State* state);
        static i32 RegisterPacketEvent(lua_State* state);

    private:
        void Register(lua_State* state);

    private: // Event Handlers (Called by CallEvent)
        i32 OnPacketReceived(lua_State* state, u32 eventID, LuaEventData* data);

    private: // Utility Functions
        void CreatePacketEventTable(lua_State* state);
        void CreateGameOpcodeTable(lua_State* state);

    private:
        robin_hood::unordered_map<u64, robin_hood::unordered_map<::Network::GameOpcode, i32>> _luaStateToOpcodeFuncRef;
    };

    static LuaMethod packetEventMethods[] =
    {
        { "RegisterPacketHandler", PacketEventHandler::RegisterPacketHandler },
        { "RegisterPacketEvent", PacketEventHandler::RegisterPacketEvent }
    };
}