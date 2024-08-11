#include "GridUtil.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <entt/entt.hpp>

namespace ECS::Util::Grid
{
    void SendToGrid(entt::entity entity, std::shared_ptr<Bytebuffer>& buffer, Singletons::GridUpdateFlag flag)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        Singletons::GridSingleton& gridSingleton = registry->ctx().get<Singletons::GridSingleton>();

        {
            gridSingleton.cell.updates.enqueue({ entity, flag, std::move(buffer) });
        }
    }
}