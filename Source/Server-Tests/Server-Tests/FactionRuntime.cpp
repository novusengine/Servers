#include <Server-Game/ECS/Components/CharacterReputation.h>
#include <Server-Game/ECS/Components/FactionModifiers.h>
#include <Server-Game/ECS/Components/UnitFaction.h>
#include <Server-Game/ECS/Components/Events.h>
#include <Server-Game/ECS/Singletons/NetworkState.h>
#include <Server-Game/ECS/Singletons/WorldState.h>
#include <Server-Game/ECS/Util/FactionModifierUtil.h>
#include <Server-Game/ECS/Util/FactionUtil.h>
#include <Server-Game/Gameplay/Faction/FactionClientDBExport.h>
#include <Server-Game/Gameplay/Faction/CreatureFactionPolicyDefines.h>
#include <Server-Game/Gameplay/Faction/FactionRuntimeData.h>

#include <Gameplay/Faction/FactionContentHash.h>

#include <MetaGen/Shared/ClientDB/ClientDB.h>
#include <MetaGen/Shared/Packet/Packet.h>

#include <catch2/catch2.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using namespace Gameplay::Faction;
    using namespace MetaGen::Postgres::World;

    FactionsRecord MakeFaction(u16 id, std::string name, Reaction defaultReaction = Reaction::Neutral, u16 flags = 0, i32 defaultReputation = 0)
    {
        return {
            .id = id,
            .name = std::move(name),
            .flags = flags,
            .defaultReactionToOthers = static_cast<u16>(defaultReaction),
            .defaultPlayerReactionMin = static_cast<u16>(Reaction::Hostile),
            .defaultPlayerReactionMax = static_cast<u16>(Reaction::Friendly),
            .defaultReputationValue = defaultReputation
        };
    }

    Database::FactionTables MakeFactionTables()
    {
        Database::FactionTables tables;
        tables.definitions.push_back(MakeFaction(20, "Reputation", Reaction::Unfriendly, static_cast<u16>(DefinitionFlags::AllowsReputation), 25));
        tables.definitions.push_back(MakeFaction(10, "Player", Reaction::Hostile));
        tables.relations.push_back({
            .id = 1,
            .sourceFactionId = 10,
            .targetFactionId = 20,
            .reaction = static_cast<u16>(Reaction::Friendly)
        });
        tables.standings.push_back({
            .id = 1,
            .name = "Hostile",
            .minimumValue = -1000,
            .reaction = static_cast<u16>(Reaction::Hostile),
            .sortOrder = 1
        });
        tables.standings.push_back({
            .id = 2,
            .name = "Neutral",
            .minimumValue = 0,
            .reaction = static_cast<u16>(Reaction::Neutral),
            .sortOrder = 2
        });
        tables.standings.push_back({
            .id = 3,
            .name = "Friendly",
            .minimumValue = 1000,
            .reaction = static_cast<u16>(Reaction::Friendly),
            .sortOrder = 3
        });
        tables.startingReputations.push_back({
            .id = 1,
            .playerFactionId = 10,
            .targetFactionId = 20,
            .value = 125
        });
        tables.unitRaceFactions.push_back({ .raceId = 1, .factionId = 10 });

        return tables;
    }

    u32 modifierConflictCount = 0;

    void CountModifierConflict(const FactionModifierConflict& conflict)
    {
        (void)conflict;
        ++modifierConflictCount;
    }
}

TEST_CASE("Faction modifier arbitration is bounded and deterministic", "[Faction][Modifier]")
{
    using ECS::Components::FactionModifiers;
    using ECS::Util::FactionModifier::AddContributor;
    using ECS::Util::FactionModifier::FindWinningContributor;
    using ECS::Util::FactionModifier::RemoveContributors;

    FactionModifiers modifiers;
    const auto Contributor = [](u32 aura, u32 spell, u8 priority, u64 sequence, i32 value)
    {
        return ModifierContributor{
            .sourceAuraInstanceID = aura,
            .spellID = spell,
            .effectID = spell * 10,
            .field = ModifierField::UnitReaction,
            .priority = priority,
            .applicationSequence = sequence,
            .value = value
        };
    };

    modifierConflictCount = 0;
    REQUIRE(AddContributor(modifiers, Contributor(1, 100, 5, 1, 1), 50, 7, CountModifierConflict));
    REQUIRE(AddContributor(modifiers, Contributor(2, 200, 6, 2, 2), 50, 7, CountModifierConflict));
    REQUIRE(FindWinningContributor(modifiers, ModifierField::UnitReaction));
    CHECK(FindWinningContributor(modifiers, ModifierField::UnitReaction)->spellID == 200);
    CHECK(modifierConflictCount == 0);

    REQUIRE(AddContributor(modifiers, Contributor(3, 300, 6, 3, 3), 50, 7, CountModifierConflict));
    CHECK(FindWinningContributor(modifiers, ModifierField::UnitReaction)->spellID == 200);
    CHECK(modifierConflictCount == 1);

    REQUIRE(AddContributor(modifiers, Contributor(4, 400, 6, 4, 2), 50, 7, CountModifierConflict));
    CHECK(modifierConflictCount == 1);

    // Equal-priority conflicts are audited even while a higher-priority field winner is active.
    REQUIRE(AddContributor(modifiers, Contributor(5, 500, 5, 5, 3), 50, 7, CountModifierConflict));
    CHECK(modifierConflictCount == 2);
    CHECK(RemoveContributors(modifiers, 3) == 1);
    CHECK(FindWinningContributor(modifiers, ModifierField::UnitReaction)->spellID == 200);

    CHECK(RemoveContributors(modifiers, 2) == 1);
    CHECK(FindWinningContributor(modifiers, ModifierField::UnitReaction)->spellID == 400);

    const ModifierContributor refreshed = Contributor(4, 400, 6, 4, 2);
    CHECK(RemoveContributors(modifiers, 4) == 1);
    REQUIRE(AddContributor(modifiers, refreshed, 50, 7, CountModifierConflict));
    CHECK(FindWinningContributor(modifiers, ModifierField::UnitReaction)->applicationSequence == 4);

    CHECK(sizeof(ECS::Components::UnitFaction) == 8);
    CHECK(sizeof(ModifierContributor) <= 48);

    while (modifiers.contributors.size() < ECS::Components::MAX_FACTION_MODIFIER_CONTRIBUTORS)
    {
        const u32 aura = static_cast<u32>(modifiers.contributors.size() + 10);
        REQUIRE(AddContributor(modifiers, Contributor(aura, aura, 1, aura, 0), 50, 7, CountModifierConflict));
    }
    CHECK_FALSE(AddContributor(modifiers, Contributor(999, 999, 1, 999, 0), 50, 7, CountModifierConflict));
}

TEST_CASE("Faction modifier recomputation restores prior winners and assigned defaults", "[Faction][Modifier]")
{
    ECS::Singletons::PacketArenaConfig packetArenaConfig;
    packetArenaConfig.globalMaxReservedBytes = 1024 * 1024;
    packetArenaConfig.maxReservedBytes = 1024 * 1024;
    packetArenaConfig.blockSize = 64 * 1024;
    ECS::World world(std::make_shared<PacketArenaBudget>(packetArenaConfig.globalMaxReservedBytes), packetArenaConfig);
    const entt::entity entity = world.CreateEntity();
    auto& faction = world.Emplace<ECS::Components::UnitFaction>(entity, ECS::Components::UnitFaction{
        .assignedFaction = 1,
        .effectiveFaction = 1,
        .assignedPlayerReactionBounds = ReactionBounds{ Reaction::Unfriendly, Reaction::Friendly }.Pack(),
        .effectivePlayerReactionBounds = ReactionBounds{ Reaction::Unfriendly, Reaction::Friendly }.Pack()
    });
    auto& modifiers = world.Emplace<ECS::Components::FactionModifiers>(entity);
    modifiers.contributors.push_back({
        .sourceAuraInstanceID = 1,
        .field = ModifierField::EffectiveFaction,
        .priority = 5,
        .applicationSequence = 1,
        .value = 2
    });
    modifiers.contributors.push_back({
        .sourceAuraInstanceID = 2,
        .field = ModifierField::EffectiveFaction,
        .priority = 6,
        .applicationSequence = 2,
        .value = 3
    });
    modifiers.contributors.push_back({
        .sourceAuraInstanceID = 3,
        .field = ModifierField::PlayerReactionMinimum,
        .priority = 1,
        .applicationSequence = 3,
        .value = static_cast<i32>(Reaction::Neutral)
    });

    ECS::Util::FactionModifier::RecomputeResolvedState(world, entity, modifiers);
    CHECK(faction.effectiveFaction == 3);
    CHECK(ReactionBounds::Unpack(faction.effectivePlayerReactionBounds).minimum == Reaction::Neutral);
    CHECK(world.AllOf<ECS::Events::UnitNeedsFactionUpdate>(entity));

    world.Remove<ECS::Events::UnitNeedsFactionUpdate>(entity);
    CHECK(ECS::Util::FactionModifier::RemoveContributors(modifiers, 2) == 1);
    ECS::Util::FactionModifier::RecomputeResolvedState(world, entity, modifiers);
    CHECK(faction.effectiveFaction == 2);
    CHECK(world.AllOf<ECS::Events::UnitNeedsFactionUpdate>(entity));

    CHECK(ECS::Util::FactionModifier::RemoveContributors(modifiers, 1) == 1);
    CHECK(ECS::Util::FactionModifier::RemoveContributors(modifiers, 3) == 1);
    ECS::Util::FactionModifier::RecomputeResolvedState(world, entity, modifiers);
    CHECK(faction.effectiveFaction == faction.assignedFaction);
    CHECK(faction.effectivePlayerReactionBounds == faction.assignedPlayerReactionBounds);
    CHECK(faction.effectiveUnitReactionOverride == INHERIT_REACTION_BOUND);
    CHECK(faction.flags == 0);
}

TEST_CASE("Faction network contracts use stable IDs and round trip absolute state", "[Faction][Network]")
{
    using namespace MetaGen::Shared::Packet;

    static_assert(std::is_same_v<decltype(ServerUnitFactionUpdatePacket::factionID), u16>);
    static_assert(std::is_same_v<decltype(ServerReputationUpdatePacket::factionID), u16>);
    static_assert(std::is_same_v<decltype(ServerFactionPerceptionOverrideUpdatePacket::factionID), u16>);
    static_assert(std::is_same_v<decltype(MetaGen::Shared::ClientDB::FactionRelationRecord::sourceFactionID), u16>);
    static_assert(std::is_same_v<decltype(MetaGen::Shared::ClientDB::FactionRelationRecord::targetFactionID), u16>);
    static_assert(std::is_same_v<decltype(MetaGen::Shared::ClientDB::UnitRaceRecord::factionID), u16>);

    SECTION("unit faction")
    {
        const ServerUnitFactionUpdatePacket source{
            .guid = ObjectGUID::CreateCreature(0x12345),
            .factionID = 400,
            .playerReactionBounds = ReactionBounds{ Reaction::Unfriendly, Reaction::Friendly }.Pack()
        };
        Bytebuffer buffer(nullptr, source.GetSerializedSize());
        REQUIRE(source.Serialize(&buffer));
        CHECK(buffer.writtenData == source.GetSerializedSize());

        ServerUnitFactionUpdatePacket decoded;
        REQUIRE(decoded.Deserialize(&buffer));
        CHECK(decoded.guid.GetData() == source.guid.GetData());
        CHECK(decoded.factionID == source.factionID);
        CHECK(decoded.playerReactionBounds == source.playerReactionBounds);
    }

    SECTION("reputation")
    {
        const ServerReputationUpdatePacket source{
            .factionID = 400,
            .value = -12345,
            .flags = static_cast<u16>(ReputationFlags::Visible) | static_cast<u16>(ReputationFlags::AtWar),
            .isPresent = 1
        };
        Bytebuffer buffer(nullptr, source.GetSerializedSize());
        REQUIRE(source.Serialize(&buffer));

        ServerReputationUpdatePacket decoded;
        REQUIRE(decoded.Deserialize(&buffer));
        CHECK(decoded.factionID == source.factionID);
        CHECK(decoded.value == source.value);
        CHECK(decoded.flags == source.flags);
        CHECK(decoded.isPresent == source.isPresent);
    }

    SECTION("local perception override")
    {
        const ServerFactionPerceptionOverrideUpdatePacket source{
            .factionID = 30,
            .activeFields = 0x3,
            .effectiveStandingValue = 21000,
            .effectiveReaction = static_cast<u8>(Reaction::Friendly)
        };
        Bytebuffer buffer(nullptr, source.GetSerializedSize());
        REQUIRE(source.Serialize(&buffer));

        ServerFactionPerceptionOverrideUpdatePacket decoded;
        REQUIRE(decoded.Deserialize(&buffer));
        CHECK(decoded.factionID == source.factionID);
        CHECK(decoded.activeFields == source.activeFields);
        CHECK(decoded.effectiveStandingValue == source.effectiveStandingValue);
        CHECK(decoded.effectiveReaction == source.effectiveReaction);
    }
}

TEST_CASE("Faction ClientDB export is deterministic across source row order", "[Faction][ClientDB]")
{
    Database::FactionTables firstTables = MakeFactionTables();
    Database::FactionTables secondTables = firstTables;
    std::reverse(secondTables.definitions.begin(), secondTables.definitions.end());
    std::reverse(secondTables.relations.begin(), secondTables.relations.end());
    std::reverse(secondTables.standings.begin(), secondTables.standings.end());

    const auto firstRuntime = BuildRuntimeData(firstTables, {});
    const auto secondRuntime = BuildRuntimeData(secondTables, {});
    CHECK(CalculateRuntimeContentHash(*firstRuntime) == CalculateRuntimeContentHash(*secondRuntime));
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "novus-faction-clientdb-test";
    std::error_code error;
    std::filesystem::remove_all(root, error);

    ClientDBExportResult firstResult;
    ClientDBExportResult secondResult;
    REQUIRE(ExportClientDB(firstTables, *firstRuntime, root / "first", firstResult));
    REQUIRE(ExportClientDB(secondTables, *secondRuntime, root / "second", secondResult));
    CHECK(firstResult.factionCount == 3);
    CHECK(firstResult.relationCount == 1);
    CHECK(firstResult.standingCount == 3);
    CHECK(std::filesystem::is_regular_file(root / "first/clientdb/Faction.cdb"));
    CHECK(std::filesystem::is_regular_file(root / "first/clientdb/FactionRelation.cdb"));
    CHECK(std::filesystem::is_regular_file(root / "first/clientdb/FactionStanding.cdb"));

    const auto readBytes = [](const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<u8>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    };
    CHECK(readBytes(root / "first/clientdb/Faction.cdb") == readBytes(root / "second/clientdb/Faction.cdb"));
    CHECK(readBytes(root / "first/clientdb/FactionRelation.cdb") == readBytes(root / "second/clientdb/FactionRelation.cdb"));
    CHECK(readBytes(root / "first/clientdb/FactionStanding.cdb") == readBytes(root / "second/clientdb/FactionStanding.cdb"));

    std::filesystem::remove_all(root, error);
}

TEST_CASE("Faction reaction bounds pack and unpack every combination", "[Faction][Runtime]")
{
    for (u8 minimum = 0; minimum <= static_cast<u8>(Reaction::Friendly); ++minimum)
    {
        for (u8 maximum = 0; maximum <= static_cast<u8>(Reaction::Friendly); ++maximum)
        {
            const ReactionBounds bounds{
                .minimum = static_cast<Reaction>(minimum),
                .maximum = static_cast<Reaction>(maximum)
            };
            const u8 packed = bounds.Pack();
            const ReactionBounds unpacked = ReactionBounds::Unpack(packed);

            CHECK(unpacked.minimum == bounds.minimum);
            CHECK(unpacked.maximum == bounds.maximum);
            CHECK(ReactionBounds::IsValidPacked(packed) == (minimum <= maximum));
            if (minimum <= maximum)
            {
                CHECK(unpacked.Clamp(Reaction::Hostile) == bounds.minimum);
                CHECK(unpacked.Clamp(Reaction::Friendly) == bounds.maximum);
            }
        }
    }

    CHECK_FALSE(ReactionBounds{ static_cast<Reaction>(4), static_cast<Reaction>(4) }.IsValid());
    CHECK_FALSE(ReactionBounds{ Reaction::Hostile, static_cast<Reaction>(4) }.IsValid());
}

TEST_CASE("Faction runtime compiles deterministic directional data", "[Faction][Runtime]")
{
    Database::FactionTables tables = MakeFactionTables();
    Database::CreatureTables creatures;
    creatures.templateIDToDefinition.emplace(100, Database::CreatureTemplate{ .id = 100, .factionId = 20 });
    creatures.templateIDToDefinition.emplace(101, Database::CreatureTemplate{
        .id = 101,
        .factionId = 20,
        .playerReactionMinOverride = static_cast<u16>(Reaction::Unfriendly),
        .playerReactionMaxOverride = static_cast<u16>(Reaction::Neutral)
    });

    const auto runtime = BuildRuntimeData(tables, creatures);
    FactionIndex none = INVALID_FACTION_INDEX;
    FactionIndex player = INVALID_FACTION_INDEX;
    FactionIndex reputation = INVALID_FACTION_INDEX;
    FactionIndex raceFaction = INVALID_FACTION_INDEX;

    REQUIRE(runtime->TryGetFactionIndex(0, none));
    REQUIRE(runtime->TryGetFactionIndex(10, player));
    REQUIRE(runtime->TryGetFactionIndex(20, reputation));
    REQUIRE(runtime->TryGetRaceFaction(1, raceFaction));
    CHECK(none == 0);
    CHECK(player == 1);
    CHECK(reputation == 2);
    CHECK(raceFaction == player);
    CHECK(runtime->GetFactionID(player) == 10);

    CHECK(runtime->GetRelation(none, player) == Reaction::Neutral);
    CHECK(runtime->GetRelation(player, none) == Reaction::Neutral);
    CHECK(runtime->GetRelation(player, player) == Reaction::Friendly);
    CHECK(runtime->GetRelation(player, reputation) == Reaction::Friendly);
    CHECK(runtime->GetRelation(reputation, player) == Reaction::Unfriendly);

    CHECK(runtime->GetStanding(0).reaction == Reaction::Neutral);
    CHECK(runtime->GetStanding(999).reaction == Reaction::Neutral);
    CHECK(runtime->GetStanding(1000).reaction == Reaction::Friendly);
    CHECK(runtime->GetStartingReputation(player, reputation) == 125);
    CHECK(runtime->GetStartingReputation(reputation, reputation) == 25);

    const ReactionBounds inherited = ReactionBounds::Unpack(runtime->ResolveCreatureReactionBounds(creatures.templateIDToDefinition.at(100)));
    CHECK(inherited.minimum == Reaction::Hostile);
    CHECK(inherited.maximum == Reaction::Friendly);

    const ReactionBounds overridden = ReactionBounds::Unpack(runtime->ResolveCreatureReactionBounds(creatures.templateIDToDefinition.at(101)));
    CHECK(overridden.minimum == Reaction::Unfriendly);
    CHECK(overridden.maximum == Reaction::Neutral);
}

TEST_CASE("Faction relation packing crosses word and row boundaries", "[Faction][Runtime]")
{
    Database::FactionTables tables;
    for (u16 id = 1; id <= 34; ++id)
    {
        tables.definitions.push_back(MakeFaction(id, "Faction " + std::to_string(id)));
    }

    tables.relations.push_back({
        .id = 1,
        .sourceFactionId = 1,
        .targetFactionId = 31,
        .reaction = static_cast<u16>(Reaction::Hostile)
    });
    tables.relations.push_back({
        .id = 2,
        .sourceFactionId = 1,
        .targetFactionId = 32,
        .reaction = static_cast<u16>(Reaction::Friendly)
    });
    tables.relations.push_back({
        .id = 3,
        .sourceFactionId = 2,
        .targetFactionId = 32,
        .reaction = static_cast<u16>(Reaction::Unfriendly)
    });

    const auto runtime = BuildRuntimeData(tables, {});
    CHECK(runtime->wordsPerRelationRow == 2);
    CHECK(runtime->GetRelation(1, 31) == Reaction::Hostile);
    CHECK(runtime->GetRelation(1, 32) == Reaction::Friendly);
    CHECK(runtime->GetRelation(2, 32) == Reaction::Unfriendly);
    CHECK(runtime->GetRelation(2, 31) == Reaction::Neutral);
}

TEST_CASE("Faction runtime rejects invalid content", "[Faction][Runtime]")
{
    SECTION("definition bounds")
    {
        Database::FactionTables tables;
        auto faction = MakeFaction(1, "Invalid");
        faction.defaultPlayerReactionMin = static_cast<u16>(Reaction::Friendly);
        faction.defaultPlayerReactionMax = static_cast<u16>(Reaction::Hostile);
        tables.definitions.push_back(std::move(faction));
        const auto runtime = BuildRuntimeData(tables, {});
        CHECK_FALSE(runtime);
    }

    SECTION("relations involving None")
    {
        Database::FactionTables tables;
        tables.definitions.push_back(MakeFaction(1, "Valid"));
        tables.relations.push_back({
            .id = 1,
            .sourceFactionId = 0,
            .targetFactionId = 1,
            .reaction = static_cast<u16>(Reaction::Hostile)
        });
        const auto runtime = BuildRuntimeData(tables, {});
        CHECK_FALSE(runtime);
    }

    SECTION("creature faction reference")
    {
        Database::FactionTables tables;
        tables.definitions.push_back(MakeFaction(1, "Valid"));
        Database::CreatureTables creatures;
        creatures.templateIDToDefinition.emplace(5, Database::CreatureTemplate{ .id = 5, .factionId = 2 });
        const auto runtime = BuildRuntimeData(tables, creatures);
        CHECK_FALSE(runtime);
    }
}

TEST_CASE("Faction reputation discovery is sparse and preserves mutation provenance", "[Faction][Reputation]")
{
    Database::FactionTables tables = MakeFactionTables();
    tables.definitions.front().flags =
        static_cast<u16>(DefinitionFlags::AllowsReputation) |
        static_cast<u16>(DefinitionFlags::DiscoverOnInteract) |
        static_cast<u16>(DefinitionFlags::DiscoverOnTarget) |
        static_cast<u16>(DefinitionFlags::CanSetAtWar);
    tables.definitions.push_back(MakeFaction(30, "Hidden", Reaction::Neutral,
        static_cast<u16>(DefinitionFlags::AllowsReputation) |
            static_cast<u16>(DefinitionFlags::DiscoverOnInteract) |
            static_cast<u16>(DefinitionFlags::DiscoverOnTarget) |
            static_cast<u16>(DefinitionFlags::HiddenUntilEarned)));
    tables.definitions.push_back(MakeFaction(40, "No Reputation"));

    const auto runtime = BuildRuntimeData(tables, {});
    FactionIndex playerIndex = INVALID_FACTION_INDEX;
    FactionIndex reputationIndex = INVALID_FACTION_INDEX;
    FactionIndex hiddenIndex = INVALID_FACTION_INDEX;
    REQUIRE(runtime->TryGetFactionIndex(10, playerIndex));
    REQUIRE(runtime->TryGetFactionIndex(20, reputationIndex));
    REQUIRE(runtime->TryGetFactionIndex(30, hiddenIndex));

    ECS::Components::UnitFaction playerFaction{
        .assignedFaction = playerIndex,
        .effectiveFaction = playerIndex
    };
    ECS::Components::CharacterReputation reputation;

    CHECK(ECS::Util::Faction::DiscoverReputation(*runtime, playerFaction, reputation, 20,
        DiscoverySource::Target));
    const auto* discovered = ECS::Util::Faction::FindReputation(reputation, reputationIndex);
    REQUIRE(discovered);
    CHECK(discovered->value == 125);
    CHECK((discovered->flags & static_cast<u16>(ReputationFlags::Visible)) != 0);
    CHECK_FALSE(ECS::Util::Faction::DiscoverReputation(*runtime, playerFaction, reputation, 20,
        DiscoverySource::Target));
    REQUIRE(reputation.dirtyByFaction.contains(reputationIndex));
    CHECK(reputation.dirtyByFaction.at(reputationIndex).type == ReputationSourceType::DiscoveryTarget);

    CHECK_FALSE(ECS::Util::Faction::DiscoverReputation(*runtime, playerFaction, reputation, 30,
        DiscoverySource::Interaction));
    CHECK_FALSE(ECS::Util::Faction::FindReputation(reputation, hiddenIndex));
    CHECK(ECS::Util::Faction::ModifyReputation(*runtime, playerFaction, reputation, 30, -25,
        { .type = ReputationSourceType::Quest, .contentID = 9001 }));
    const auto* hidden = ECS::Util::Faction::FindReputation(reputation, hiddenIndex);
    REQUIRE(hidden);
    CHECK(hidden->value == -25);
    CHECK((hidden->flags & static_cast<u16>(ReputationFlags::Visible)) != 0);
    CHECK(reputation.dirtyByFaction.at(hiddenIndex).type == ReputationSourceType::Quest);
    CHECK(reputation.dirtyByFaction.at(hiddenIndex).contentID == 9001);

    CHECK_FALSE(ECS::Util::Faction::DiscoverReputation(*runtime, playerFaction, reputation, 40,
        DiscoverySource::Target));
    CHECK(reputation.byFaction.size() == 2);
}

TEST_CASE("Faction reputation remains sparse at representative character sizes and coalesces network state",
    "[Faction][Reputation][Network]")
{
    Database::FactionTables tables;
    tables.definitions.push_back(MakeFaction(500, "Player"));
    for (u16 id = 1; id <= 400; ++id)
    {
        tables.definitions.push_back(MakeFaction(id, "Reputation " + std::to_string(id), Reaction::Neutral, static_cast<u16>(DefinitionFlags::AllowsReputation)));
    }
    tables.standings.push_back({
        .id = 1,
        .name = "Hostile",
        .minimumValue = -1000,
        .reaction = static_cast<u16>(Reaction::Hostile),
        .sortOrder = 1
    });
    tables.standings.push_back({
        .id = 2,
        .name = "Neutral",
        .minimumValue = 0,
        .reaction = static_cast<u16>(Reaction::Neutral),
        .sortOrder = 2
    });
    tables.standings.push_back({
        .id = 3,
        .name = "Friendly",
        .minimumValue = 1000,
        .reaction = static_cast<u16>(Reaction::Friendly),
        .sortOrder = 3
    });
    tables.standings.push_back({
        .id = 4,
        .name = "Honored",
        .minimumValue = 2000,
        .reaction = static_cast<u16>(Reaction::Friendly),
        .sortOrder = 4
    });

    const auto runtime = BuildRuntimeData(tables, {});
    FactionIndex playerIndex = INVALID_FACTION_INDEX;
    FactionIndex faction30 = INVALID_FACTION_INDEX;
    FactionIndex faction400 = INVALID_FACTION_INDEX;
    REQUIRE(runtime->TryGetFactionIndex(500, playerIndex));
    REQUIRE(runtime->TryGetFactionIndex(30, faction30));
    REQUIRE(runtime->TryGetFactionIndex(400, faction400));

    ECS::Components::UnitFaction playerFaction{
        .assignedFaction = playerIndex,
        .effectiveFaction = playerIndex
    };
    ECS::Components::CharacterReputation reputation;
    reputation.byFaction.reserve(400);
    for (u16 id = 1; id <= 400; ++id)
    {
        REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, id, id,
            { .type = ReputationSourceType::Administrative }));
    }
    CHECK(reputation.byFaction.size() == 400);
    REQUIRE(ECS::Util::Faction::FindReputation(reputation, faction30));
    REQUIRE(ECS::Util::Faction::FindReputation(reputation, faction400));
    CHECK(ECS::Util::Faction::FindReputation(reputation, faction30)->value == 30);
    CHECK(ECS::Util::Faction::FindReputation(reputation, faction400)->value == 400);

    reputation.pendingNetworkByFaction.clear();
    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 30, 31,
        { .type = ReputationSourceType::Quest, .contentID = 1 }));
    REQUIRE(reputation.pendingNetworkByFaction.contains(faction30));
    CHECK(reputation.pendingNetworkByFaction.at(faction30) ==
          ECS::Components::ReputationNetworkUpdateFlags::None);

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 30, 32,
        { .type = ReputationSourceType::Quest, .contentID = 2 }));
    CHECK(reputation.pendingNetworkByFaction.size() == 1);
    CHECK(reputation.pendingNetworkByFaction.at(faction30) ==
          ECS::Components::ReputationNetworkUpdateFlags::None);

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 30, 1000,
        { .type = ReputationSourceType::Quest, .contentID = 3 }));
    CHECK(reputation.pendingNetworkByFaction.at(faction30) ==
          ECS::Components::ReputationNetworkUpdateFlags::Immediate);

    reputation.pendingNetworkByFaction.clear();
    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 30, 2000,
        { .type = ReputationSourceType::Quest, .contentID = 4 }));
    CHECK(reputation.pendingNetworkByFaction.at(faction30) ==
          ECS::Components::ReputationNetworkUpdateFlags::Immediate);

    reputation.pendingNetworkByFaction.clear();
    REQUIRE(ECS::Util::Faction::SetReputationFlags(*runtime, reputation, 30,
        static_cast<u16>(ReputationFlags::Visible) | static_cast<u16>(ReputationFlags::AtWar),
        { .type = ReputationSourceType::Administrative }));
    CHECK(reputation.pendingNetworkByFaction.at(faction30) ==
          ECS::Components::ReputationNetworkUpdateFlags::Immediate);
}

TEST_CASE("Faction reputation removal is lock-aware and coalesces persistence tombstones",
    "[Faction][Reputation][Persistence][Network]")
{
    Database::FactionTables tables = MakeFactionTables();
    tables.definitions.front().flags = static_cast<u16>(DefinitionFlags::AllowsReputation);
    const auto runtime = BuildRuntimeData(tables, {});

    FactionIndex playerIndex = INVALID_FACTION_INDEX;
    FactionIndex reputationIndex = INVALID_FACTION_INDEX;
    REQUIRE(runtime->TryGetFactionIndex(10, playerIndex));
    REQUIRE(runtime->TryGetFactionIndex(20, reputationIndex));
    ECS::Components::UnitFaction playerFaction{
        .assignedFaction = playerIndex,
        .effectiveFaction = playerIndex
    };
    ECS::Components::CharacterReputation reputation;

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 20, 250,
        { .type = ReputationSourceType::Administrative }));
    REQUIRE(ECS::Util::Faction::SetReputationFlags(*runtime, reputation, 20,
        static_cast<u16>(ReputationFlags::Visible) | static_cast<u16>(ReputationFlags::Locked),
        { .type = ReputationSourceType::Administrative }));
    CHECK_FALSE(ECS::Util::Faction::ModifyReputation(*runtime, playerFaction, reputation, 20, 10,
        { .type = ReputationSourceType::Quest }));
    CHECK_FALSE(ECS::Util::Faction::RemoveReputation(*runtime, reputation, 20,
        { .type = ReputationSourceType::Script }));

    REQUIRE(ECS::Util::Faction::RemoveReputation(*runtime, reputation, 20,
        { .type = ReputationSourceType::Administrative }));
    CHECK_FALSE(ECS::Util::Faction::FindReputation(reputation, reputationIndex));
    CHECK_FALSE(reputation.dirtyByFaction.contains(reputationIndex));
    REQUIRE(reputation.removedDirtyByFaction.contains(reputationIndex));
    CHECK(reputation.pendingNetworkByFaction.at(reputationIndex) ==
          ECS::Components::ReputationNetworkUpdateFlags::Immediate);

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 20, 300,
        { .type = ReputationSourceType::Administrative }));
    CHECK(reputation.dirtyByFaction.contains(reputationIndex));
    CHECK_FALSE(reputation.removedDirtyByFaction.contains(reputationIndex));
    CHECK(ECS::Util::Faction::FindReputation(reputation, reputationIndex)->value == 300);
}

TEST_CASE("Creature faction policies expose stable content values and bounded assistance",
    "[Faction][CreaturePolicy]")
{
    CHECK(IsValidCreatureAggressionPolicy(0));
    CHECK(IsValidCreatureAggressionPolicy(2));
    CHECK_FALSE(IsValidCreatureAggressionPolicy(3));
    CHECK(IsValidCreatureAssistancePolicy(0));
    CHECK(IsValidCreatureAssistancePolicy(2));
    CHECK_FALSE(IsValidCreatureAssistancePolicy(3));
    CHECK(CreatureAggressionPolicyName(CreatureAggressionPolicy::Aggressive) == "Aggressive");
    CHECK(CreatureAssistancePolicyName(CreatureAssistancePolicy::SameFaction) == "SameFaction");
    CHECK(MAX_CREATURE_ASSISTANCE_RANGE == 40.0f);
}

TEST_CASE("Faction reactions remain directional and apply player-facing bounds", "[Faction][Reaction]")
{
    Database::FactionTables tables = MakeFactionTables();
    tables.definitions[1].defaultReactionToOthers = static_cast<u16>(Reaction::Neutral);
    tables.definitions.front().flags = static_cast<u16>(DefinitionFlags::AllowsReputation) | static_cast<u16>(DefinitionFlags::CanSetAtWar);
    tables.definitions.push_back(MakeFaction(30, "Friendly to Player"));
    tables.definitions.push_back(MakeFaction(40, "Hostile to Player"));
    tables.definitions.push_back(MakeFaction(50, "Neutral to Player"));
    tables.definitions.push_back(MakeFaction(60, "Player Regards Friendly"));
    tables.relations.push_back({
        .id = 2,
        .sourceFactionId = 20,
        .targetFactionId = 10,
        .reaction = static_cast<u16>(Reaction::Hostile)
    });
    tables.relations.push_back({
        .id = 3,
        .sourceFactionId = 30,
        .targetFactionId = 10,
        .reaction = static_cast<u16>(Reaction::Friendly)
    });
    tables.relations.push_back({
        .id = 4,
        .sourceFactionId = 40,
        .targetFactionId = 10,
        .reaction = static_cast<u16>(Reaction::Hostile)
    });
    tables.relations.push_back({
        .id = 5,
        .sourceFactionId = 10,
        .targetFactionId = 60,
        .reaction = static_cast<u16>(Reaction::Friendly)
    });
    const auto runtime = BuildRuntimeData(tables, {});

    FactionIndex playerIndex = INVALID_FACTION_INDEX;
    FactionIndex townIndex = INVALID_FACTION_INDEX;
    REQUIRE(runtime->TryGetFactionIndex(10, playerIndex));
    REQUIRE(runtime->TryGetFactionIndex(20, townIndex));

    ECS::Components::UnitFaction playerFaction{
        .assignedFaction = playerIndex,
        .effectiveFaction = playerIndex
    };
    ECS::Components::UnitFaction townFaction{
        .assignedFaction = townIndex,
        .effectiveFaction = townIndex,
        .assignedPlayerReactionBounds = ReactionBounds{ Reaction::Neutral, Reaction::Friendly }.Pack(),
        .effectivePlayerReactionBounds = ReactionBounds{ Reaction::Neutral, Reaction::Friendly }.Pack()
    };
    ECS::Components::CharacterReputation reputation;

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 20, -1000,
        { .type = ReputationSourceType::Quest, .contentID = 1 }));
    CHECK(ECS::Util::Faction::GetReaction(*runtime, playerFaction, &reputation, townFaction, nullptr) == Reaction::Hostile);
    CHECK(ECS::Util::Faction::GetReaction(*runtime, townFaction, nullptr, playerFaction, &reputation) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::GetPresentationReaction(*runtime, playerFaction, reputation, townFaction) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::CanAttack(*runtime, playerFaction, &reputation, townFaction, nullptr));
    CHECK_FALSE(ECS::Util::Faction::CanInteract(*runtime, playerFaction, reputation, townFaction));

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 20, 1000,
        { .type = ReputationSourceType::Quest, .contentID = 2 }));
    CHECK(ECS::Util::Faction::GetPresentationReaction(*runtime, playerFaction, reputation, townFaction) == Reaction::Friendly);
    CHECK_FALSE(ECS::Util::Faction::CanAttack(*runtime, playerFaction, &reputation, townFaction, nullptr));
    CHECK(ECS::Util::Faction::CanAssist(*runtime, playerFaction, &reputation, townFaction, nullptr));
    CHECK(ECS::Util::Faction::CanInteract(*runtime, playerFaction, reputation, townFaction));

    REQUIRE(ECS::Util::Faction::SetReputation(*runtime, playerFaction, reputation, 20, 0,
        { .type = ReputationSourceType::Administrative }));
    REQUIRE(ECS::Util::Faction::SetReputationFlags(*runtime, reputation, 20,
        static_cast<u16>(ReputationFlags::Visible) | static_cast<u16>(ReputationFlags::AtWar),
        { .type = ReputationSourceType::Administrative }));
    CHECK(ECS::Util::Faction::GetReaction(*runtime, playerFaction, &reputation, townFaction, nullptr) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::CanAttack(*runtime, playerFaction, &reputation, townFaction, nullptr));

    REQUIRE(ECS::Util::Faction::SetReputationFlags(*runtime, reputation, 20,
        static_cast<u16>(ReputationFlags::Visible),
        { .type = ReputationSourceType::Administrative }));
    CHECK(ECS::Util::Faction::GetReaction(*runtime, playerFaction, &reputation, townFaction, nullptr) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::CanAttack(*runtime, playerFaction, &reputation, townFaction, nullptr));
    CHECK_FALSE(ECS::Util::Faction::IsHostileTo(*runtime, playerFaction, &reputation, townFaction, nullptr));

    ECS::Components::UnitFaction hostileTownFaction = townFaction;
    hostileTownFaction.assignedPlayerReactionBounds =
        ReactionBounds{ Reaction::Hostile, Reaction::Friendly }.Pack();
    hostileTownFaction.effectivePlayerReactionBounds = hostileTownFaction.assignedPlayerReactionBounds;
    CHECK(ECS::Util::Faction::GetReaction(*runtime, playerFaction, &reputation, hostileTownFaction, nullptr) ==
          Reaction::Neutral);
    CHECK(ECS::Util::Faction::GetReaction(*runtime, hostileTownFaction, nullptr, playerFaction, &reputation) ==
          Reaction::Neutral);
    CHECK(ECS::Util::Faction::GetPresentationReaction(*runtime, playerFaction, reputation, hostileTownFaction) ==
          Reaction::Neutral);
    CHECK(ECS::Util::Faction::CanAttack(*runtime, playerFaction, &reputation, hostileTownFaction, nullptr));
    CHECK_FALSE(ECS::Util::Faction::IsHostileTo(
        *runtime, playerFaction, &reputation, hostileTownFaction, nullptr));

    FactionIndex friendlyToPlayerIndex = INVALID_FACTION_INDEX;
    FactionIndex hostileToPlayerIndex = INVALID_FACTION_INDEX;
    FactionIndex neutralToPlayerIndex = INVALID_FACTION_INDEX;
    FactionIndex playerRegardsFriendlyIndex = INVALID_FACTION_INDEX;
    REQUIRE(runtime->TryGetFactionIndex(30, friendlyToPlayerIndex));
    REQUIRE(runtime->TryGetFactionIndex(40, hostileToPlayerIndex));
    REQUIRE(runtime->TryGetFactionIndex(50, neutralToPlayerIndex));
    REQUIRE(runtime->TryGetFactionIndex(60, playerRegardsFriendlyIndex));

    const auto MakeUnitFaction = [](FactionIndex factionIndex)
    {
        return ECS::Components::UnitFaction{
            .assignedFaction = factionIndex,
            .effectiveFaction = factionIndex,
            .assignedPlayerReactionBounds = ReactionBounds{ Reaction::Hostile, Reaction::Friendly }.Pack(),
            .effectivePlayerReactionBounds = ReactionBounds{ Reaction::Hostile, Reaction::Friendly }.Pack()
        };
    };

    const ECS::Components::UnitFaction friendlyToPlayerFaction = MakeUnitFaction(friendlyToPlayerIndex);
    CHECK(ECS::Util::Faction::GetReaction(
              *runtime, playerFaction, &reputation, friendlyToPlayerFaction, nullptr) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::GetReaction(
              *runtime, friendlyToPlayerFaction, nullptr, playerFaction, &reputation) == Reaction::Friendly);
    CHECK(ECS::Util::Faction::GetPresentationReaction(
              *runtime, playerFaction, reputation, friendlyToPlayerFaction) == Reaction::Friendly);
    CHECK_FALSE(ECS::Util::Faction::CanAttack(
        *runtime, playerFaction, &reputation, friendlyToPlayerFaction, nullptr));

    const ECS::Components::UnitFaction hostileToPlayerFaction = MakeUnitFaction(hostileToPlayerIndex);
    CHECK(ECS::Util::Faction::GetPresentationReaction(
              *runtime, playerFaction, reputation, hostileToPlayerFaction) == Reaction::Hostile);
    CHECK(ECS::Util::Faction::CanAttack(
        *runtime, playerFaction, &reputation, hostileToPlayerFaction, nullptr));

    const ECS::Components::UnitFaction neutralToPlayerFaction = MakeUnitFaction(neutralToPlayerIndex);
    CHECK(ECS::Util::Faction::GetPresentationReaction(
              *runtime, playerFaction, reputation, neutralToPlayerFaction) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::CanAttack(
        *runtime, playerFaction, &reputation, neutralToPlayerFaction, nullptr));

    const ECS::Components::UnitFaction playerRegardsFriendlyFaction = MakeUnitFaction(playerRegardsFriendlyIndex);
    CHECK(ECS::Util::Faction::GetReaction(
              *runtime, playerFaction, &reputation, playerRegardsFriendlyFaction, nullptr) == Reaction::Friendly);
    CHECK(ECS::Util::Faction::GetReaction(
              *runtime, playerRegardsFriendlyFaction, nullptr, playerFaction, &reputation) == Reaction::Neutral);
    CHECK(ECS::Util::Faction::GetPresentationReaction(
              *runtime, playerFaction, reputation, playerRegardsFriendlyFaction) == Reaction::Neutral);
    CHECK_FALSE(ECS::Util::Faction::CanAttack(
        *runtime, playerFaction, &reputation, playerRegardsFriendlyFaction, nullptr));

    ECS::Components::UnitFaction boundedTownFaction = townFaction;
    boundedTownFaction.assignedPlayerReactionBounds =
        ReactionBounds{ Reaction::Hostile, Reaction::Neutral }.Pack();
    boundedTownFaction.effectivePlayerReactionBounds = boundedTownFaction.assignedPlayerReactionBounds;
    ECS::Components::FactionModifiers playerModifiers;
    playerModifiers.perceptionByFaction.emplace(townIndex, ECS::Components::ResolvedFactionPerception{
        .effectiveStandingValue = 1000,
        .activeFields = static_cast<u8>(PerceptionOverrideFields::Standing)
    });
    CHECK(ECS::Util::Faction::GetPresentationReaction(*runtime, playerFaction, reputation, boundedTownFaction, &playerModifiers) == Reaction::Neutral);

    playerModifiers.perceptionByFaction.at(townIndex) = ECS::Components::ResolvedFactionPerception{
        .effectiveReaction = Reaction::Friendly,
        .activeFields = static_cast<u8>(PerceptionOverrideFields::Reaction)
    };
    CHECK(ECS::Util::Faction::GetPresentationReaction(
              *runtime, playerFaction, reputation, boundedTownFaction, &playerModifiers) == Reaction::Friendly);

    ECS::Components::UnitFaction friendlyOverrideTownFaction = townFaction;
    friendlyOverrideTownFaction.effectiveUnitReactionOverride = static_cast<u8>(Reaction::Friendly);
    REQUIRE(ECS::Util::Faction::SetReputationFlags(*runtime, reputation, 20,
        static_cast<u16>(ReputationFlags::Visible) | static_cast<u16>(ReputationFlags::AtWar),
        { .type = ReputationSourceType::Administrative }));
    CHECK(ECS::Util::Faction::CanAttack(
        *runtime, playerFaction, &reputation, friendlyOverrideTownFaction, nullptr));
}
