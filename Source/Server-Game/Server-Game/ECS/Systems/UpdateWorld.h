#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Singletons
    {
        struct GameCache;
        struct NetworkState;
        struct TimeState;
        struct WorldState;
    }

    namespace Systems
    {
        class UpdateWorld
        {
        public:
            static void Init(entt::registry& registry);
            static void Update(entt::registry& registry, f32 deltaTime);

        private:
            static bool HandleMapInitialization(World& world, Singletons::TimeState& timeState, Singletons::GameCache& gameCache);
            static void HandleCreatureDeinitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache);
            static void HandleCreatureInitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache);
            static void HandleCreatureUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);

            static void HandleUnitUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);

            static void HandleProximityTriggerDeinitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleProximityTriggerInitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleProximityTriggerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);

            static void HandleReplication(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
            static void HandleContainerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleDisplayUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
            static void HandleCombatUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
            static void HandlePowerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
            static void HandleAuraUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleSpellUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleCombatEventUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState);
        };
    }
}