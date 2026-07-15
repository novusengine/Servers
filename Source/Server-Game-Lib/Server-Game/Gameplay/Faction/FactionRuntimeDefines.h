#pragma once

#include <Gameplay/Faction/FactionDefines.h>

#include <limits>

namespace Gameplay::Faction
{
    using FactionIndex = u16;

    static constexpr FactionIndex NONE_FACTION_INDEX = 0;
    static constexpr FactionIndex INVALID_FACTION_INDEX = std::numeric_limits<FactionIndex>::max();
}
