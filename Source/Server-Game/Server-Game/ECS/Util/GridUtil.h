#pragma once
#include <Base/Types.h>
#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/Bytebuffer.h>

#include <entt/fwd.hpp>

namespace ECS::Util::Grid
{
    void SendToNearby(entt::entity entity, std::shared_ptr<Bytebuffer>& buffer, bool sendToSelf = false);
}