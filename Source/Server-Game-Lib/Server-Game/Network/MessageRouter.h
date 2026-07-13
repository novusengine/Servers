#pragma once
#include <Base/Types.h>
#include <Base/FunctionTraits.h>
#include <Base/Memory/Bytebuffer.h>

#include <Gameplay/Network/Define.h>

#include <MetaGen/PacketList.h>

#include <Network/Define.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

namespace ECS
{
    struct World;
}

namespace Network
{
    using MessageHandlerFn = std::function<bool(ECS::World* world, entt::entity, SocketID, Message& message)>;
    struct MessageHandler
    {
        MessageHandler() {}
        MessageHandler(ConnectionStatus inStatus, u16 inSize, MessageHandlerFn inHandler) : status(inStatus), minSize(inSize), maxSize(inSize), handler(inHandler) {}
        MessageHandler(ConnectionStatus inStatus, u16 inMinSize, i16 inMaxSize, MessageHandlerFn inHandler) : status(inStatus), minSize(inMinSize), maxSize(inMaxSize), handler(inHandler) {}

        ConnectionStatus status = ConnectionStatus::None;
        u16 minSize = 0;
        i16 maxSize = 0;
        MessageHandlerFn handler = nullptr;
    };

    class MessageRouter
    {
    public:
        MessageRouter();

        void SetMessageHandler(OpcodeType opcode, MessageHandler&& handler);

        template <typename PacketHandler>
        void RegisterPacketHandler(ConnectionStatus connectionStatus, PacketHandler callback)
        {
            using PacketStruct = std::decay_t<std::tuple_element_t<3, function_args_t<PacketHandler>>>;
            static_assert(std::is_invocable_r_v<bool, PacketHandler, ECS::World*, entt::entity, SocketID, PacketStruct&>, "MessageHandler - The Callback provided to 'RegisterPacketHandler' must return a bool and take 3 parameters (World*, SocketID, T&)");
            static_assert(PacketConcept<PacketStruct>, "MessageHandler::RegisterPacketHandler PacketStruct Parameter in the provided callback does not satisfy PacketConcept requirements");

            MessageHandler& handler = _handlers[static_cast<OpcodeType>(PacketStruct::PACKET_ID)];

            handler.status = connectionStatus;
            handler.minSize = 0;
            handler.maxSize = -1;
            handler.handler = [callback](ECS::World* world, entt::entity entity, SocketID socketID, Message& message) -> bool
            {
                PacketStruct packet;
                if (!message.buffer->Deserialize(packet))
                    return false;

                return callback(world, entity, socketID, packet);
            };
        }

        void UnregisterPacketHandler(OpcodeType opcode)
        {
            MessageHandler& handler = _handlers[opcode];
            handler.status = ConnectionStatus::None;
            handler.minSize = 0;
            handler.maxSize = 0;
            handler.handler = nullptr;
        }

        bool GetMessageHeader(Message& message, Network::MessageHeader& header) const;
        bool HasValidHandlerForHeader(const Network::MessageHeader& header) const;
        bool CallHandler(ECS::World* world, entt::entity entity, SocketID socketID, Network::MessageHeader& header, Message& message);

    private:
        MessageHandler _handlers[(u16)MetaGen::PacketListEnum::Count];
    };
}