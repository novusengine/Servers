#include <Base/Types.h>

#include <Network/Define.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Util::Network
    {
        bool HasPacketHandler(Zenith* zenith, ::Network::OpcodeType opcode);
        bool CallPacketHandler(Zenith* zenith, ::Network::MessageHeader& header, ::Network::Message& message);
    }
}