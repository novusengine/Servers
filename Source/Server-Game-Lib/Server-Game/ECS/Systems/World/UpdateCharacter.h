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
        class UpdateCharacter
        {
        public:
            static void HandleDeinitialization(Singletons::WorldState& worldState, World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleInitialization(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState);
            static void HandleUpdate(Singletons::WorldState& worldState, World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime);
        };
    }
}