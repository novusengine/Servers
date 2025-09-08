#pragma once
#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Network/Define.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

namespace Scripting
{
    namespace Network
    {
        struct Packet
        {
        public:
            static void Register(Zenith* zenith);
            static Packet* Create(Zenith* zenith, ::Network::MessageHeader header, std::shared_ptr<Bytebuffer> buffer, u32 baseReadPos);

        public:
            u32 baseReadPos = 0;
            std::shared_ptr<Bytebuffer> buffer;
        };

        namespace PacketMethods
        {
            i32 Prepare(Zenith* zenith, Packet* packet);

            i32 GetI8(Zenith* zenith, Packet* packet);
            i32 GetI16(Zenith* zenith, Packet* packet);
            i32 GetI32(Zenith* zenith, Packet* packet);
            i32 GetI64(Zenith* zenith, Packet* packet);

            i32 GetU8(Zenith* zenith, Packet* packet);
            i32 GetU16(Zenith* zenith, Packet* packet);
            i32 GetU32(Zenith* zenith, Packet* packet);
            i32 GetU64(Zenith* zenith, Packet* packet);

            i32 GetF32(Zenith* zenith, Packet* packet);
            i32 GetF64(Zenith* zenith, Packet* packet);

            i32 GetString(Zenith* zenith, Packet* packet);
            i32 GetBytes(Zenith* zenith, Packet* packet);
        };

        static LuaRegister<Network::Packet> packetMethods[] =
        {
            { "Prepare",    Network::PacketMethods::Prepare },
            { "GetI8",      Network::PacketMethods::GetI8 },
            { "GetI16",     Network::PacketMethods::GetI16 },
            { "GetI32",     Network::PacketMethods::GetI32 },
            { "GetI64",     Network::PacketMethods::GetI64 },
            { "GetU8",      Network::PacketMethods::GetU8 },
            { "GetU16",     Network::PacketMethods::GetU16 },
            { "GetU32",     Network::PacketMethods::GetU32 },
            { "GetU64",     Network::PacketMethods::GetU64 },

            { "GetF32",     Network::PacketMethods::GetF32 },
            { "GetF64",     Network::PacketMethods::GetF64 },

            { "GetString",  Network::PacketMethods::GetString },
            { "GetBytes",   Network::PacketMethods::GetBytes },
        };
    }

    class PacketHandler : public LuaHandlerBase
    {
    private:
        void Register(Zenith* zenith);
        void Clear(Zenith* zenith) {}

        void PostLoad(Zenith* zenith) {}
        void Update(Zenith* zenith, f32 deltaTime) {}
    };
}