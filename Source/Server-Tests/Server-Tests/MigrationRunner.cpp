#include <Server-Common/Database/MigrationRunner.h>
#include <Server-Common/Database/SchemaTraits.h>

#include <catch2/catch2.hpp>

#include <type_traits>

namespace
{
    Database::MigrationDescriptor Baseline()
    {
        return {
            "0001_baseline", "baseline", "root", "manifest-1", "content-1",
            "BEGIN;\nSELECT 1;\nCOMMIT;\n", true
        };
    }

    Database::AppliedMigration AppliedBaseline()
    {
        return { 1, "0001_baseline", "baseline", "root", "manifest-1", "content-1" };
    }
}

TEST_CASE("Database leases and schema routing are compile-time constrained", "[Database][Connection]")
{
    STATIC_REQUIRE(!std::is_copy_constructible_v<Database::DBConnectionLease>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<Database::DBConnectionLease>);
    STATIC_REQUIRE(std::is_move_constructible_v<Database::DBConnectionLease>);
    STATIC_REQUIRE((std::is_same_v<Database::SchemaTraits<Database::DBType::Auth>::Schema, MetaGen::Postgres::Auth::AuthSchema>));
    STATIC_REQUIRE((std::is_same_v<Database::SchemaTraits<Database::DBType::Character>::Schema, MetaGen::Postgres::Character::CharacterSchema>));
    STATIC_REQUIRE((std::is_same_v<Database::SchemaTraits<Database::DBType::World>::Schema, MetaGen::Postgres::World::WorldSchema>));
}

TEST_CASE("Migration wrapper extraction is strict", "[Database][Migration]")
{
    CHECK(Database::MigrationBody("BEGIN;\nSELECT 1;\nCOMMIT;\n") == "SELECT 1;\n");
    CHECK_THROWS(Database::MigrationBody(" BEGIN;\nSELECT 1;\nCOMMIT;\n"));
    CHECK_THROWS(Database::MigrationBody("BEGIN;\r\nSELECT 1;\r\nCOMMIT;\r\n"));
    CHECK_THROWS(Database::MigrationBody("BEGIN;\nSELECT 1;\nCOMMIT;"));
}

TEST_CASE("Empty migration history has a pending baseline", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    CHECK(Database::ValidateMigrationHistory("auth", expected, {}, Database::MigrationMode::Migrate) == 1);
}

TEST_CASE("Applied migration history is a no-op on second startup", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    const std::vector applied{ AppliedBaseline() };
    CHECK(Database::ValidateMigrationHistory("auth", expected, applied, Database::MigrationMode::Migrate) == 0);
    CHECK(Database::ValidateMigrationHistory("auth", expected, applied, Database::MigrationMode::Validate) == 0);
}

TEST_CASE("Validate mode rejects pending migrations", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    CHECK_THROWS_WITH(
        Database::ValidateMigrationHistory("auth", expected, {}, Database::MigrationMode::Validate),
        "Bundle 'auth' has 1 pending migration(s) in Validate mode");
}

TEST_CASE("Migration history rejects changed content", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    auto changed = AppliedBaseline();
    changed.contentHash = "changed";
    CHECK_THROWS_WITH(
        Database::ValidateMigrationHistory("auth", expected, { changed }, Database::MigrationMode::Migrate),
        "Database migration history for bundle 'auth' is invalid at ordinal 1: content hash changed");
}

TEST_CASE("Migration history rejects reordered and divergent entries", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    auto reordered = AppliedBaseline();
    reordered.ordinal = 2;
    CHECK_THROWS(Database::ValidateMigrationHistory("auth", expected, { reordered }, Database::MigrationMode::Migrate));

    auto divergent = AppliedBaseline();
    divergent.targetHash = "divergent";
    CHECK_THROWS(Database::ValidateMigrationHistory("auth", expected, { divergent }, Database::MigrationMode::Migrate));
}

TEST_CASE("Migration history rejects unknown extra entries", "[Database][Migration]")
{
    const std::vector expected{ Baseline() };
    auto extra = AppliedBaseline();
    extra.ordinal = 2;
    extra.id = "0002_unknown";
    CHECK_THROWS_WITH(
        Database::ValidateMigrationHistory("auth", expected, { AppliedBaseline(), extra }, Database::MigrationMode::Migrate),
        "Database migration history for bundle 'auth' is invalid at ordinal 2: database contains an unknown/extra migration");
}

TEST_CASE("Generated migration history rejects a broken parent chain", "[Database][Migration]")
{
    auto first = Baseline();
    auto second = Baseline();
    second.id = "0002_forward";
    second.parentHash = "not-manifest-1";
    second.targetHash = "manifest-2";
    second.contentHash = "content-2";
    CHECK_THROWS_WITH(
        Database::ValidateMigrationHistory("auth", { first, second }, { AppliedBaseline() }, Database::MigrationMode::Migrate),
        "Database migration history for bundle 'auth' is invalid at ordinal 2: generated parent chain is broken");
}

TEST_CASE("Malformed and non-transactional generated migrations are fatal even after application", "[Database][Migration]")
{
    auto malformed = Baseline();
    malformed.sql = "SELECT 1;";
    CHECK_THROWS(Database::ValidateMigrationHistory("auth", { malformed }, { AppliedBaseline() }, Database::MigrationMode::Migrate));

    auto nonTransactional = Baseline();
    nonTransactional.transactional = false;
    CHECK_THROWS_WITH(
        Database::ValidateMigrationHistory("auth", { nonTransactional }, { AppliedBaseline() }, Database::MigrationMode::Migrate),
        "Database migration history for bundle 'auth' is invalid at ordinal 1: non-transactional migration cannot be applied atomically");
}
