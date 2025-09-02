#pragma once
#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct VisibilityInfo
    {
    public:
        robin_hood::unordered_set<ObjectGUID> visiblePlayers;
        robin_hood::unordered_set<ObjectGUID> visibleCreatures;
    };
}