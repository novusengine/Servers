#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <MetaGen/Shared/Unit/Unit.h>

#include <Network/Define.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    struct UnitPower;
    struct UnitResistance;
    struct UnitStat;

    namespace Components
    {
        struct DisplayInfo;
        struct ObjectInfo;
        struct Transform;
        struct UnitAuraInfo;
        struct UnitCombatInfo;
        struct UnitFields;
        struct UnitPowersComponent;
        struct UnitResistancesComponent;
        struct UnitSpellCooldownHistory;
        struct UnitStatsComponent;
        struct VisibilityInfo;
    }

    namespace Singletons
    {
        struct GameCache;
        struct NetworkState;
        struct WorldState;
    }
}

namespace ECS::Util::Unit
{
    u32 GetDisplayIDFromRaceGender(GameDefine::UnitRace race, GameDefine::UnitGender unitGender);
    void UpdateDisplayID(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, u32 displayID, bool forceDirty = true);
    void UpdateDisplayRaceGender(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitRace race, GameDefine::UnitGender gender, bool forceDirty = true);
    void UpdateDisplayRace(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitRace race, bool forceDirty = true);
    void UpdateDisplayGender(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitGender gender, bool forceDirty = true);

    ECS::Components::UnitPowersComponent& AddPowersComponent(World& world, entt::entity entity, GameDefine::UnitClass unitClass);
    ECS::Components::UnitResistancesComponent& AddResistancesComponent(World& world, entt::entity entity);
    ECS::Components::UnitStatsComponent& AddStatsComponent(World& world, entt::entity entity);

    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime);
    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime);

    void TeleportToXYZ(World& world, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, const vec3& position, f32 orientation);
    bool TeleportToLocation(Singletons::WorldState& worldState, World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, u32 mapID, const vec3& position, f32 orientation);

    void SendChatMessage(World& world, Singletons::NetworkState& networkState, ::Network::SocketID socketID, const std::string& message);

    bool HasSpellCooldown(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID);
    f32 GetSpellCooldownRemaining(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID);
    void SetSpellCooldown(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID, f32 cooldown);

    bool HasPower(const Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType);
    UnitPower& GetPower(Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType);
    UnitPower* TryGetPower(Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType);
    bool AddPower(World& world, entt::entity entity, Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType, f64 base, f64 current, f64 max);
    bool SetPower(World& world, entt::entity entity, Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType, f64 base, f64 current, f64 max);

    bool HasResistance(const Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType);
    UnitResistance& GetResistance(Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType);
    bool AddResistance(Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType, f64 base, f64 current, f64 max);

    bool HasStat(const Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType);
    UnitStat& GetStat(Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType);
    bool AddStat(Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType, f64 base, f64 current);

    /// <returns>True if aura was applied or stacks increased, False if aura could not be applied</returns>
    bool AddAura(World& world, Singletons::GameCache& gameCache, entt::entity caster, entt::entity target, Components::UnitAuraInfo& unitAuraInfo, u32 spellID, u16 stackCount, entt::entity& outAuraEntity);
    /// <returns>True if aura was removed or stacks decreased, False if aura was not found</returns>
    bool RemoveAura(World& world, Components::UnitAuraInfo& unitAuraInfo, u32 spellID, u16 stacksToRemove);
    bool HasAura(Components::UnitAuraInfo& unitAuraInfo, u32 spellID);
    entt::entity GetAura(Components::UnitAuraInfo& unitAuraInfo, u32 spellID);
}