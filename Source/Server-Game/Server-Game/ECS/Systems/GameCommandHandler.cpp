#include "GameCommandHandler.h"

#include "Server-Game/ECS/GameCommands/CharacterCommands.h"
#include "Server-Game/ECS/GameCommands/ItemCommands.h"
#include "Server-Game/ECS/Singletons/GameCommandQueue.h"

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void GameCommandHandler::Init(entt::registry& registry)
    {
        auto& gameCommandQueue = registry.ctx().emplace<Singletons::GameCommandQueue>();

        GameCommands::Character::InstallHandlers(registry, gameCommandQueue);
        GameCommands::Item::InstallHandlers(registry, gameCommandQueue);
    }

    void GameCommandHandler::Update(entt::registry& registry, f32 deltaTime)
    {
        Singletons::GameCommandQueue& gameCommandQueue = registry.ctx().get<Singletons::GameCommandQueue>();

        GameCommands::GameCommandBase* gameCommandBase = nullptr;
        while (gameCommandQueue.Pop(gameCommandBase))
        {
            if (!gameCommandBase)
                continue;

            gameCommandQueue.CallHandler(registry, gameCommandBase);
            delete gameCommandBase;
        }
    }
}