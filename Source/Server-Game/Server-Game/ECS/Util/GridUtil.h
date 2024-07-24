#pragma once
#include "Server-Game/ECS/Singletons/GridSingleton.h"

#include <Base/Types.h>
#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/Bytebuffer.h>

#include <entt/fwd.hpp>

namespace ECS::Util::Grid
{
    void SendToGrid(entt::entity entity, std::shared_ptr<Bytebuffer>& buffer, Singletons::GridUpdateFlag flag = {});
}