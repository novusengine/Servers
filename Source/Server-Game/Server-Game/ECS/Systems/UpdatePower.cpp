#include "UpdatePower.h"

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
	void UpdatePower::Init(entt::registry& registry)
	{
	}

	void UpdatePower::Update(entt::registry& registry, f32 deltaTime)
	{
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();

		auto view = registry.view<Components::UnitStatsComponent>();
        view.each([&](entt::entity entity, Components::UnitStatsComponent& unitStatsComponent)
        {
            static constexpr f32 healthGainRate = 5.0f;
            static constexpr f32 powerGainRate = 25.0f;

            if (!networkState.EntityToSocketID.contains(entity))
                return;

            bool isUnitStatsDirty = false;

            // Handle Health Regen
            {
                f32 currentHealth = unitStatsComponent.currentHealth;

                if (unitStatsComponent.currentHealth > 0.0f && currentHealth < unitStatsComponent.maxHealth)
                {
                    unitStatsComponent.currentHealth = glm::clamp(currentHealth + (healthGainRate * deltaTime), 0.0f, unitStatsComponent.maxHealth);
                }

                isUnitStatsDirty |= currentHealth != unitStatsComponent.currentHealth || unitStatsComponent.healthIsDirty;
                unitStatsComponent.healthIsDirty = false;
            }

            for (i32 i = 0; i < (u32)Components::PowerType::Count; i++)
            {
                f32 prevPowerValue = unitStatsComponent.currentPower[i];
                f32& powerValue = unitStatsComponent.currentPower[i];
                f32 basePower = unitStatsComponent.basePower[i];
                f32 maxPower = unitStatsComponent.maxPower[i];

                Components::PowerType powerType = (Components::PowerType)i;
                if (powerType == Components::PowerType::Rage)
                {
                    if (powerValue == 0)
                        continue;
                }
                else
                {
                    if (powerValue == maxPower)
                        continue;
                }

                switch (powerType)
                {
                    case Components::PowerType::Rage:
                    {
                        powerValue = UnitUtils::HandleRageRegen(powerValue, 1.0f, deltaTime);
                        break;
                    }

                    case Components::PowerType::Focus:
                    case Components::PowerType::Energy:
                    {
                        powerValue = UnitUtils::HandleEnergyRegen(powerValue, maxPower, 1.0f, deltaTime);
                        break;
                    }

                    default:
                    {
                        continue;
                    }
                }

                isUnitStatsDirty |= powerValue != prevPowerValue;
            }

            if (isUnitStatsDirty)
            {
                std::shared_ptr<Bytebuffer> unitStatsMessage = nullptr;
                if (!Util::MessageBuilder::Entity::BuildUnitStatsMessage(unitStatsMessage, entity, unitStatsComponent))
                    return;

                ECS::Util::Grid::SendToGrid(entity, unitStatsMessage, ECS::Singletons::GridUpdateFlag{ .SendToSelf = true });
            }
        });
	}
}