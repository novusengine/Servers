#include "FactionRuntimeData.h"

#include <Base/Util/DebugHandler.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace Gameplay::Faction
{
    namespace
    {
        u32 MakePairKey(FactionIndex source, FactionIndex target)
        {
            return (static_cast<u32>(source) << 16) | static_cast<u32>(target);
        }

        void SetRelation(FactionRuntimeData& runtime, FactionIndex source, FactionIndex target, Reaction reaction)
        {
            const size_t wordIndex = static_cast<size_t>(source) * runtime.wordsPerRelationRow + target / 32;
            const u32 shift = static_cast<u32>(target % 32) * 2;
            const u64 mask = u64{ 0x3 } << shift;
            runtime.packedRelations[wordIndex] = (runtime.packedRelations[wordIndex] & ~mask) | (static_cast<u64>(reaction) << shift);
        }

        bool TryPackBounds(u16 minimum, u16 maximum, u8& packed)
        {
            if (!IsValidReaction(minimum) || !IsValidReaction(maximum) || minimum > maximum)
                return false;

            const ReactionBounds bounds{
                .minimum = static_cast<Reaction>(minimum),
                .maximum = static_cast<Reaction>(maximum)
            };
            packed = bounds.Pack();
            return true;
        }

        void ResetStandingToNeutral(FactionRuntimeData& runtime)
        {
            runtime.standingThresholds.clear();
            runtime.standingThresholds.push_back({
                .id = 0,
                .name = "Neutral",
                .minimumValue = std::numeric_limits<i32>::min(),
                .reaction = Reaction::Neutral,
                .sortOrder = 0
            });
        }
    }

    bool FactionRuntimeData::TryGetFactionIndex(FactionID id, FactionIndex& index) const
    {
        const auto itr = idToIndex.find(id);
        if (itr == idToIndex.end())
        {
            index = INVALID_FACTION_INDEX;
            return false;
        }

        index = itr->second;
        return true;
    }

    FactionID FactionRuntimeData::GetFactionID(FactionIndex index) const
    {
        return index < indexToID.size() ? indexToID[index] : NONE_FACTION_ID;
    }

    const DefinitionRuntime& FactionRuntimeData::GetDefinition(FactionIndex index) const
    {
        NC_ASSERT(!definitions.empty(), "Faction runtime has no definitions");
        return definitions.at(index < definitions.size() ? index : NONE_FACTION_INDEX);
    }

    Reaction FactionRuntimeData::GetRelation(FactionIndex source, FactionIndex target) const
    {
        if (source >= definitions.size() || target >= definitions.size() || wordsPerRelationRow == 0)
            return Reaction::Neutral;

        const size_t wordIndex = static_cast<size_t>(source) * wordsPerRelationRow + target / 32;
        const u32 shift = static_cast<u32>(target % 32) * 2;
        return static_cast<Reaction>((packedRelations[wordIndex] >> shift) & 0x3);
    }

    const StandingThreshold& FactionRuntimeData::GetStanding(i32 value) const
    {
        NC_ASSERT(!standingThresholds.empty(), "Faction runtime has no standing thresholds");

        const auto itr = std::upper_bound(standingThresholds.begin(), standingThresholds.end(), value, [](i32 candidate, const StandingThreshold& threshold)
        {
            return candidate < threshold.minimumValue;
        });

        return itr == standingThresholds.begin() ? standingThresholds.at(0) : *std::prev(itr);
    }

    i32 FactionRuntimeData::GetStartingReputation(FactionIndex playerFaction, FactionIndex targetFaction) const
    {
        if (playerFaction >= definitions.size() || targetFaction >= definitions.size())
            return 0;

        const auto itr = startingReputationByPair.find(MakePairKey(playerFaction, targetFaction));
        if (itr != startingReputationByPair.end())
            return itr->second;

        return definitions[targetFaction].defaultReputationValue;
    }

    bool FactionRuntimeData::TryGetRaceFaction(u16 raceID, FactionIndex& faction) const
    {
        const auto itr = raceToFaction.find(raceID);
        if (itr == raceToFaction.end())
        {
            faction = INVALID_FACTION_INDEX;
            return false;
        }

        faction = itr->second;
        return true;
    }

    u8 FactionRuntimeData::ResolveCreatureReactionBounds(const Database::CreatureTemplate& creatureTemplate) const
    {
        FactionIndex faction = NONE_FACTION_INDEX;
        const u8 factionBounds = TryGetFactionIndex(creatureTemplate.factionId, faction) ? GetDefinition(faction).defaultPlayerReactionBounds : NEUTRAL_REACTION_BOUNDS;

        const bool inheritsMinimum = creatureTemplate.playerReactionMinOverride == INHERIT_REACTION_BOUND;
        const bool inheritsMaximum = creatureTemplate.playerReactionMaxOverride == INHERIT_REACTION_BOUND;
        if (inheritsMinimum || inheritsMaximum)
            return inheritsMinimum && inheritsMaximum ? factionBounds : NEUTRAL_REACTION_BOUNDS;

        u8 packed = NEUTRAL_REACTION_BOUNDS;
        return TryPackBounds(creatureTemplate.playerReactionMinOverride, creatureTemplate.playerReactionMaxOverride, packed) ? packed : NEUTRAL_REACTION_BOUNDS;
    }

    std::shared_ptr<const FactionRuntimeData> BuildRuntimeData(const Database::FactionTables& factionTables, const Database::CreatureTables& creatureTables)
    {
        FactionRuntimeData runtime;
        bool contentIsValid = true;
        runtime.idToIndex.reserve(factionTables.definitions.size() + 1);
        runtime.indexToID.reserve(factionTables.definitions.size() + 1);
        runtime.definitions.reserve(factionTables.definitions.size() + 1);

        const MetaGen::Postgres::World::FactionsRecord* noneRecord = nullptr;
        std::vector<const MetaGen::Postgres::World::FactionsRecord*> sortedDefinitions;
        robin_hood::unordered_flat_set<FactionID> factionIDs;

        sortedDefinitions.reserve(factionTables.definitions.size());
        factionIDs.reserve(factionTables.definitions.size() + 1);

        for (const auto& record : factionTables.definitions)
        {
            if (!factionIDs.insert(record.id).second)
            {
                NC_LOG_ERROR("Faction content has duplicate definition ID {0}", record.id);
                contentIsValid = false;
                continue;
            }

            if (record.id == NONE_FACTION_ID)
            {
                noneRecord = &record;
            }
            else
            {
                sortedDefinitions.push_back(&record);
            }
        }

        std::sort(sortedDefinitions.begin(), sortedDefinitions.end(), [](const auto* left, const auto* right)
        {
            return left->id < right->id;
        });

        const size_t maximumDefinitions = static_cast<size_t>(INVALID_FACTION_INDEX) - 1;
        if (sortedDefinitions.size() > maximumDefinitions)
        {
            NC_LOG_ERROR("Faction content contains {0} nonzero definitions, but only {1} fit in the runtime index range", sortedDefinitions.size(), maximumDefinitions);
            contentIsValid = false;
            sortedDefinitions.resize(maximumDefinitions);
        }

        DefinitionRuntime noneDefinition;
        if (noneRecord)
        {
            noneDefinition.name = noneRecord->name;
            if (noneRecord->flags != 0 || noneRecord->defaultReactionToOthers != static_cast<u16>(Reaction::Neutral) || noneRecord->defaultPlayerReactionMin != static_cast<u16>(Reaction::Neutral) || noneRecord->defaultPlayerReactionMax != static_cast<u16>(Reaction::Neutral) || noneRecord->defaultReputationValue != 0)
            {
                NC_LOG_ERROR("Faction 0 is not a strict neutral None definition");
                contentIsValid = false;
            }
        }
        else
        {
            NC_LOG_WARNING("Faction 0 is missing; a strict neutral None definition was synthesized");
        }

        runtime.idToIndex.emplace(NONE_FACTION_ID, NONE_FACTION_INDEX);
        runtime.indexToID.push_back(NONE_FACTION_ID);
        runtime.definitions.push_back(std::move(noneDefinition));

        bool anyFactionAllowsReputation = false;
        for (const auto* record : sortedDefinitions)
        {
            u16 flags = record->flags;
            if ((flags & ~DEFINITION_FLAG_MASK) != 0)
            {
                NC_LOG_ERROR("Faction {0} has unknown definition flags {1}", record->id, flags & ~DEFINITION_FLAG_MASK);
                contentIsValid = false;
                flags &= DEFINITION_FLAG_MASK;
            }

            Reaction defaultReaction = Reaction::Neutral;
            if (IsValidReaction(record->defaultReactionToOthers))
            {
                defaultReaction = static_cast<Reaction>(record->defaultReactionToOthers);
            }
            else
            {
                NC_LOG_ERROR("Faction {0} has invalid default reaction {1}", record->id, record->defaultReactionToOthers);
                contentIsValid = false;
            }

            u8 bounds = NEUTRAL_REACTION_BOUNDS;
            if (!TryPackBounds(record->defaultPlayerReactionMin, record->defaultPlayerReactionMax, bounds))
            {
                NC_LOG_ERROR("Faction {0} has invalid player reaction bounds {1}..{2}", record->id, record->defaultPlayerReactionMin, record->defaultPlayerReactionMax);
                contentIsValid = false;
            }

            const bool allowsReputation = HasFlag(flags, DefinitionFlags::AllowsReputation);
            i32 defaultReputationValue = record->defaultReputationValue;
            if (!allowsReputation && defaultReputationValue != 0)
            {
                NC_LOG_ERROR("Faction {0} has default reputation {1} but does not allow reputation", record->id, defaultReputationValue);
                contentIsValid = false;
                defaultReputationValue = 0;
            }

            const auto index = static_cast<FactionIndex>(runtime.definitions.size());
            runtime.idToIndex.emplace(record->id, index);
            runtime.indexToID.push_back(record->id);
            runtime.definitions.push_back({
                .id = record->id,
                .name = record->name,
                .flags = flags,
                .defaultReactionToOthers = defaultReaction,
                .defaultPlayerReactionBounds = bounds,
                .defaultReputationValue = defaultReputationValue
            });

            anyFactionAllowsReputation |= allowsReputation;
        }

        const size_t factionCount = runtime.definitions.size();
        runtime.wordsPerRelationRow = static_cast<u32>((factionCount + 31) / 32);
        runtime.packedRelations.assign(factionCount * runtime.wordsPerRelationRow, 0);

        for (size_t source = 0; source < factionCount; ++source)
        {
            const auto sourceIndex = static_cast<FactionIndex>(source);
            const Reaction rowDefault = runtime.definitions[source].defaultReactionToOthers;

            for (size_t target = 0; target < factionCount; ++target)
            {
                SetRelation(runtime, sourceIndex, static_cast<FactionIndex>(target), rowDefault);
            }

            SetRelation(runtime, sourceIndex, NONE_FACTION_INDEX, Reaction::Neutral);
            SetRelation(runtime, sourceIndex, sourceIndex, sourceIndex == NONE_FACTION_INDEX ? Reaction::Neutral : Reaction::Friendly);
        }

        for (size_t target = 0; target < factionCount; ++target)
        {
            SetRelation(runtime, NONE_FACTION_INDEX, static_cast<FactionIndex>(target), Reaction::Neutral);
        }

        robin_hood::unordered_flat_set<u32> relationPairs;
        relationPairs.reserve(factionTables.relations.size());
        for (const auto& relation : factionTables.relations)
        {
            if (relation.sourceFactionId == NONE_FACTION_ID || relation.targetFactionId == NONE_FACTION_ID)
            {
                NC_LOG_ERROR("Faction relation {0}->{1} is invalid: faction 0 must remain Neutral", relation.sourceFactionId, relation.targetFactionId);
                contentIsValid = false;
                continue;
            }

            FactionIndex source = INVALID_FACTION_INDEX;
            FactionIndex target = INVALID_FACTION_INDEX;
            if (!runtime.TryGetFactionIndex(relation.sourceFactionId, source) || !runtime.TryGetFactionIndex(relation.targetFactionId, target))
            {
                NC_LOG_ERROR("Faction relation {0}->{1} references an unknown faction", relation.sourceFactionId, relation.targetFactionId);
                contentIsValid = false;
                continue;
            }

            const u32 pair = MakePairKey(source, target);
            if (!relationPairs.insert(pair).second)
            {
                NC_LOG_ERROR("Faction content has duplicate relation {0}->{1}", relation.sourceFactionId, relation.targetFactionId);
                contentIsValid = false;
                continue;
            }

            if (!IsValidReaction(relation.reaction))
            {
                NC_LOG_ERROR("Faction relation {0}->{1} has invalid reaction {2}", relation.sourceFactionId, relation.targetFactionId, relation.reaction);
                contentIsValid = false;
                continue;
            }

            SetRelation(runtime, source, target, static_cast<Reaction>(relation.reaction));
        }

        std::vector<const MetaGen::Postgres::World::FactionStandingsRecord*> sortedStandings;
        sortedStandings.reserve(factionTables.standings.size());

        for (const auto& standing : factionTables.standings)
        {
            sortedStandings.push_back(&standing);
        }

        std::sort(sortedStandings.begin(), sortedStandings.end(), [](const auto* left, const auto* right)
        {
            if (left->minimumValue != right->minimumValue)
                return left->minimumValue < right->minimumValue;

            return left->id < right->id;
        });

        robin_hood::unordered_flat_set<StandingID> standingIDs;
        standingIDs.reserve(sortedStandings.size());

        Reaction previousReaction = Reaction::Hostile;
        i32 previousMinimum = 0;
        u16 previousSortOrder = 0;
        bool firstStanding = true;

        for (const auto* standing : sortedStandings)
        {
            if (!standingIDs.insert(standing->id).second)
            {
                NC_LOG_ERROR("Faction content has duplicate standing ID {0}", standing->id);
                contentIsValid = false;
                continue;
            }

            if (!IsValidReaction(standing->reaction))
            {
                NC_LOG_ERROR("Faction standing {0} has invalid reaction {1}", standing->id, standing->reaction);
                contentIsValid = false;
                continue;
            }

            if (!firstStanding && standing->minimumValue <= previousMinimum)
            {
                NC_LOG_ERROR("Faction standing {0} minimum value {1} is not strictly greater than {2}", standing->id, standing->minimumValue, previousMinimum);
                contentIsValid = false;
                continue;
            }

            if (!firstStanding && standing->sortOrder <= previousSortOrder)
            {
                NC_LOG_ERROR("Faction standing {0} sort order {1} is not greater than {2}", standing->id, standing->sortOrder, previousSortOrder);
                contentIsValid = false;
                continue;
            }

            const Reaction reaction = static_cast<Reaction>(standing->reaction);
            if (!firstStanding && static_cast<u8>(reaction) < static_cast<u8>(previousReaction))
            {
                NC_LOG_ERROR("Faction standing {0} reaction ordering is not monotonic", standing->id);
                contentIsValid = false;
                continue;
            }

            runtime.standingThresholds.push_back({
                .id = standing->id,
                .name = standing->name,
                .minimumValue = standing->minimumValue,
                .reaction = reaction,
                .sortOrder = standing->sortOrder
            });

            previousMinimum = standing->minimumValue;
            previousSortOrder = standing->sortOrder;
            previousReaction = reaction;
            firstStanding = false;
        }

        if (runtime.standingThresholds.empty())
        {
            if (anyFactionAllowsReputation)
            {
                NC_LOG_ERROR("Reputation-enabled factions require standing thresholds");
                contentIsValid = false;
            }

            ResetStandingToNeutral(runtime);
        }
        else if (runtime.standingThresholds.front().minimumValue > 0 || runtime.GetStanding(0).reaction != Reaction::Neutral)
        {
            NC_LOG_ERROR("Faction standing thresholds do not map value 0 to Neutral");
            contentIsValid = false;
            ResetStandingToNeutral(runtime);
        }

        runtime.startingReputationByPair.reserve(factionTables.startingReputations.size());
        for (const auto& startingReputation : factionTables.startingReputations)
        {
            FactionIndex playerFaction = INVALID_FACTION_INDEX;
            FactionIndex targetFaction = INVALID_FACTION_INDEX;
            if (!runtime.TryGetFactionIndex(startingReputation.playerFactionId, playerFaction) || !runtime.TryGetFactionIndex(startingReputation.targetFactionId, targetFaction))
            {
                NC_LOG_ERROR("Starting reputation {0}->{1} references an unknown faction", startingReputation.playerFactionId, startingReputation.targetFactionId);
                contentIsValid = false;
                continue;
            }

            if (!HasFlag(runtime.definitions[targetFaction].flags, DefinitionFlags::AllowsReputation))
            {
                NC_LOG_ERROR("Starting reputation {0}->{1} targets a faction that does not allow reputation", startingReputation.playerFactionId, startingReputation.targetFactionId);
                contentIsValid = false;
                continue;
            }

            const u32 pair = MakePairKey(playerFaction, targetFaction);
            if (!runtime.startingReputationByPair.emplace(pair, startingReputation.value).second)
            {
                NC_LOG_ERROR("Faction content has duplicate starting reputation {0}->{1}", startingReputation.playerFactionId, startingReputation.targetFactionId);
                contentIsValid = false;
            }
        }

        runtime.raceToFaction.reserve(factionTables.unitRaceFactions.size());
        for (const auto& raceFaction : factionTables.unitRaceFactions)
        {
            FactionIndex faction = INVALID_FACTION_INDEX;
            if (raceFaction.raceId == 0 || raceFaction.factionId == NONE_FACTION_ID || !runtime.TryGetFactionIndex(raceFaction.factionId, faction))
            {
                NC_LOG_ERROR("Unit race faction mapping {0}->{1} has an invalid race or faction", raceFaction.raceId, raceFaction.factionId);
                contentIsValid = false;
                continue;
            }

            if (!runtime.raceToFaction.emplace(raceFaction.raceId, faction).second)
            {
                NC_LOG_ERROR("Faction content has duplicate mapping for unit race {0}", raceFaction.raceId);
                contentIsValid = false;
            }
        }

        for (const auto& [templateID, creatureTemplate] : creatureTables.templateIDToDefinition)
        {
            FactionIndex faction = INVALID_FACTION_INDEX;
            if (!runtime.TryGetFactionIndex(creatureTemplate.factionId, faction))
            {
                NC_LOG_ERROR("Creature template {0} references unknown faction {1}", templateID, creatureTemplate.factionId);
                contentIsValid = false;
            }

            const bool inheritsMinimum = creatureTemplate.playerReactionMinOverride == INHERIT_REACTION_BOUND;
            const bool inheritsMaximum = creatureTemplate.playerReactionMaxOverride == INHERIT_REACTION_BOUND;
            if (inheritsMinimum != inheritsMaximum)
            {
                NC_LOG_ERROR("Creature template {0} must inherit both player reaction bounds or neither", templateID);
                contentIsValid = false;
                continue;
            }

            u8 packed = NEUTRAL_REACTION_BOUNDS;
            if (!inheritsMinimum && !TryPackBounds(creatureTemplate.playerReactionMinOverride, creatureTemplate.playerReactionMaxOverride, packed))
            {
                NC_LOG_ERROR("Creature template {0} has invalid player reaction bounds {1}..{2}", templateID, creatureTemplate.playerReactionMinOverride, creatureTemplate.playerReactionMaxOverride);
                contentIsValid = false;
            }
        }

        if (!contentIsValid)
        {
            NC_LOG_ERROR("Faction runtime initialization rejected invalid content; correct the preceding errors before startup");
            return nullptr;
        }

        return std::make_shared<const FactionRuntimeData>(std::move(runtime));
    }
}
