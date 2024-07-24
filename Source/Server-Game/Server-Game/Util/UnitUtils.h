#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct UnitStatsComponent;
}

namespace UnitUtils
{
    ECS::Components::UnitStatsComponent& AddStatsComponent(entt::registry& registry, entt::entity entity);
    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime);
    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime);
}