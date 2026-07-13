#include "Scheduler.h"

#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Systems/AccountLoginHandler.h"
#include "Server-Game/ECS/Systems/CharacterLoginHandler.h"
#include "Server-Game/ECS/Systems/DatabaseSetup.h"
#include "Server-Game/ECS/Systems/NetworkConnection.h"
#include "Server-Game/ECS/Systems/UpdateScripts.h"
#include "Server-Game/ECS/Systems/UpdateWorld.h"

#include <entt/entt.hpp>

namespace ECS
{
    Scheduler::Scheduler()
    {

    }

    void Scheduler::Init(entt::registry& registry)
    {
        registry.ctx().emplace<Singletons::TimeState>();

        Systems::DatabaseSetup::Init(registry);
        Systems::NetworkConnection::Init(registry);
        Systems::AccountLoginHandler::Init(registry);
        Systems::CharacterLoginHandler::Init(registry);
        Systems::UpdateWorld::Init(registry);
        Systems::UpdateScripts::Init(registry);
    }

    void Scheduler::Update(entt::registry& registry, f32 deltaTime)
    {
        // TODO: You know, actually scheduling stuff and multithreading (enkiTS tasks?)
        Systems::DatabaseSetup::Update(registry, deltaTime);
        Systems::NetworkConnection::Update(registry, deltaTime);
        Systems::AccountLoginHandler::Update(registry, deltaTime);
        Systems::CharacterLoginHandler::Update(registry, deltaTime);
        Systems::UpdateWorld::Update(registry, deltaTime);
        Systems::UpdateScripts::Update(registry, deltaTime);
    }
}