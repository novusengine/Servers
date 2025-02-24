#include "UpdatePower.h"

#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
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

        static f32 timeSinceLastUpdate = 0.0f;
        static constexpr f32 UPDATE_INTERVAL = 1.0f / 10.0f;

        auto view = registry.view<Components::UnitStatsComponent>();
        view.each([&](entt::entity entity, Components::UnitStatsComponent& unitStatsComponent)
        {
            static constexpr f32 healthGainRate = 5.0f;
            static constexpr f32 powerGainRate = 25.0f;

            // Handle Health Regen
            {
                f32 currentHealth = unitStatsComponent.currentHealth;

                if (unitStatsComponent.currentHealth > 0.0f && currentHealth < unitStatsComponent.maxHealth)
                {
                    unitStatsComponent.currentHealth = glm::clamp(currentHealth + (healthGainRate * deltaTime), 0.0f, unitStatsComponent.maxHealth);
                }

                bool isHealthDirty = (currentHealth != unitStatsComponent.currentHealth || unitStatsComponent.healthIsDirty);
                unitStatsComponent.healthIsDirty |= isHealthDirty;
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

                bool isStatDirty = powerValue != prevPowerValue;
                unitStatsComponent.powerIsDirty[i] |= isStatDirty;
            }
        });

        timeSinceLastUpdate += deltaTime;
        if (timeSinceLastUpdate >= UPDATE_INTERVAL)
        {
            auto view = registry.view<const Components::NetInfo, const Components::ObjectInfo, Components::UnitStatsComponent>();
            view.each([&](entt::entity entity, const Components::NetInfo& netInfo, const Components::ObjectInfo& objectInfo, Components::UnitStatsComponent& unitStatsComponent)
            {
                if (unitStatsComponent.healthIsDirty)
                {
                    std::shared_ptr<Bytebuffer> unitStatsMessage = Bytebuffer::Borrow<32>();
                    if (!Util::MessageBuilder::Entity::BuildUnitStatsMessage(unitStatsMessage, objectInfo.guid, Components::PowerType::Health, unitStatsComponent.baseHealth, unitStatsComponent.currentHealth, unitStatsComponent.maxHealth))
                        return;

                    ECS::Util::Grid::SendToNearby(entity, unitStatsMessage, true);
                    unitStatsComponent.healthIsDirty = false;
                }

                for (i32 i = 0; i < (u32)Components::PowerType::Count; i++)
                {
                    bool isDirty = unitStatsComponent.powerIsDirty[i];
                    if (!isDirty)
                        continue;

                    Components::PowerType powerType = (Components::PowerType)i;

                    std::shared_ptr<Bytebuffer> unitStatsMessage = Bytebuffer::Borrow<32>();
                    if (!Util::MessageBuilder::Entity::BuildUnitStatsMessage(unitStatsMessage, objectInfo.guid, powerType, unitStatsComponent.basePower[i], unitStatsComponent.currentPower[i], unitStatsComponent.maxPower[i]))
                        return;

                    ECS::Util::Grid::SendToNearby(entity, unitStatsMessage, true);
                    unitStatsComponent.powerIsDirty[i] = false;
                }
            });

            timeSinceLastUpdate -= UPDATE_INTERVAL;
        }
    }
}