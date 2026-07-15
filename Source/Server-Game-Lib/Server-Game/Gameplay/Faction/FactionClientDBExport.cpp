#include "FactionClientDBExport.h"

#include <Base/Memory/Bytebuffer.h>
#include <Base/Util/DebugHandler.h>

#include <FileFormat/Novus/ClientDB/ClientDB.h>

#include <MetaGen/Shared/ClientDB/ClientDB.h>

#include <robinhood/robinhood.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <system_error>

namespace Gameplay::Faction
{
    namespace
    {
        struct SerializedArtifact
        {
        public:
            std::string name;
            std::shared_ptr<Bytebuffer> buffer;
        };

        template <typename Record>
        bool SerializeClientDB(ClientDB::Data& storage, std::string_view name, SerializedArtifact& artifact)
        {
            storage.Sort();
            const size_t size = storage.GetSerializedSize();
            if (size > std::numeric_limits<u32>::max())
            {
                NC_LOG_ERROR("Faction ClientDB export '{}' exceeds the Bytebuffer size limit", name);
                return false;
            }

            artifact.name = name;
            artifact.buffer = Bytebuffer::BorrowRuntime(static_cast<u32>(size));
            if (!artifact.buffer || !storage.Save(artifact.buffer))
            {
                NC_LOG_ERROR("Failed to serialize faction ClientDB '{}'", name);
                return false;
            }

            return true;
        }

        bool WriteAtomically(const std::filesystem::path& path, const void* bytes, size_t size)
        {
            const std::filesystem::path temporaryPath = path.string() + ".tmp";
            std::ofstream stream(temporaryPath, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                NC_LOG_ERROR("Failed to open temporary faction ClientDB export file '{}'", temporaryPath.string());
                return false;
            }

            stream.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
            stream.close();

            if (!stream)
            {
                NC_LOG_ERROR("Failed to write temporary faction ClientDB export file '{}'", temporaryPath.string());
                std::error_code cleanupError;
                std::filesystem::remove(temporaryPath, cleanupError);
                return false;
            }

            std::error_code error;
#ifdef _WIN32
            if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
            std::filesystem::rename(temporaryPath, path, error);
#endif
            if (error)
            {
                NC_LOG_ERROR("Failed to publish faction ClientDB export file '{}': {}", path.string(), error.message());
                std::error_code cleanupError;
                std::filesystem::remove(temporaryPath, cleanupError);
                return false;
            }

            return true;
        }

        bool WriteArtifact(const std::filesystem::path& directory, const SerializedArtifact& artifact)
        {
            const std::filesystem::path path = directory / (artifact.name + ClientDB::FILE_EXTENSION);
            return WriteAtomically(path, artifact.buffer->GetDataPointer(), artifact.buffer->writtenData);
        }

        bool ValidateSourceMatchesRuntime(const Database::FactionTables& tables, const FactionRuntimeData& runtime)
        {
            robin_hood::unordered_flat_set<FactionID> definitionIDs;
            definitionIDs.reserve(tables.definitions.size());
            for (const auto& source : tables.definitions)
            {
                FactionIndex index = INVALID_FACTION_INDEX;
                const ReactionBounds sourceBounds{
                    .minimum = static_cast<Reaction>(source.defaultPlayerReactionMin),
                    .maximum = static_cast<Reaction>(source.defaultPlayerReactionMax)
                };
                const bool validBounds = IsValidReaction(source.defaultPlayerReactionMin) && IsValidReaction(source.defaultPlayerReactionMax) && sourceBounds.IsValid();
                const u8 bounds = validBounds ? sourceBounds.Pack() : NEUTRAL_REACTION_BOUNDS;
                const bool valid = definitionIDs.insert(source.id).second && runtime.TryGetFactionIndex(source.id, index) && IsValidReaction(source.defaultReactionToOthers) && validBounds &&
                    (source.flags & ~DEFINITION_FLAG_MASK) == 0 && (HasFlag(source.flags, DefinitionFlags::AllowsReputation) || source.defaultReputationValue == 0);
                if (!valid)
                {
                    NC_LOG_ERROR("Faction ClientDB export rejected invalid faction definition {}", source.id);
                    return false;
                }

                const DefinitionRuntime& compiled = runtime.GetDefinition(index);
                if (compiled.name != source.name || compiled.flags != source.flags || compiled.defaultReactionToOthers != static_cast<Reaction>(source.defaultReactionToOthers) ||
                    compiled.defaultPlayerReactionBounds != bounds || compiled.defaultReputationValue != source.defaultReputationValue)
                {
                    NC_LOG_ERROR("Faction ClientDB export rejected faction {} because startup normalization changed it", source.id);
                    return false;
                }
            }

            if (runtime.definitions.size() != definitionIDs.size() + (definitionIDs.contains(NONE_FACTION_ID) ? 0 : 1))
            {
                NC_LOG_ERROR("Faction ClientDB export rejected inconsistent compiled faction definition count");
                return false;
            }

            if (runtime.standingThresholds.size() != tables.standings.size())
            {
                NC_LOG_ERROR("Faction ClientDB export rejected standings normalized during startup");
                return false;
            }

            robin_hood::unordered_flat_set<StandingID> standingIDs;
            standingIDs.reserve(tables.standings.size());
            for (const auto& source : tables.standings)
            {
                const auto compiled = std::find_if(runtime.standingThresholds.begin(), runtime.standingThresholds.end(), [&](const StandingThreshold& standing)
                {
                    return standing.id == source.id;
                });
                if (!standingIDs.insert(source.id).second || compiled == runtime.standingThresholds.end() || compiled->name != source.name || compiled->minimumValue != source.minimumValue ||
                    compiled->reaction != static_cast<Reaction>(source.reaction) || compiled->sortOrder != source.sortOrder)
                {
                    NC_LOG_ERROR("Faction ClientDB export rejected invalid faction standing {}", source.id);
                    return false;
                }
            }

            if (runtime.startingReputationByPair.size() != tables.startingReputations.size() || runtime.raceToFaction.size() != tables.unitRaceFactions.size())
            {
                NC_LOG_ERROR("Faction ClientDB export rejected normalized starting-reputation or race-faction rows");
                return false;
            }

            return true;
        }
    }

    bool ExportClientDB(const Database::FactionTables& factionTables, const FactionRuntimeData& runtime, const std::filesystem::path& patchStagingDirectory, ClientDBExportResult& result)
    {
        result = {};
        if (patchStagingDirectory.empty() || runtime.definitions.empty() || runtime.indexToID.size() != runtime.definitions.size())
        {
            NC_LOG_ERROR("Faction ClientDB export requires a staging directory and initialized faction runtime data");
            return false;
        }

        if (!ValidateSourceMatchesRuntime(factionTables, runtime))
            return false;

        ClientDB::Data factionStorage;
        ClientDB::Data relationStorage;
        ClientDB::Data standingStorage;
        if (!factionStorage.Initialize<MetaGen::Shared::ClientDB::FactionRecord>() || !relationStorage.Initialize<MetaGen::Shared::ClientDB::FactionRelationRecord>() || !standingStorage.Initialize<MetaGen::Shared::ClientDB::FactionStandingRecord>())
        {
            NC_LOG_ERROR("Failed to initialize faction ClientDB export storage");
            return false;
        }

        factionStorage.Reserve(static_cast<u32>(runtime.definitions.size()));
        for (const DefinitionRuntime& definition : runtime.definitions)
        {
            const ReactionBounds bounds = ReactionBounds::Unpack(definition.defaultPlayerReactionBounds);
            const MetaGen::Shared::ClientDB::FactionRecord record{
                .name = factionStorage.AddString(definition.name),
                .flags = definition.flags,
                .defaultReactionToOthers = static_cast<u8>(definition.defaultReactionToOthers),
                .defaultPlayerReactionMin = static_cast<u8>(bounds.minimum),
                .defaultPlayerReactionMax = static_cast<u8>(bounds.maximum),
                .defaultReputationValue = definition.defaultReputationValue
            };
            factionStorage.Replace(definition.id, record);
        }

        std::vector<const MetaGen::Postgres::World::FactionRelationsRecord*> relations;
        relations.reserve(factionTables.relations.size());
        robin_hood::unordered_flat_set<u32> relationKeys;
        relationKeys.reserve(factionTables.relations.size());
        for (const auto& relation : factionTables.relations)
        {
            FactionIndex source = INVALID_FACTION_INDEX;
            FactionIndex target = INVALID_FACTION_INDEX;
            const u32 key = (static_cast<u32>(relation.sourceFactionId) << 16u) | relation.targetFactionId;
            if (relation.sourceFactionId == NONE_FACTION_ID || relation.targetFactionId == NONE_FACTION_ID || !runtime.TryGetFactionIndex(relation.sourceFactionId, source) ||
                !runtime.TryGetFactionIndex(relation.targetFactionId, target) || !IsValidReaction(relation.reaction) || !relationKeys.insert(key).second)
            {
                NC_LOG_ERROR("Faction ClientDB export rejected invalid or duplicate relation {} -> {}", relation.sourceFactionId, relation.targetFactionId);
                return false;
            }

            relations.push_back(&relation);
        }

        std::sort(relations.begin(), relations.end(), [](const auto* left, const auto* right)
        {
            if (left->sourceFactionId != right->sourceFactionId)
                return left->sourceFactionId < right->sourceFactionId;

            return left->targetFactionId < right->targetFactionId;
        });
        relationStorage.Reserve(static_cast<u32>(relations.size()));
        for (const auto* relation : relations)
        {
            relationStorage.Add(MetaGen::Shared::ClientDB::FactionRelationRecord{
                .sourceFactionID = relation->sourceFactionId,
                .targetFactionID = relation->targetFactionId,
                .reaction = static_cast<u8>(relation->reaction)
            });
        }

        standingStorage.Reserve(static_cast<u32>(runtime.standingThresholds.size()));
        for (const StandingThreshold& standing : runtime.standingThresholds)
        {
            standingStorage.Replace(standing.id, MetaGen::Shared::ClientDB::FactionStandingRecord{
                .name = standingStorage.AddString(standing.name),
                .minimumValue = standing.minimumValue,
                .reaction = static_cast<u8>(standing.reaction),
                .sortOrder = standing.sortOrder
            });
        }

        std::vector<SerializedArtifact> artifacts(3);
        if (!SerializeClientDB<MetaGen::Shared::ClientDB::FactionRecord>(factionStorage, "Faction", artifacts[0]) || !SerializeClientDB<MetaGen::Shared::ClientDB::FactionRelationRecord>(relationStorage, "FactionRelation", artifacts[1]) ||
            !SerializeClientDB<MetaGen::Shared::ClientDB::FactionStandingRecord>(standingStorage, "FactionStanding", artifacts[2]))
            return false;

        result.factionCount = static_cast<u32>(runtime.definitions.size());
        result.relationCount = static_cast<u32>(relations.size());
        result.standingCount = static_cast<u32>(runtime.standingThresholds.size());

        const std::filesystem::path clientDBDirectory = patchStagingDirectory / "clientdb";
        std::error_code error;
        std::filesystem::create_directories(clientDBDirectory, error);
        if (error)
        {
            NC_LOG_ERROR("Failed to create faction ClientDB staging directory '{}': {}", clientDBDirectory.string(), error.message());
            return false;
        }

        for (const SerializedArtifact& artifact : artifacts)
        {
            if (!WriteArtifact(clientDBDirectory, artifact))
                return false;
        }

        return true;
    }
}
