#pragma once

#include "Server-Game/Gameplay/Faction/FactionRuntimeDefines.h"

namespace ECS::Components
{
    enum class UnitFactionFlags : u8
    {
        None = 0,
        HasEffectiveFactionOverride = 1 << 0,
        HasPlayerReactionBoundsOverride = 1 << 1,
        HasUnitReactionOverride = 1 << 2
    };

    struct UnitFaction
    {
    public:
        Gameplay::Faction::FactionIndex assignedFaction = Gameplay::Faction::NONE_FACTION_INDEX;
        Gameplay::Faction::FactionIndex effectiveFaction = Gameplay::Faction::NONE_FACTION_INDEX;
        u8 assignedPlayerReactionBounds = Gameplay::Faction::NEUTRAL_REACTION_BOUNDS;
        u8 effectivePlayerReactionBounds = Gameplay::Faction::NEUTRAL_REACTION_BOUNDS;
        u8 flags = 0;
        u8 effectiveUnitReactionOverride = static_cast<u8>(Gameplay::Faction::INHERIT_REACTION_BOUND);
    };

    static_assert(sizeof(UnitFaction) == 8, "UnitFaction must remain a compact hot component");
}
