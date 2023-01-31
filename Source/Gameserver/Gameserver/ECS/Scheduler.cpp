#include "Scheduler.h"

#include "Gameserver/ECS/Components/Transform.h"

#include "Gameserver/ECS/Systems/DatabaseSetup.h"
#include "Gameserver/ECS/Systems/NetworkConnection.h"
#include "Gameserver/ECS/Systems/UpdateScripts.h"

#include <entt/entt.hpp>

namespace ECS
{
	Scheduler::Scheduler()
	{

	}

	void Scheduler::Init(entt::registry& registry)
	{
		Systems::DatabaseSetup::Init(registry);
		Systems::NetworkConnection::Init(registry);
		Systems::UpdateScripts::Init(registry);
	}

	void Scheduler::Update(entt::registry& registry, f32 deltaTime)
	{
		// TODO: You know, actually scheduling stuff and multithreading (enkiTS tasks?)
		Systems::DatabaseSetup::Update(registry, deltaTime);
		Systems::NetworkConnection::Update(registry, deltaTime);
		Systems::UpdateScripts::Update(registry, deltaTime);
	}
}