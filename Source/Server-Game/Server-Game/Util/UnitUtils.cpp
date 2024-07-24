#include "UnitUtils.h"

#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <entt/entt.hpp>

using namespace ECS;

namespace UnitUtils
{
    Components::UnitStatsComponent& AddStatsComponent(entt::registry& registry, entt::entity entity)
    {
        auto& unitStatsComponent = registry.emplace_or_replace<Components::UnitStatsComponent>(entity);

        {
            unitStatsComponent.baseHealth = 100.0f;
            unitStatsComponent.currentHealth = 50.0f;
            unitStatsComponent.maxHealth = 100.0f;
        }

        for (u32 i = 0; i < (u32)Components::PowerType::Count; ++i)
        {
            unitStatsComponent.basePower[i] = 100.0f;

            if (i == (u32)Components::PowerType::Mana || i == (u32)Components::PowerType::Rage || i == (u32)Components::PowerType::Happiness)
                unitStatsComponent.currentPower[i] = 100.0f;
            else
                unitStatsComponent.currentPower[i] = 0.0f;

            unitStatsComponent.maxPower[i] = 100.0f;
        }

        for (u32 i = 0; i < (u32)Components::StatType::Count; ++i)
        {
            unitStatsComponent.baseStat[i] = 10;
            unitStatsComponent.currentStat[i] = 10;
        }

        for (u32 i = 0; i < (u32)Components::ResistanceType::Count; ++i)
        {
            unitStatsComponent.baseResistance[i] = 5;
            unitStatsComponent.currentResistance[i] = 5;
        }

        return unitStatsComponent;
    }

    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime)
    {
        static constexpr f32 RageGainPerSecond = -10.0f;

        f32 result = glm::clamp(current + (RageGainPerSecond * rateModifier) * deltaTime, 0.0f, 100.0f);
        return result;
    }

    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime)
    {
        static constexpr f32 EnergyGainPerSecond = 10.0f;

        f32 result = glm::clamp(current + ((EnergyGainPerSecond * rateModifier) * deltaTime), 0.0f, max);
        return result;
    }
}