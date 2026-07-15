#pragma once

#include "Server-Game/Gameplay/Faction/FactionRuntimeDefines.h"

#include <Server-Common/Database/Definitions.h>

#include <robinhood/robinhood.h>

#include <memory>
#include <string>
#include <vector>

namespace Gameplay::Faction
{
    struct DefinitionRuntime
    {
    public:
        FactionID id = NONE_FACTION_ID;
        std::string name = "None";
        u16 flags = 0;
        Reaction defaultReactionToOthers = Reaction::Neutral;
        u8 defaultPlayerReactionBounds = NEUTRAL_REACTION_BOUNDS;
        i32 defaultReputationValue = 0;
    };

    struct StandingThreshold
    {
    public:
        StandingID id = 0;
        std::string name;
        i32 minimumValue = 0;
        Reaction reaction = Reaction::Neutral;
        u16 sortOrder = 0;
    };

    struct FactionRuntimeData
    {
    public:
        bool TryGetFactionIndex(FactionID id, FactionIndex& index) const;
        FactionID GetFactionID(FactionIndex index) const;
        const DefinitionRuntime& GetDefinition(FactionIndex index) const;
        Reaction GetRelation(FactionIndex source, FactionIndex target) const;
        const StandingThreshold& GetStanding(i32 value) const;
        i32 GetStartingReputation(FactionIndex playerFaction, FactionIndex targetFaction) const;
        bool TryGetRaceFaction(u16 raceID, FactionIndex& faction) const;
        u8 ResolveCreatureReactionBounds(const Database::CreatureTemplate& creatureTemplate) const;

    public:
        robin_hood::unordered_flat_map<FactionID, FactionIndex> idToIndex;
        std::vector<FactionID> indexToID;
        std::vector<DefinitionRuntime> definitions;
        std::vector<u64> packedRelations;
        std::vector<StandingThreshold> standingThresholds;
        robin_hood::unordered_flat_map<u32, i32> startingReputationByPair;
        robin_hood::unordered_flat_map<u16, FactionIndex> raceToFaction;
        u32 wordsPerRelationRow = 0;
    };

    std::shared_ptr<const FactionRuntimeData> BuildRuntimeData(const Database::FactionTables& factionTables, const Database::CreatureTables& creatureTables);
}
