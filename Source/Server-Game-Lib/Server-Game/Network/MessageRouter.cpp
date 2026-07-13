#include "MessageRouter.h"

#include <Base/Memory/Bytebuffer.h>

#include <MetaGen/PacketList.h>

namespace Network
{
    MessageRouter::MessageRouter() {}

    void MessageRouter::SetMessageHandler(OpcodeType opcode, MessageHandler&& handler)
    {
        _handlers[opcode] = std::move(handler);
    }

    bool MessageRouter::GetMessageHeader(Message& message, Network::MessageHeader& header) const
    {
        if (!message.buffer->Get(header))
            return false;

        if (header.opcode == static_cast<OpcodeType>(MetaGen::PacketListEnum::Invalid) || header.opcode >= static_cast<OpcodeType>(MetaGen::PacketListEnum::Count))
            return false;

        return true;
    }

    bool MessageRouter::HasValidHandlerForHeader(const Network::MessageHeader& header) const
    {
        const MessageHandler& opcodeHandler = _handlers[header.opcode];

        if (!opcodeHandler.handler)
            return false;

        bool isSmallerThanMinSize = header.size < opcodeHandler.minSize;
        bool isLargerThanMaxSize = header.size > opcodeHandler.maxSize && opcodeHandler.maxSize != -1;
        if (isSmallerThanMinSize || isLargerThanMaxSize)
            return false;

        return true;
    }

    bool MessageRouter::CallHandler(ECS::World* world, entt::entity entity, SocketID socketID, Network::MessageHeader& header, Message& message)
    {
        const MessageHandler& opcodeHandler = _handlers[header.opcode];
        return opcodeHandler.handler(world, entity, socketID, message);
    }
}