#include "UpdateSpell.h"

#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
	void UpdateSpell::Init(entt::registry& registry)
	{
	}

	void UpdateSpell::Update(entt::registry& registry, f32 deltaTime)
	{
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();

		struct CastEvent
        {
            entt::entity caster;
            entt::entity target;
        };

		std::vector<CastEvent> castEvents;

		auto view = registry.view<Components::CastInfo>();
        view.each([&](entt::entity entity, Components::CastInfo& castInfo)
        {
			castInfo.duration += deltaTime;

			if (castInfo.duration >= castInfo.castTime)
			{
				castEvents.push_back({ entity, castInfo.target });
			}
        });

		for (const CastEvent& castEvent : castEvents)
		{
			entt::entity caster = castEvent.caster;
			entt::entity target = castEvent.target;

			Components::UnitStatsComponent& targetStats = registry.get<Components::UnitStatsComponent>(target);
			targetStats.currentHealth = glm::max(0.0f, targetStats.currentHealth - 35.0f);
			targetStats.healthIsDirty = true;

			// Send Grid Message
			{
				std::shared_ptr<Bytebuffer> damageDealtMessage = nullptr;
				if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, caster, target, 35.0f))
					continue;

				ECS::Util::Grid::SendToGrid(caster, damageDealtMessage, { .SendToSelf = true });
			}

			registry.remove<Components::CastInfo>(caster);
		}
	}
}