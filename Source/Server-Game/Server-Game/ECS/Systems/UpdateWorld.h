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
            static bool HandleMapInitialization(World& world, Singletons::GameCache& gameCache);
            static void HandleCreatureDeinitialization(World& world, Singletons::GameCache& gameCache);
            static void HandleCreatureInitialization(World& world, Singletons::GameCache& gameCache);
            static void HandleCreatureUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime);

            static void HandleUnitUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime);

            static void HandleProximityTriggerDeinitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleProximityTriggerInitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleProximityTriggerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);

            static void HandleReplication(World& world, Singletons::NetworkState& networkState, f32 deltaTime);
            static void HandleContainerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleDisplayUpdate(World& world, Singletons::NetworkState& networkState);
            static void HandleCombatUpdate(World& world, Scripting::Zenith* zenith, Singletons::NetworkState& networkState, f32 deltaTime);
            static void HandlePowerUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime);
            static void HandleSpellUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime);
            static void HandleCombatEventUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime);
        };
    }
}