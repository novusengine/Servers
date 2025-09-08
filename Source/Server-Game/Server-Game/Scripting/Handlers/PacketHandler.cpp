#include "PacketHandler.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    void PacketHandler::Register(Zenith* zenith)
    {
        Network::Packet::Register(zenith);
    }

    namespace Network
    {
        void Packet::Register(Zenith* zenith)
        {
            LuaMetaTable<Packet>::Register(zenith, "PacketMetaTable");
            LuaMetaTable<Packet>::Set(zenith, packetMethods);
        }

        Packet* Packet::Create(Zenith* zenith, ::Network::MessageHeader header, std::shared_ptr<Bytebuffer> buffer, u32 baseReadPos)
        {
            Scripting::Network::Packet* packet = zenith->PushUserData<Scripting::Network::Packet>([](void* x) {});

            packet->baseReadPos = baseReadPos;
            packet->buffer = buffer;

            luaL_getmetatable(zenith->state, "PacketMetaTable");
            lua_setmetatable(zenith->state, -2);

            return packet;
        }

        namespace PacketMethods
        {
            i32 Prepare(Zenith* zenith, Packet* packet)
            {
                packet->buffer->readData = packet->baseReadPos;
                return 0;
            }

            i32 GetI8(Zenith* zenith, Packet* packet)
            {
                i8 value = 0;
                if (!packet->buffer->GetI8(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetI16(Zenith* zenith, Packet* packet)
            {
                i16 value = 0;
                if (!packet->buffer->GetI16(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetI32(Zenith* zenith, Packet* packet)
            {
                i32 value = 0;
                if (!packet->buffer->GetI32(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetI64(Zenith* zenith, Packet* packet)
            {
                i64 value = 0;
                if (!packet->buffer->GetI64(value))
                    return 0;

                zenith->Push((f64)value);
                return 1;
            }

            i32 GetU8(Zenith* zenith, Packet* packet)
            {
                u8 value = 0;
                if (!packet->buffer->GetU8(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetU16(Zenith* zenith, Packet* packet)
            {
                u16 value = 0;
                if (!packet->buffer->GetU16(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetU32(Zenith* zenith, Packet* packet)
            {
                u32 value = 0;
                if (!packet->buffer->GetU32(value))
                    return 0;

                zenith->Push((u32)value);
                return 1;
            }
            i32 GetU64(Zenith* zenith, Packet* packet)
            {
                u64 value = 0;
                if (!packet->buffer->GetU64(value))
                    return 0;

                zenith->Push((f64)value);
                return 1;
            }

            i32 GetF32(Zenith* zenith, Packet* packet)
            {
                f32 value = 0;
                if (!packet->buffer->GetF32(value))
                    return 0;

                zenith->Push((f32)value);
                return 1;
            }
            i32 GetF64(Zenith* zenith, Packet* packet)
            {
                f64 value = 0;
                if (!packet->buffer->GetF64(value))
                    return 0;

                zenith->Push((f64)value);
                return 1;
            }

            i32 GetString(Zenith* zenith, Packet* packet)
            {
                char* value = nullptr;
                if (!packet->buffer->GetString(value))
                    return 0;

                zenith->Push(static_cast<const char*>(value));
                return 1;
            }

            i32 GetBytes(Zenith* zenith, Packet* packet)
            {
                u32 numBytes = zenith->CheckVal<u32>(2);
                if (numBytes == 0 || !packet->buffer->CanPerformRead(numBytes))
                    return 0;

                zenith->CreateTable();

                for (u32 i = 0; i < numBytes; i++)
                {
                    u32 key = i + 1;
                    u8 value = packet->buffer->GetReadPointer()[i];

                    zenith->AddTableField(key, (u32)value);
                }

                packet->buffer->SkipRead(numBytes);

                return 1;
            }
        }
    }
}
