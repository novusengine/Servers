#pragma once

#include <Base/Types.h>

#include <Network/Define.h>

namespace ECS::Components
{
    struct AdministrativeRequestContext
    {
        ::Network::SocketID requesterSocketID = ::Network::SOCKET_ID_INVALID;
    };

    struct DatabaseRetryState
    {
        u8 retryCount = 0;
        u64 retryAfterEpoch = 0;
    };
}
