#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <Network/Define.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct ObjectInfo;
        struct DisplayInfo;
        struct Transform;
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
    void UpdateDisplayID(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, u32 displayID, bool forceDirty = true);
    void UpdateDisplayRaceGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, GameDefine::UnitGender gender, bool forceDirty = true);
    void UpdateDisplayRace(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, bool forceDirty = true);
    void UpdateDisplayGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitGender gender, bool forceDirty = true);

    ECS::Components::UnitStatsComponent& AddStatsComponent(entt::registry& registry, entt::entity entity);

    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime);
    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime);

    void TeleportToXYZ(World& world, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, const vec3& position, f32 orientation);
    bool TeleportToLocation(Singletons::WorldState& worldState, World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, u32 mapID, const vec3& position, f32 orientation);

    void SendChatMessage(World& world, Singletons::NetworkState& networkState, ::Network::SocketID socketID, const std::string& message);
}