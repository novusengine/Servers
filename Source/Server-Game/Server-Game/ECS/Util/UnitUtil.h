#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct DisplayInfo;
    struct UnitStatsComponent;
}

namespace ECS::Util::Unit
{
    u32 GetDisplayIDFromRaceGender(GameDefine::UnitRace race, GameDefine::UnitGender unitGender);
    void UpdateDisplayID(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, u32 displayID, bool forceDirty = true);
    void UpdateDisplayRaceGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, GameDefine::UnitGender gender, bool forceDirty = true);
    void UpdateDisplayRace(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, bool forceDirty = true);
    void UpdateDisplayGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitGender gender, bool forceDirty = true);

    ECS::Components::UnitStatsComponent& AddStatsComponent(entt::registry& registry, entt::entity entity);

    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime);
    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime);
}