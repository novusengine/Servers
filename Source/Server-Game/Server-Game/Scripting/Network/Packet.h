#pragma once
#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Gameplay/Network/Opcode.h>
#include <Network/Define.h>

#include <lualib.h>

#include <memory>

namespace Scripting::Network
{
    struct Packet
    {
    public:
        static void Register(lua_State* state);
        static Packet* Create(lua_State* state, ::Network::MessageHeader& header, std::shared_ptr<Bytebuffer> buffer);

    public:
        ::Network::MessageHeader messageHeader;
        std::shared_ptr<Bytebuffer> buffer;
    };

    namespace PacketMethods
    {
        i32 GetOpcode(lua_State* state);
        i32 GetSize(lua_State* state);

        i32 GetI8(lua_State* state);
        i32 GetI16(lua_State* state);
        i32 GetI32(lua_State* state);
        i32 GetI64(lua_State* state);

        i32 GetU8(lua_State* state);
        i32 GetU16(lua_State* state);
        i32 GetU32(lua_State* state);
        i32 GetU64(lua_State* state);

        i32 GetF32(lua_State* state);
        i32 GetF64(lua_State* state);

        i32 GetString(lua_State* state);
        i32 GetBytes(lua_State* state);
    };
}