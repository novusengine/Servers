#pragma once
#include "GameCommandBase.h"

#include <entt/fwd.hpp>

#include <string>

namespace ECS
{
    namespace Singletons
    {
        struct GameCommandQueue;
    };

    // Struct Defines
    namespace GameCommands::Item
    {
    }

    // Handler Defines
    namespace GameCommands::Item
    {
        void InstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue);
        void UninstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue);
    }
}