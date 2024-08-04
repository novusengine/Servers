#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct NetInfo
    {
    public:
        static constexpr u64 PING_INTERVAL = 5000;
        static constexpr u8 MAX_EARLY_PINGS = 5;
        static constexpr u8 MAX_LATE_PINGS = 1;

        u16 ping = 0;
        u8 numEarlyPings = 0;
        u8 numLatePings = 0;
        u64 lastPingTime = 0;
    };
}