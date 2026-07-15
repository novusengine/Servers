#pragma once

#include "Server-Game/Gameplay/Faction/FactionGameplayDefines.h"
#include "Server-Game/Gameplay/Faction/FactionRuntimeDefines.h"

#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct ReputationState
    {
    public:
        i32 value = 0;
        u16 flags = 0;
    };

    enum class ReputationNetworkUpdateFlags : u8
    {
        None = 0,
        Immediate = 1 << 0
    };

    struct CharacterReputation
    {
    public:
        robin_hood::unordered_flat_map<Gameplay::Faction::FactionIndex, ReputationState> byFaction;
        robin_hood::unordered_flat_map<Gameplay::Faction::FactionIndex, Gameplay::Faction::ReputationSource> dirtyByFaction;
        robin_hood::unordered_flat_map<Gameplay::Faction::FactionIndex, Gameplay::Faction::ReputationSource> removedDirtyByFaction;
        robin_hood::unordered_flat_map<Gameplay::Faction::FactionIndex, ReputationNetworkUpdateFlags> pendingNetworkByFaction;
        u64 nextPersistenceEpoch = 0;
        u8 persistenceRetryCount = 0;
    };
}
