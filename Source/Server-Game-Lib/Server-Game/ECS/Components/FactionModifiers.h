#pragma once

#include "Server-Game/Gameplay/Faction/FactionModifierDefines.h"

#include <robinhood/robinhood.h>

#include <vector>

namespace ECS::Components
{
    static constexpr u32 MAX_FACTION_MODIFIER_CONTRIBUTORS = 64;

    struct ResolvedFactionPerception
    {
    public:
        i32 effectiveStandingValue = 0;
        Gameplay::Faction::Reaction effectiveReaction = Gameplay::Faction::Reaction::Neutral;
        u8 activeFields = 0;
    };

    struct FactionModifiers
    {
    public:
        std::vector<Gameplay::Faction::ModifierContributor> contributors;
        robin_hood::unordered_flat_map<Gameplay::Faction::FactionIndex, ResolvedFactionPerception> perceptionByFaction;
        robin_hood::unordered_flat_set<Gameplay::Faction::FactionIndex> pendingPerceptionNetwork;
    };

    struct FactionModifierApplication
    {
    public:
        u64 applicationSequence = 0;
    };
}
