#include "ItemCommands.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GameCommandQueue.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/StringUtils.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS
{
    namespace GameCommands::Item
    {
        void InstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue)
        {
            auto& gameCommandQueue = registry.ctx().get<Singletons::GameCommandQueue>();

        }

        void UninstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue)
        {
            auto& gameCommandQueue = registry.ctx().get<Singletons::GameCommandQueue>();

        }
    }
}
