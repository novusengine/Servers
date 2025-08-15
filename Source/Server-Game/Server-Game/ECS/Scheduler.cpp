#include "Scheduler.h"

#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Systems/CharacterInitialization.h"
#include "Server-Game/ECS/Systems/CharacterLoginHandler.h"
#include "Server-Game/ECS/Systems/CharacterUpdate.h"
#include "Server-Game/ECS/Systems/DatabaseSetup.h"
#include "Server-Game/ECS/Systems/NetworkConnection.h"
#include "Server-Game/ECS/Systems/Replication.h"
#include "Server-Game/ECS/Systems/UpdatePower.h"
#include "Server-Game/ECS/Systems/UpdateScripts.h"
#include "Server-Game/ECS/Systems/UpdateSpell.h"

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
        Systems::Replication::Init(registry);
        Systems::NetworkConnection::Init(registry);
        Systems::CharacterLoginHandler::Init(registry);
        Systems::CharacterInitialization::Init(registry);
        Systems::CharacterUpdate::Init(registry);
        Systems::UpdatePower::Init(registry);
        Systems::UpdateSpell::Init(registry);
        Systems::UpdateScripts::Init(registry);
    }

    void Scheduler::Update(entt::registry& registry, f32 deltaTime)
    {
        // TODO: You know, actually scheduling stuff and multithreading (enkiTS tasks?)
        Systems::DatabaseSetup::Update(registry, deltaTime);
        Systems::NetworkConnection::Update(registry, deltaTime);
        Systems::CharacterLoginHandler::Update(registry, deltaTime);
        Systems::CharacterInitialization::Update(registry, deltaTime);
        Systems::CharacterUpdate::Update(registry, deltaTime);
        Systems::UpdatePower::Update(registry, deltaTime);
        Systems::UpdateSpell::Update(registry, deltaTime);
        Systems::Replication::Update(registry, deltaTime);
        Systems::UpdateScripts::Update(registry, deltaTime);
    }
}