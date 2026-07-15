#include <Server-Common/Database/GeneratedRuntime.h>
#include <Server-Common/Database/DatabaseService.h>
#include <Server-Common/Database/MigrationRunner.h>
#include <Server-Common/Database/ReliableExecution.h>
#include <Server-Common/Database/SchemaTraits.h>
#include <Server-Common/Database/Detail/CharacterUtils.h>
#include <Server-Common/Database/Detail/CreatureUtils.h>
#include <Server-Common/Database/Detail/FactionUtils.h>
#include <Server-Common/Database/Detail/ItemUtils.h>
#include <Server-Common/Database/Detail/MapUtils.h>
#include <Server-Common/Database/Detail/SpellUtils.h>

#include <MetaGen/Postgres/Auth/Tables/Accounts.h>
#include <MetaGen/Postgres/Auth/Schema.h>

#include <catch2/catch2.hpp>

#include <atomic>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

namespace
{
    struct TestWorldCacheSnapshot
    {
    public:
        Database::MapTables maps;
        Database::SpellTables spells;
        Database::ItemTables items;
        Database::CreatureTables creatures;
        Database::FactionTables factions;
    };

    struct TestIntRecord
    {
    public:
        int value = 0;
    };

    struct ExactlyOneTestDescriptor
    {
    public:
        using Parameters = std::tuple<>;
        using Record = TestIntRecord;

        static Record Decode(const pqxx::row& row)
        {
            return { row[0].as<int>() };
        }

    public:
        static constexpr std::string_view PREPARED_NAME = "test_exactly_one";
        static constexpr auto CARDINALITY = MetaGen::Postgres::QueryCardinality::ExactlyOne;
    };

    struct ExactlyOneEmptyTestDescriptor : ExactlyOneTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_exactly_one_empty";
    };

    struct ExactlyOneMultipleTestDescriptor : ExactlyOneTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_exactly_one_multiple";
    };

    struct ZeroOrOneTestDescriptor : ExactlyOneTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_zero_or_one";
        static constexpr auto CARDINALITY = MetaGen::Postgres::QueryCardinality::ZeroOrOne;
    };

    struct ZeroOrOneEmptyTestDescriptor : ZeroOrOneTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_zero_or_one_empty";
    };

    struct ZeroOrOneMultipleTestDescriptor : ZeroOrOneEmptyTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_zero_or_one_multiple";
    };

    struct ZeroOrMoreTestDescriptor : ExactlyOneTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_zero_or_more";
        static constexpr auto CARDINALITY = MetaGen::Postgres::QueryCardinality::ZeroOrMore;
    };

    struct MutationTestDescriptor
    {
    public:
        static constexpr std::string_view PREPARED_NAME = "test_mutation";

        using Parameters = std::tuple<int>;
    };

    void EnsureTestLogger()
    {
        static const bool initialized = []
        {
            quill::Backend::start();
            auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("server_tests_console");
            quill::Frontend::create_or_get_logger("root", std::move(sink),
                "%(time:<16) LOG_%(log_level:<11) %(message)", "%H:%M:%S.%Qms",
                quill::Timezone::LocalTime, quill::ClockSourceType::System);
            return true;
        }();
        (void)initialized;
    }

    [[maybe_unused]] const bool TestLoggerReady = []
    {
        EnsureTestLogger();
        return true;
    }();

    class TemporaryDatabase
    {
    public:
        TemporaryDatabase()
            : maintenance(ConnectionString())
        {
            static std::atomic_uint64_t serial = 0;
            const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            name = "novus_server_test_" + std::to_string(timestamp) + "_" + std::to_string(serial++);
            pqxx::nontransaction transaction(maintenance);
            transaction.exec("CREATE DATABASE " + transaction.quote_name(name));
        }

        ~TemporaryDatabase()
        {
            try
            {
                pqxx::nontransaction transaction(maintenance);
                transaction.exec("DROP DATABASE IF EXISTS " + transaction.quote_name(name) + " WITH (FORCE)");
            }
            catch (...)
            {
            }
        }

        std::unique_ptr<pqxx::connection> Connect() const
        {
            return std::make_unique<pqxx::connection>(ConnectionString() + " dbname=" + name);
        }

        const std::string& Name() const
        {
            return name;
        }

        void Drop()
        {
            pqxx::nontransaction transaction(maintenance);
            transaction.exec("DROP DATABASE IF EXISTS " + transaction.quote_name(name) + " WITH (FORCE)");
        }

    private:
        static std::string ConnectionString()
        {
            const char* value = std::getenv("NOVUS_TEST_POSTGRES_CONNECTION");
            if (!value)
                throw std::runtime_error("NOVUS_TEST_POSTGRES_CONNECTION must contain a libpq keyword connection string to a maintenance database");
            return value;
        }

        pqxx::connection maintenance;
        std::string name;
    };

    std::optional<Database::DBEntry> ControllerEntry(const std::string& database, bool createIfMissing = false)
    {
        const char* host = std::getenv("NOVUS_TEST_POSTGRES_HOST");
        const char* user = std::getenv("NOVUS_TEST_POSTGRES_USER");
        const char* password = std::getenv("NOVUS_TEST_POSTGRES_PASSWORD");
        if (!host || !user || !password)
            return std::nullopt;
        const char* portValue = std::getenv("NOVUS_TEST_POSTGRES_PORT");
        const u16 port = static_cast<u16>(portValue ? std::stoul(portValue) : 5432);
        return Database::DBEntry(host, port, database, user, password, Database::MigrationMode::Migrate, createIfMissing);
    }
}

TEST_CASE("PostgreSQL baseline and second startup", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();

    Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate);
    {
        pqxx::read_transaction transaction(*connection);
        CHECK(transaction.query_value<std::size_t>("SELECT count(*) FROM metagen.schema_migrations WHERE bundle = 'auth'") == 1);
    }

    Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate);
    pqxx::read_transaction transaction(*connection);
    CHECK(transaction.query_value<std::size_t>("SELECT count(*) FROM metagen.schema_migrations WHERE bundle = 'auth'") == 1);
}

TEST_CASE("PostgreSQL Validate mode rolls back an empty pending baseline", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();

    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Validate));
    pqxx::read_transaction transaction(*connection);
    CHECK(transaction.query_value<bool>("SELECT to_regclass('metagen.schema_migrations') IS NULL"));
    CHECK(transaction.query_value<bool>("SELECT to_regclass('public.accounts') IS NULL"));
}

TEST_CASE("PostgreSQL Validate mode does not repair missing or mismatched bundle state",
    "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();
    Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate);
    {
        pqxx::work transaction(*connection);
        transaction.exec("DELETE FROM metagen.bundle_state WHERE bundle = 'auth'");
        transaction.commit();
    }
    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Validate));
    {
        pqxx::read_transaction transaction(*connection);
        CHECK(transaction.query_value<std::size_t>("SELECT count(*) FROM metagen.bundle_state WHERE bundle = 'auth'") == 0);
    }
    {
        pqxx::work transaction(*connection);
        transaction.exec("INSERT INTO metagen.bundle_state (bundle, manifest_hash) VALUES ('auth', 'wrong')");
        transaction.commit();
    }
    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Validate));
    pqxx::read_transaction verification(*connection);
    CHECK(verification.query_value<std::string>("SELECT manifest_hash FROM metagen.bundle_state WHERE bundle = 'auth'") == "wrong");
}

TEST_CASE("PostgreSQL migration failure rolls back DDL and ledger", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();

    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate,
        [](pqxx::work&, std::size_t)
    {
        throw std::runtime_error("injected migration failure");
    }));

    pqxx::read_transaction transaction(*connection);
    CHECK(transaction.query_value<bool>("SELECT to_regclass('public.accounts') IS NULL"));
    CHECK(transaction.query_value<bool>("SELECT to_regclass('metagen.schema_migrations') IS NULL"));
    CHECK(transaction.query_value<bool>("SELECT to_regclass('metagen.bundle_state') IS NULL"));
}

TEST_CASE("PostgreSQL refuses an existing owned table without a ledger", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();
    {
        pqxx::work transaction(*connection);
        transaction.exec("CREATE TABLE public.accounts (id bigint PRIMARY KEY)");
        transaction.commit();
    }

    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate));
    pqxx::read_transaction transaction(*connection);
    CHECK(transaction.query_value<bool>("SELECT to_regclass('metagen.schema_migrations') IS NULL"));
}

TEST_CASE("PostgreSQL rejects changed applied migration content", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();
    Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate);
    {
        pqxx::work transaction(*connection);
        transaction.exec("UPDATE metagen.schema_migrations SET content_hash = 'changed' WHERE bundle = 'auth'");
        transaction.commit();
    }
    CHECK_THROWS(Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate));
}

TEST_CASE("Concurrent PostgreSQL startup serializes one baseline", "[.integration][Database][Migration]")
{
    TemporaryDatabase database;
    auto first = database.Connect();
    auto second = database.Connect();
    std::exception_ptr firstError;
    std::exception_ptr secondError;

    std::thread firstThread([&]
    {
        try
        {
            Database::MigrationRunner::Run(Database::DBType::Auth, *first, Database::MigrationMode::Migrate);
        }
        catch (...)
        {
            firstError = std::current_exception();
        }
    });
    std::thread secondThread([&]
    {
        try
        {
            Database::MigrationRunner::Run(Database::DBType::Auth, *second, Database::MigrationMode::Migrate);
        }
        catch (...)
        {
            secondError = std::current_exception();
        }
    });
    firstThread.join();
    secondThread.join();

    CHECK_FALSE(firstError);
    CHECK_FALSE(secondError);
    pqxx::read_transaction transaction(*first);
    CHECK(transaction.query_value<std::size_t>("SELECT count(*) FROM metagen.schema_migrations WHERE bundle = 'auth'") == 1);
}

TEST_CASE("Generated bytea insert and decode round trip", "[.integration][Database][Generated]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();
    Database::MigrationRunner::Run(Database::DBType::Auth, *connection, Database::MigrationMode::Migrate);
    Database::Generated::PrepareStatements<MetaGen::Postgres::Auth::AuthSchema>(*connection);

    Bytebuffer bytes(nullptr, sizeof(u32));
    bytes.SetOwnership(true);
    REQUIRE(bytes.PutU32(0xC0DEF00D));

    pqxx::work transaction(*connection);
    auto inserted = Database::Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::Insert>(transaction,
        u64{ 0 }, std::string{ "integration" }, std::string{ "integration@example.invalid" }, u64{ 1 }, u64{ 0 },
        std::optional<Bytebuffer>{ std::move(bytes) });
    auto loaded = Database::Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::ByPrimaryKey>(transaction, inserted.id);
    REQUIRE(loaded);
    REQUIRE(loaded->blob);
    CHECK(loaded->blob->writtenData == sizeof(u32));
    u32 decoded = 0;
    REQUIRE(loaded->blob->GetU32(decoded));
    CHECK(decoded == 0xC0DEF00D);

    auto insertedNull = Database::Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::Insert>(transaction,
        u64{ 0 }, std::string{ "integration-null" }, std::string{ "integration-null@example.invalid" }, u64{ 1 }, u64{ 0 },
        std::optional<Bytebuffer>{});
    auto loadedNull = Database::Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::ByPrimaryKey>(transaction, insertedNull.id);
    REQUIRE(loadedNull);
    CHECK_FALSE(loadedNull->blob);
    transaction.commit();
}

TEST_CASE("Database service owns connection and transaction details", "[.integration][Database][Service]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DatabaseService integration cases");
    }

    Database::DatabaseService service;
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::Auth, *entry));

    std::array<unsigned char, 4> blob = { 0xCA, 0xFE, 0xBA, 0xBE };
    auto create = service.CreateAccount("service-account", "service@example.invalid", 42, blob.data(), static_cast<u32>(blob.size()));
    REQUIRE(create);
    CHECK(create.Value().name == "service-account");

    auto read = service.GetAccountByName("service-account");
    REQUIRE(read);
    REQUIRE(read.Value());
    CHECK(read.Value()->id == create.Value().id);
    REQUIRE(read.Value()->blob);
    CHECK(read.Value()->blob->writtenData == blob.size());
}

TEST_CASE("Faction content and character reputation round trip through PostgreSQL",
    "[.integration][Database][Faction]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DatabaseService integration cases");

    Database::DatabaseService service;
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::World, *entry));
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::Character, *entry));

    auto connection = database.Connect();
    {
        pqxx::work seed(*connection);
        seed.exec(R"sql(INSERT INTO public.factions
            (id, name, flags, default_reaction_to_others, default_player_reaction_min,
             default_player_reaction_max, default_reputation_value)
            VALUES (1, 'Players', 1, 2, 0, 3, 0), (2, 'Town', 1, 2, 1, 3, 25))sql");
        seed.exec("INSERT INTO public.faction_relations (source_faction_id, target_faction_id, reaction) VALUES (1, 2, 3)");
        seed.exec("INSERT INTO public.faction_standings (id, name, minimum_value, reaction, sort_order) "
                  "VALUES (1, 'Hostile', -1000, 0, 1), (2, 'Neutral', 0, 2, 2)");
        seed.exec("INSERT INTO public.faction_starting_reputations (player_faction_id, target_faction_id, value) VALUES (1, 2, 100)");
        seed.exec("INSERT INTO public.unit_race_factions (race_id, faction_id) VALUES (7, 1)");
        seed.commit();
    }

    auto world = service.LoadWorldCache();
    REQUIRE(world);
    REQUIRE(world.Value().factions.definitions.size() == 2);
    REQUIRE(world.Value().factions.relations.size() == 1);
    REQUIRE(world.Value().factions.standings.size() == 2);
    REQUIRE(world.Value().factions.startingReputations.size() == 1);
    REQUIRE(world.Value().factions.unitRaceFactions.size() == 1);
    CHECK(world.Value().factions.relations.front().sourceFactionId == 1);
    CHECK(world.Value().factions.relations.front().targetFactionId == 2);
    CHECK(world.Value().factions.standings.front().minimumValue == -1000);
    CHECK(world.Value().factions.unitRaceFactions.front().raceId == 7);

    auto character = service.CreateCharacter(1, "FactionTest", 7, 1);
    REQUIRE(character);
    CHECK(character.Value().factionId == 1);

    auto factionUpdate = service.SetCharacterFaction(character.Value().id, 2);
    REQUIRE(factionUpdate);
    CHECK(factionUpdate.Value() == Database::UpdateOutcome::Updated);

    const std::array reputationUpdates{
        Database::CharacterReputationUpdate{ .factionID = 2, .value = 50, .flags = 1 },
        Database::CharacterReputationUpdate{ .factionID = 2, .value = 125, .flags = 5 }
    };
    auto reputationBatch = service.SetCharacterReputations(character.Value().id, reputationUpdates);
    REQUIRE(reputationBatch);
    CHECK(reputationBatch.Value() == reputationUpdates.size());

    auto initialization = service.GetCharacterInitialization(character.Value().id);
    REQUIRE(initialization);
    REQUIRE(initialization.Value().character);
    CHECK(initialization.Value().character->factionId == 2);
    REQUIRE(initialization.Value().reputations.size() == 1);
    CHECK(initialization.Value().reputations.front().factionId == 2);
    CHECK(initialization.Value().reputations.front().value == 125);
    CHECK(initialization.Value().reputations.front().flags == 5);

    const std::array reputationChanges{
        Database::CharacterReputationChange{ .factionID = 2, .value = 200, .flags = 1 },
        Database::CharacterReputationChange{ .factionID = 2, .remove = true }
    };
    auto appliedChanges = service.ApplyCharacterReputationChanges(character.Value().id, reputationChanges);
    REQUIRE(appliedChanges);
    CHECK(appliedChanges.Value() == reputationChanges.size());
    auto reputationsAfterDelete = service.GetCharacterReputations(character.Value().id);
    REQUIRE(reputationsAfterDelete);
    CHECK(reputationsAfterDelete.Value().empty());

    auto deleteReputation = service.DeleteCharacterReputation(character.Value().id, 2);
    REQUIRE(deleteReputation);
    CHECK(deleteReputation.Value() == Database::DeleteOutcome::AlreadyAbsent);

    REQUIRE(service.SetCharacterReputation(character.Value().id, 2, -25, 1));
    auto deleteCharacter = service.DeleteCharacter(character.Value().id);
    REQUIRE(deleteCharacter);
    CHECK(deleteCharacter.Value() == Database::DeleteOutcome::Deleted);

    pqxx::read_transaction read(*connection);
    CHECK(read.query_value<std::size_t>("SELECT count(*) FROM public.character_reputations") == 0);
}

TEST_CASE("Database service rejects cross-character and incomplete inventory mutations",
    "[.integration][Database][Service][Inventory]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DatabaseService integration cases");

    Database::DatabaseService service;
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::Character, *entry));
    auto firstCharacter = service.CreateCharacter(1, "InventoryOne", 1);
    auto secondCharacter = service.CreateCharacter(2, "InventoryTwo", 1);
    REQUIRE(firstCharacter);
    REQUIRE(secondCharacter);

    auto firstItem = service.AddCharacterItem(firstCharacter.Value().id, 100, 0, 0, 1, 10);
    auto secondItem = service.AddCharacterItem(secondCharacter.Value().id, 200, 0, 1, 1, 10);
    REQUIRE(firstItem);
    REQUIRE(secondItem);

    auto crossDelete = service.DeleteCharacterItem(firstCharacter.Value().id, secondItem.Value().id);
    REQUIRE(crossDelete);
    CHECK(crossDelete.Value() == Database::DeleteOutcome::AlreadyAbsent);

    auto crossSwap = service.SwapCharacterItems(firstCharacter.Value().id, firstItem.Value().id,
        secondItem.Value().id, true, 0, 0, 0, 1);
    REQUIRE(crossSwap);
    CHECK(crossSwap.Value() == Database::UpdateOutcome::TargetNotFound);

    auto missingSwap = service.SwapCharacterItems(firstCharacter.Value().id, firstItem.Value().id,
        static_cast<u64>(std::numeric_limits<i64>::max()), true, 0, 0, 0, 1);
    REQUIRE(missingSwap);
    CHECK(missingSwap.Value() == Database::UpdateOutcome::TargetNotFound);

    auto firstPlacements = service.GetCharacterItemPlacements(firstCharacter.Value().id);
    auto secondPlacements = service.GetCharacterItemPlacements(secondCharacter.Value().id);
    REQUIRE(firstPlacements);
    REQUIRE(secondPlacements);
    REQUIRE(firstPlacements.Value().size() == 1);
    REQUIRE(secondPlacements.Value().size() == 1);
    CHECK(firstPlacements.Value().front().itemInstanceId == firstItem.Value().id);
    CHECK(firstPlacements.Value().front().slot == 0);
    CHECK(secondPlacements.Value().front().itemInstanceId == secondItem.Value().id);
    CHECK(secondPlacements.Value().front().slot == 1);

    auto concurrentCharacter = service.CreateCharacter(3, "SlotRace", 1);
    REQUIRE(concurrentCharacter);
    std::atomic_uint32_t insertSuccesses = 0;
    std::atomic_uint32_t insertRejections = 0;
    auto insertSameSlot = [&](u32 itemID)
    {
        auto result = service.AddCharacterItem(concurrentCharacter.Value().id, itemID, 0, 5, 1, 10);
        if (result)
        {
            ++insertSuccesses;
        }
        else if (result.Failure() == Database::OperationFailure::Rejected || result.Failure() == Database::OperationFailure::Conflict)
        {
            ++insertRejections;
        }
    };
    std::thread firstInsert(insertSameSlot, 300);
    std::thread secondInsert(insertSameSlot, 301);
    firstInsert.join();
    secondInsert.join();
    CHECK(insertSuccesses == 1);
    CHECK(insertRejections == 1);

    auto concurrentItems = service.GetCharacterItems(concurrentCharacter.Value().id);
    auto concurrentPlacements = service.GetCharacterItemPlacements(concurrentCharacter.Value().id);
    REQUIRE(concurrentItems);
    REQUIRE(concurrentPlacements);
    CHECK(concurrentItems.Value().size() == 1);
    REQUIRE(concurrentPlacements.Value().size() == 1);
    CHECK(concurrentPlacements.Value().front().slot == 5);

    auto widenedMap = service.SetCharacterMap(firstCharacter.Value().id, 32768);
    REQUIRE(widenedMap);
    CHECK(widenedMap.Value() == Database::UpdateOutcome::Updated);
    auto maximumMap = service.SetCharacterMap(firstCharacter.Value().id,
        static_cast<u32>(std::numeric_limits<i32>::max()));
    REQUIRE(maximumMap);
    CHECK(maximumMap.Value() == Database::UpdateOutcome::Updated);
    auto rejectedMap = service.SetCharacterMap(firstCharacter.Value().id,
        static_cast<u32>(std::numeric_limits<i32>::max()) + 1u);
    CHECK_FALSE(rejectedMap);
    CHECK(rejectedMap.Failure() == Database::OperationFailure::Rejected);
    auto characterAfterRejectedMap = service.GetCharacterByID(firstCharacter.Value().id);
    REQUIRE(characterAfterRejectedMap);
    REQUIRE(characterAfterRejectedMap.Value());
    CHECK(characterAfterRejectedMap.Value()->mapId == static_cast<u32>(std::numeric_limits<i32>::max()));
}

TEST_CASE("Database service routes bundles to distinct physical databases",
    "[.integration][Database][Service][Routing]")
{
    TemporaryDatabase authDatabase;
    TemporaryDatabase characterDatabase;
    TemporaryDatabase worldDatabase;
    auto authEntry = ControllerEntry(authDatabase.Name());
    auto characterEntry = ControllerEntry(characterDatabase.Name());
    auto worldEntry = ControllerEntry(worldDatabase.Name());
    if (!authEntry || !characterEntry || !worldEntry)
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DatabaseService integration cases");

    Database::DatabaseService service;
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::Auth, *authEntry));
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::Character, *characterEntry));
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::World, *worldEntry));

    auto authConnection = authDatabase.Connect();
    auto characterConnection = characterDatabase.Connect();
    auto worldConnection = worldDatabase.Connect();
    pqxx::read_transaction authRead(*authConnection);
    pqxx::read_transaction characterRead(*characterConnection);
    pqxx::read_transaction worldRead(*worldConnection);
    CHECK(authRead.query_value<bool>("SELECT to_regclass('public.accounts') IS NOT NULL"));
    CHECK(authRead.query_value<bool>("SELECT to_regclass('public.characters') IS NULL"));
    CHECK(characterRead.query_value<bool>("SELECT to_regclass('public.characters') IS NOT NULL"));
    CHECK(characterRead.query_value<bool>("SELECT to_regclass('public.maps') IS NULL"));
    CHECK(characterRead.query_value<std::string>(R"sql(SELECT data_type FROM information_schema.columns
        WHERE table_schema = 'public' AND table_name = 'characters' AND column_name = 'map_id')sql") == "integer");
    CHECK(characterRead.query_value<bool>("SELECT to_regclass('public.characters_account_id_id_idx') IS NOT NULL"));
    CHECK(characterRead.query_value<bool>(R"sql(SELECT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conname = 'character_items_character_container_slot_key'))sql"));
    CHECK(characterRead.query_value<bool>(R"sql(SELECT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conname = 'character_items_owner_item_fk'))sql"));
    CHECK(worldRead.query_value<bool>("SELECT to_regclass('public.maps') IS NOT NULL"));
    CHECK(worldRead.query_value<bool>("SELECT to_regclass('public.accounts') IS NULL"));
    CHECK(worldRead.query_value<bool>("SELECT to_regclass('public.creatures_map_id_idx') IS NOT NULL"));
    CHECK(worldRead.query_value<bool>("SELECT to_regclass('public.proximity_triggers_map_id_idx') IS NOT NULL"));
}

TEST_CASE("Database service high-arity setters preserve parameter order",
    "[.integration][Database][Service][Generated]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DatabaseService integration cases");

    Database::DatabaseService service;
    REQUIRE_NOTHROW(service.InitializeBundle(Database::DBType::World, *entry));
    GameDefine::Database::ItemStatTemplate statTemplate{};
    statTemplate.id = 700;
    for (u32 index = 0; index < 8; ++index)
    {
        statTemplate.statTypes[index] = static_cast<u8>(index + 1);
        statTemplate.statValues[index] = -100 - static_cast<i32>(index);
    }
    auto statResult = service.SetItemStatTemplate(statTemplate);
    REQUIRE(statResult);
    CHECK(statResult.Value() == Database::UpdateOutcome::Updated);

    GameDefine::Database::SpellEffect effect{
        .id = 701, .spellID = 702, .effectPriority = 7, .effectType = 2, .effectValue1 = 11, .effectValue2 = 12, .effectValue3 = 13, .effectMiscValue1 = 21, .effectMiscValue2 = 22, .effectMiscValue3 = 23
    };
    auto effectResult = service.SetSpellEffect(effect);
    REQUIRE(effectResult);
    CHECK(effectResult.Value() == Database::UpdateOutcome::Updated);

    auto connection = database.Connect();
    pqxx::read_transaction read(*connection);
    const auto statRows = read.exec("SELECT stat_type_1, stat_type_2, stat_type_3, stat_type_4, stat_type_5, stat_type_6, stat_type_7, stat_type_8, "
                                    "stat_value_1, stat_value_2, stat_value_3, stat_value_4, stat_value_5, stat_value_6, stat_value_7, stat_value_8 "
                                    "FROM public.item_stat_templates WHERE id = 700");
    REQUIRE(statRows.size() == 1);
    const auto statRow = statRows.front();
    for (u32 index = 0; index < 8; ++index)
    {
        CHECK(statRow[index].as<u16>() == index + 1);
        CHECK(statRow[index + 8].as<i32>() == -100 - static_cast<i32>(index));
    }
    const auto effectRows = read.exec("SELECT spell_id, effect_priority, effect_type, effect_value_1, effect_value_2, effect_value_3, "
                                      "effect_misc_value_1, effect_misc_value_2, effect_misc_value_3 FROM public.spell_effects WHERE id = 701");
    REQUIRE(effectRows.size() == 1);
    const auto effectRow = effectRows.front();
    CHECK(effectRow[0].as<u32>() == 702);
    CHECK(effectRow[1].as<u16>() == 7);
    CHECK(effectRow[2].as<u16>() == 2);
    CHECK(effectRow[3].as<i32>() == 11);
    CHECK(effectRow[4].as<i32>() == 12);
    CHECK(effectRow[5].as<i32>() == 13);
    CHECK(effectRow[6].as<i32>() == 21);
    CHECK(effectRow[7].as<i32>() == 22);
    CHECK(effectRow[8].as<i32>() == 23);
}

TEST_CASE("Generated runtime enforces descriptor cardinality", "[.integration][Database][Generated]")
{
    TemporaryDatabase database;
    auto connection = database.Connect();
    {
        pqxx::work setup(*connection);
        setup.exec("CREATE TEMP TABLE generated_mutation_probe (value integer NOT NULL)");
        setup.commit();
    }
    connection->prepare(std::string{ ExactlyOneTestDescriptor::PREPARED_NAME }, "SELECT 7");
    connection->prepare(std::string{ ExactlyOneEmptyTestDescriptor::PREPARED_NAME }, "SELECT 7 WHERE false");
    connection->prepare(std::string{ ExactlyOneMultipleTestDescriptor::PREPARED_NAME }, "SELECT value FROM (VALUES (1), (2)) AS rows(value)");
    connection->prepare(std::string{ ZeroOrOneTestDescriptor::PREPARED_NAME }, "SELECT 7");
    connection->prepare(std::string{ ZeroOrOneEmptyTestDescriptor::PREPARED_NAME }, "SELECT 7 WHERE false");
    connection->prepare(std::string{ ZeroOrOneMultipleTestDescriptor::PREPARED_NAME }, "SELECT value FROM (VALUES (1), (2)) AS rows(value)");
    connection->prepare(std::string{ ZeroOrMoreTestDescriptor::PREPARED_NAME }, "SELECT value FROM (VALUES (3), (1), (2)) AS rows(value) ORDER BY value");
    connection->prepare(std::string{ MutationTestDescriptor::PREPARED_NAME }, "INSERT INTO generated_mutation_probe (value) VALUES ($1)");

    pqxx::work transaction(*connection);
    const auto exactlyOne = Database::Generated::Execute<ExactlyOneTestDescriptor>(transaction);
    CHECK(exactlyOne.value == 7);
    CHECK_THROWS_AS(Database::Generated::Execute<ExactlyOneEmptyTestDescriptor>(transaction), std::runtime_error);
    CHECK_THROWS_AS(Database::Generated::Execute<ExactlyOneMultipleTestDescriptor>(transaction), std::runtime_error);

    const auto zeroOrOne = Database::Generated::Execute<ZeroOrOneTestDescriptor>(transaction);
    REQUIRE(zeroOrOne);
    CHECK(zeroOrOne->value == 7);
    CHECK_FALSE(Database::Generated::Execute<ZeroOrOneEmptyTestDescriptor>(transaction));
    CHECK_THROWS_AS(Database::Generated::Execute<ZeroOrOneMultipleTestDescriptor>(transaction), std::runtime_error);

    const auto zeroOrMore = Database::Generated::Execute<ZeroOrMoreTestDescriptor>(transaction);
    REQUIRE(zeroOrMore.size() == 3);
    CHECK(zeroOrMore[0].value == 1);
    CHECK(zeroOrMore[1].value == 2);
    CHECK(zeroOrMore[2].value == 3);

    const auto mutation = Database::Generated::Execute<MutationTestDescriptor>(transaction, 42);
    CHECK(mutation.affected_rows() == 1);
    CHECK(transaction.query_value<int>("SELECT value FROM generated_mutation_probe") == 42);
    transaction.commit();
}

TEST_CASE("Pool prepares every physical and reconnected connection", "[.integration][Database][Connection]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DBController integration cases");
    }

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::Auth, *entry));
    REQUIRE(controller.SetDBEntry(Database::DBType::Character, *entry));
    REQUIRE(controller.SetDBEntry(Database::DBType::World, *entry));
    std::size_t preparationCount = 0;
    REQUIRE(controller.SetPrepareCallback(Database::DBType::Auth, [&](Database::DBConnection& connection)
    {
        ++preparationCount;
        connection.connection->prepare("server_test_ping", "SELECT 1");
    }));
    REQUIRE(controller.SetPrepareCallback(Database::DBType::Character, [](Database::DBConnection& connection)
    {
        if (connection.type != Database::DBType::Character)
            throw std::logic_error("Character pool routed the wrong physical bundle");
    }));
    REQUIRE(controller.SetPrepareCallback(Database::DBType::World, [](Database::DBConnection& connection)
    {
        if (connection.type != Database::DBType::World)
            throw std::logic_error("World pool routed the wrong physical bundle");
    }));

    auto first = controller.GetConnection(Database::DBType::Auth);
    auto second = controller.GetConnection(Database::DBType::Auth);
    auto character = controller.GetConnection(Database::DBType::Character);
    auto world = controller.GetConnection(Database::DBType::World);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(character);
    REQUIRE(world);
    CHECK(character->type == Database::DBType::Character);
    CHECK(world->type == Database::DBType::World);
    CHECK(preparationCount == 2);
    CHECK_THROWS(first->NewTransaction(Database::DBType::Character));

    first->connection->close();
    first = {};
    auto reconnected = controller.GetConnection(Database::DBType::Auth);
    REQUIRE(reconnected);
    CHECK(preparationCount == 3);
    auto transaction = reconnected->NewReadTransaction(Database::DBType::Auth);
    CHECK(transaction.query_value<int>(pqxx::prepped{ "server_test_ping" }) == 1);

    Database::DBController retryController;
    REQUIRE(retryController.SetDBEntry(Database::DBType::Auth, *entry));
    std::size_t attempts = 0;
    REQUIRE(retryController.SetPrepareCallback(Database::DBType::Auth, [&](Database::DBConnection&)
    {
        if (++attempts == 1)
            throw std::runtime_error("injected preparation failure");
    }));
    CHECK_THROWS(retryController.GetConnection(Database::DBType::Auth));
    CHECK(retryController.GetConnection(Database::DBType::Auth));
    CHECK(attempts == 2);

    entry->maximumPoolSize = 1;
    entry->acquisitionTimeoutMilliseconds = 20;
    Database::DBController boundedController;
    REQUIRE(boundedController.SetDBEntry(Database::DBType::Auth, *entry));
    auto onlyLease = boundedController.GetConnection(Database::DBType::Auth);
    REQUIRE(onlyLease);
    const auto waitStarted = std::chrono::steady_clock::now();
    CHECK_FALSE(boundedController.GetConnection(Database::DBType::Auth));
    CHECK(std::chrono::steady_clock::now() - waitStarted >= std::chrono::milliseconds(20));
    onlyLease = {};
    CHECK(boundedController.GetConnection(Database::DBType::Auth));
}

TEST_CASE("DBController createIfMissing creates a missing bundle database", "[.integration][Database][Connection]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name(), true);
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run DBController integration cases");
    }
    database.Drop();

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::Auth, *entry));
    auto connection = controller.GetConnection(Database::DBType::Auth);
    REQUIRE(connection);
    CHECK(connection->connection->is_open());
}

TEST_CASE("Reliable transaction retries the entire transaction", "[.integration][Database][Reliability]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run reliable execution integration cases");
    }

    auto setupConnection = database.Connect();
    {
        pqxx::work transaction(*setupConnection);
        transaction.exec("CREATE TABLE retry_probe (value integer NOT NULL)");
        transaction.commit();
    }

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::World, *entry));
    std::size_t attempts = 0;
    auto result = Database::RunTransaction(controller, Database::DBType::World, "test_transaction_retry",
        [&](pqxx::work& transaction)
    {
        ++attempts;
        transaction.exec("INSERT INTO retry_probe (value) VALUES (1)");
        if (attempts == 1)
            throw pqxx::serialization_failure("injected serialization failure", "INSERT INTO retry_probe", "40001");
        return attempts;
    });

    REQUIRE(result);
    CHECK(result.Value() == 2);
    CHECK(attempts == 2);
    pqxx::read_transaction verification(*setupConnection);
    CHECK(verification.query_value<std::size_t>("SELECT count(*) FROM retry_probe") == 1);
}

TEST_CASE("Reliable read retries without exposing connection handling", "[.integration][Database][Reliability]")
{
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run reliable execution integration cases");
    }

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::Auth, *entry));
    std::size_t attempts = 0;
    auto result = Database::RunRead(controller, Database::DBType::Auth, "test_read_retry",
        [&](pqxx::read_transaction& transaction)
    {
        if (++attempts == 1)
            throw pqxx::broken_connection("injected connection failure");
        return transaction.query_value<int>("SELECT 42");
    });

    REQUIRE(result);
    CHECK(result.Value() == 42);
    CHECK(attempts == 2);
}

TEST_CASE("Reliable execution reconnects after PostgreSQL terminates a backend", "[.integration][Database][Reliability][Connection]")
{
    EnsureTestLogger();
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run real reconnect integration cases");
    }

    auto setupConnection = database.Connect();
    {
        pqxx::work setup(*setupConnection);
        setup.exec("CREATE TABLE reconnect_probe (value integer NOT NULL)");
        setup.commit();
    }
    auto killerConnection = database.Connect();

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::Auth, *entry));
    std::size_t preparationCount = 0;
    REQUIRE(controller.SetPrepareCallback(Database::DBType::Auth, [&](Database::DBConnection& connection)
    {
        ++preparationCount;
        connection.connection->prepare("real_reconnect_ping", "SELECT 42");
    }));

    std::size_t readAttempts = 0;
    auto readResult = Database::RunRead(controller, Database::DBType::Auth, "test_real_read_reconnect",
        [&](pqxx::read_transaction& transaction)
    {
        if (++readAttempts == 1)
        {
            const int backendPID = transaction.query_value<int>("SELECT pg_backend_pid()");
            pqxx::nontransaction killer(*killerConnection);
            REQUIRE(killer.query_value<bool>("SELECT pg_terminate_backend(" + std::to_string(backendPID) + ")"));
        }
        return transaction.query_value<int>(pqxx::prepped{ "real_reconnect_ping" });
    });

    REQUIRE(readResult);
    CHECK(readResult.Value() == 42);
    CHECK(readAttempts == 2);
    CHECK(preparationCount == 2);

    std::size_t transactionAttempts = 0;
    auto transactionResult = Database::RunTransaction(controller, Database::DBType::Auth, "test_real_transaction_reconnect",
        [&](pqxx::work& transaction)
    {
        ++transactionAttempts;
        transaction.exec("INSERT INTO reconnect_probe (value) VALUES (1)");
        if (transactionAttempts == 1)
        {
            const int backendPID = transaction.query_value<int>("SELECT pg_backend_pid()");
            pqxx::nontransaction killer(*killerConnection);
            REQUIRE(killer.query_value<bool>("SELECT pg_terminate_backend(" + std::to_string(backendPID) + ")"));
            transaction.exec("SELECT 1");
        }
        return true;
    });

    REQUIRE(transactionResult);
    CHECK(transactionAttempts == 2);
    CHECK(preparationCount == 3);
    pqxx::read_transaction verification(*setupConnection);
    CHECK(verification.query_value<std::size_t>("SELECT count(*) FROM reconnect_probe") == 1);
}

TEST_CASE("Reliable execution classifies rejected and indeterminate outcomes", "[.integration][Database][Reliability]")
{
    EnsureTestLogger();
    TemporaryDatabase database;
    auto entry = ControllerEntry(database.Name());
    if (!entry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run reliable execution integration cases");
    }

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::Auth, *entry));

    std::size_t rejectedAttempts = 0;
    auto rejected = Database::RunRead(controller, Database::DBType::Auth, "test_rejected_read",
        [&](pqxx::read_transaction& transaction)
    {
        ++rejectedAttempts;
        return transaction.query_value<int>("SELECT missing_test_column");
    });
    CHECK_FALSE(rejected);
    CHECK(rejected.Failure() == Database::OperationFailure::Rejected);
    CHECK(rejectedAttempts == 1);

    std::size_t conflictAttempts = 0;
    auto conflict = Database::RunTransaction(controller, Database::DBType::Auth, "test_conflict_transaction",
        [&](pqxx::work&)
    {
        ++conflictAttempts;
        throw pqxx::serialization_failure("injected persistent serialization conflict", "test conflict", "40001");
        return false;
    });
    CHECK_FALSE(conflict);
    CHECK(conflict.Failure() == Database::OperationFailure::Conflict);
    CHECK(conflictAttempts == 3);

    std::size_t unavailableAttempts = 0;
    auto unavailable = Database::RunRead(controller, Database::DBType::Auth, "test_unavailable_read",
        [&](pqxx::read_transaction&)
    {
        ++unavailableAttempts;
        throw pqxx::broken_connection("injected persistent connection failure");
        return false;
    });
    CHECK_FALSE(unavailable);
    CHECK(unavailable.Failure() == Database::OperationFailure::Unavailable);
    CHECK(unavailableAttempts == 3);

    std::size_t failedAttempts = 0;
    auto failed = Database::RunRead(controller, Database::DBType::Auth, "test_failed_read",
        [&](pqxx::read_transaction&)
    {
        ++failedAttempts;
        throw std::runtime_error("injected application failure");
        return false;
    });
    CHECK_FALSE(failed);
    CHECK(failed.Failure() == Database::OperationFailure::Failed);
    CHECK(failedAttempts == 1);

    std::size_t attempts = 0;
    auto indeterminate = Database::RunTransaction(controller, Database::DBType::Auth, "test_indeterminate_transaction",
        [&](pqxx::work&)
    {
        ++attempts;
        throw pqxx::in_doubt_error("injected indeterminate outcome");
        return false;
    });
    CHECK_FALSE(indeterminate);
    CHECK(indeterminate.Failure() == Database::OperationFailure::Indeterminate);
    CHECK(attempts == 1);

    std::size_t commitBoundaryAttempts = 0;
    std::size_t commitCalls = 0;
    auto commitIndeterminate = Database::RunTransactionWithCommit(controller, Database::DBType::Auth,
        "test_commit_indeterminate",
        [&](pqxx::work&)
    {
        ++commitBoundaryAttempts;
        return true;
    },
        [&](pqxx::work&)
    {
        ++commitCalls;
        throw pqxx::in_doubt_error("injected lost COMMIT response");
    });
    CHECK_FALSE(commitIndeterminate);
    CHECK(commitIndeterminate.Failure() == Database::OperationFailure::Indeterminate);
    CHECK(commitBoundaryAttempts == 1);
    CHECK(commitCalls == 1);
}

TEST_CASE("Startup cache loaders stage and retry complete bundle snapshots", "[.integration][Database][Reliability][Startup]")
{
    EnsureTestLogger();
    TemporaryDatabase worldDatabase;
    auto worldEntry = ControllerEntry(worldDatabase.Name());
    if (!worldEntry)
    {
        FAIL("Set NOVUS_TEST_POSTGRES_HOST/USER/PASSWORD to run startup loader integration cases");
    }

    auto worldMigrationConnection = worldDatabase.Connect();
    Database::MigrationRunner::Run(Database::DBType::World, *worldMigrationConnection, Database::MigrationMode::Migrate);
    {
        pqxx::work seed(*worldMigrationConnection);
        seed.exec(R"sql(INSERT INTO public.spells
            (id, name, description, aura_description, icon_id, cast_time, cooldown, duration)
            SELECT value, 'spell-' || value, '', '', 0, 0, 0, 0 FROM generate_series(1, 513) AS value)sql");
        seed.exec(R"sql(INSERT INTO public.spell_effects
            (id, spell_id, effect_priority, effect_type, effect_value_1, effect_value_2, effect_value_3,
             effect_misc_value_1, effect_misc_value_2, effect_misc_value_3)
            SELECT value, value, 1, 1, value, 0, 0, 0, 0, 0 FROM generate_series(1, 513) AS value)sql");
        seed.exec(R"sql(INSERT INTO public.spell_effects
            (id, spell_id, effect_priority, effect_type, effect_value_1, effect_value_2, effect_value_3,
             effect_misc_value_1, effect_misc_value_2, effect_misc_value_3)
            VALUES (514, 513, 9, 1, 999, 0, 0, 0, 0, 0))sql");
        seed.exec(R"sql(INSERT INTO public.spell_proc_data
            (id, phase_mask, type_mask, hit_mask, flags, procs_per_min, chance_to_proc, internal_cooldown_ms, charges)
            SELECT value, 1, 1, 1, 0, 1, 0, 0, -1 FROM generate_series(1, 513) AS value)sql");
        seed.exec(R"sql(INSERT INTO public.spell_proc_link (id, spell_id, effect_mask, proc_data_id)
            SELECT value, value, 1, value FROM generate_series(1, 513) AS value)sql");
        seed.exec("INSERT INTO public.spell_proc_link (id, spell_id, effect_mask, proc_data_id) VALUES (514, 513, 2, 512)");
        seed.commit();
    }

    Database::DBController controller;
    REQUIRE(controller.SetDBEntry(Database::DBType::World, *worldEntry));
    REQUIRE(controller.SetPrepareCallback(Database::DBType::World, [](Database::DBConnection& connection)
    {
        Database::Generated::PrepareStatements<Database::SchemaTraits<Database::DBType::World>::Schema>(*connection.connection);
    }));

    std::size_t worldAttempts = 0;
    auto worldResult = Database::RunRead(controller, Database::DBType::World, "test_load_world_cache",
        [&](pqxx::read_transaction& transaction)
    {
        TestWorldCacheSnapshot snapshot;
        Database::Detail::Map::Loading::LoadMapTables(transaction, snapshot.maps);
        Database::Detail::Spell::Loading::LoadSpellTables(transaction, snapshot.spells);
        Database::Detail::Item::Loading::LoadItemTables(transaction, snapshot.items);
        Database::Detail::Creature::Loading::LoadCreatureTables(transaction, snapshot.creatures);
        Database::Detail::Faction::Loading::LoadFactionTables(transaction, snapshot.factions);
        if (++worldAttempts == 1)
        {
            snapshot.maps.idToDefinition[0xFFFFFFFFu] = {};
            throw pqxx::serialization_failure("injected startup snapshot retry", "startup cache load", "40001");
        }
        return snapshot;
    });

    REQUIRE(worldResult);
    CHECK(worldAttempts == 2);
    CHECK(worldResult.Value().maps.idToDefinition.find(0xFFFFFFFFu) == worldResult.Value().maps.idToDefinition.end());
    CHECK(worldResult.Value().spells.idToDefinition.size() == 513);
    REQUIRE(worldResult.Value().spells.spellIDToEffects.at(513).effects.size() == 2);
    CHECK(worldResult.Value().spells.spellIDToEffects.at(513).effects[0].effectPriority == 9);
    CHECK(worldResult.Value().spells.spellIDToEffects.at(513).effects[1].effectPriority == 1);
    REQUIRE(worldResult.Value().spells.spellIDToProcInfo.at(513).links.size() == 2);
    CHECK(worldResult.Value().spells.spellIDToProcInfo.at(513).links[0].procData.id == 512);
    CHECK(worldResult.Value().spells.spellIDToProcInfo.at(513).links[1].procData.id == 513);
}
