#pragma once
#include <Base/Types.h>

struct lua_State;
typedef i32 (*lua_CFunction)(lua_State* L);

namespace Scripting
{
    class LuaManager;
    class LuaHandlerBase;
    class LuaSystemBase;
    using LuaUserDataDtor = void(void*);

    enum class LuaHandlerType
    {
        Global,
        PacketEvent,
        Count
    };

    enum class LuaSystemEvent
    {
        Invalid,
        Reload
    };

    enum class LuaPacketEvent
    {
        Invalid,
        OnReceive,
        Count
    };

    struct LuaEventData
    {
    public:
    };

    struct LuaPacketEventOnHandleData : LuaEventData
    {
    public:
        u32 opcode;
        u32 size;
        void* data;
    };
}