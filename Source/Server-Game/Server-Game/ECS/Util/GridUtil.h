#pragma once
#include <Base/Types.h>
#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/Bytebuffer.h>

#include <entt/fwd.hpp>

namespace ECS
{
    namespace Components
    {
        struct NetInfo;
    }

    namespace Util::Grid
    {
        void SendToNearby(entt::registry& registry, const entt::entity entity, const Components::NetInfo& netInfo, std::shared_ptr<Bytebuffer>& buffer, bool sendToSelf = false);
    }
}