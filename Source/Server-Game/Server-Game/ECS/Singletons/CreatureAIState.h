#pragma once
#include <Base/Types.h>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

namespace ECS::Singletons
{
    struct CreatureAIState
    {
    public:
        robin_hood::unordered_set<u32> loadedScriptNameHashes;
        robin_hood::unordered_map<ObjectGUID, u32> creatureGUIDToScriptNameHash;
    };
}