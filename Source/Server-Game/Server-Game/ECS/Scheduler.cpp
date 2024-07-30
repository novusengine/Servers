#include "Scheduler.h"

#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Systems/DatabaseSetup.h"
#include "Server-Game/ECS/Systems/NetworkConnection.h"
#include "Server-Game/ECS/Systems/UpdatePower.h"
#include "Server-Game/ECS/Systems/UpdateScripts.h"
#include "Server-Game/ECS/Systems/UpdateSpell.h"
#include "Server-Game/ECS/Systems/SimpleReplication.h"

#include <entt/entt.hpp>

namespace ECS
{
    Scheduler::Scheduler()
    {

    }

    void Scheduler::Init(entt::registry& registry)
    {
        Systems::DatabaseSetup::Init(registry);
        Systems::SimpleReplication::Init(registry);
        Systems::NetworkConnection::Init(registry);
        Systems::UpdatePower::Init(registry);
        Systems::UpdateSpell::Init(registry);
        Systems::UpdateScripts::Init(registry);
    }

    void Scheduler::Update(entt::registry& registry, f32 deltaTime)
    {
        // TODO: You know, actually scheduling stuff and multithreading (enkiTS tasks?)
        Systems::DatabaseSetup::Update(registry, deltaTime);
        Systems::NetworkConnection::Update(registry, deltaTime);
        Systems::UpdatePower::Update(registry, deltaTime);
        Systems::UpdateSpell::Update(registry, deltaTime);
        Systems::UpdateScripts::Update(registry, deltaTime);
        Systems::SimpleReplication::Update(registry, deltaTime);
    }
}