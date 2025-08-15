#include "Packet.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Scripting/LuaMethodTable.h"
#include "Server-Game/Util/ServiceLocator.h"

namespace Scripting::Network
{
    static LuaMethod packetMethods[] =
    {
        { "GetOpcode", PacketMethods::GetOpcode },
        { "GetSize", PacketMethods::GetSize },

        { "GetI8", PacketMethods::GetI8 },
        { "GetI16", PacketMethods::GetI16 },
        { "GetI32", PacketMethods::GetI32 },
        { "GetI64", PacketMethods::GetI64 },
        { "GetU8", PacketMethods::GetU8 },
        { "GetU16", PacketMethods::GetU16 },
        { "GetU32", PacketMethods::GetU32 },
        { "GetU64", PacketMethods::GetU64 },

        { "GetF32", PacketMethods::GetF32 },
        { "GetF64", PacketMethods::GetF64 },

        { "GetString", PacketMethods::GetString },
        { "GetBytes", PacketMethods::GetBytes },

    };

    void Packet::Register(lua_State* state)
    {
        LuaMetaTable<Packet>::Register(state, "PacketMetaTable");

        LuaMetaTable<Packet>::Set(state, packetMethods);
    }

    Packet* Packet::Create(lua_State* state, ::Network::MessageHeader& header, std::shared_ptr<Bytebuffer> buffer)
    {
        LuaState ctx(state);
        Scripting::Network::Packet* packet = ctx.PushUserData<Scripting::Network::Packet>([](void* x)
        {

        });

        packet->messageHeader = header;
        packet->buffer = buffer;
        luaL_getmetatable(state, "PacketMetaTable");
        lua_setmetatable(state, -2);

        return packet;
    }

    namespace PacketMethods
    {
        i32 GetOpcode(lua_State* state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            ctx.Push((u32)packet->messageHeader.opcode);
            return 1;
        }

        i32 GetSize(lua_State* state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            ctx.Push((u32)packet->messageHeader.size);
            return 1;
        }

        i32 GetI8(lua_State* state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            i8 value = 0;
            if (packet->buffer->GetI8(value))
            {
                ctx.Push((i32)value);
                return 1;
            }

            return 0;
        }
        i32 GetI16(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            i16 value = 0;
            if (packet->buffer->GetI16(value))
            {
                ctx.Push((i32)value);
                return 1;
            }

            return 0;
        }
        i32 GetI32(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            i32 value = 0;
            if (packet->buffer->GetI32(value))
            {
                ctx.Push((i32)value);
                return 1;
            }

            return 0;
        }
        i32 GetI64(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            i64 value = 0;
            if (packet->buffer->GetI64(value))
            {
                ctx.Push((f64)value);
                return 1;
            }

            return 0;
        }

        i32 GetU8(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            u8 value = 0;
            if (packet->buffer->GetU8(value))
            {
                ctx.Push((u32)value);
                return 1;
            }

            return 0;
        }
        i32 GetU16(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            u16 value = 0;
            if (packet->buffer->GetU16(value))
            {
                ctx.Push((u32)value);
                return 1;
            }

            return 0;
        }
        i32 GetU32(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            u32 value = 0;
            if (packet->buffer->GetU32(value))
            {
                ctx.Push((u32)value);
                return 1;
            }

            return 0;
        }
        i32 GetU64(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            u64 value = 0;
            if (packet->buffer->GetU64(value))
            {
                ctx.Push((f64)value);
                return 1;
            }

            return 0;
        }

        i32 GetF32(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            f32 value = 0;
            if (packet->buffer->GetF32(value))
            {
                ctx.Push((f32)value);
                return 1;
            }

            return 0;
        }
        i32 GetF64(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            f64 value = 0;
            if (packet->buffer->GetF64(value))
            {
                ctx.Push((f64)value);
                return 1;
            }

            return 0;
        }

        i32 GetString(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            char* value = nullptr;
            if (packet->buffer->GetString(value))
            {
                ctx.Push(static_cast<const char*>(value));
                return 1;
            }

            return 0;
        }

        i32 GetBytes(lua_State * state)
        {
            LuaState ctx(state);

            Packet* packet = ctx.GetUserData<Packet>(nullptr, 1);
            if (packet == nullptr)
            {
                luaL_error(state, "Packet is null");
            }

            u32 numBytes = ctx.Get(0u, 2);
            if (numBytes > 0 && packet->buffer->CanPerformRead(numBytes))
            {
                ctx.CreateTableAndPopulate([&ctx, packet, numBytes]()
                {
                    for (u32 i = 0; i < numBytes; i++)
                    {
                        u8 value = packet->buffer->GetReadPointer()[i];

                        ctx.Push((u32)value);
                        ctx.SetTable(i + 1);
                    }

                    packet->buffer->SkipRead(numBytes);
                });

                return 1;
            }

            return 0;
        }
    }
}