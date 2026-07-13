#include "MigrationRunner.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace Database
{
    namespace
    {
        constexpr std::string_view Begin = "BEGIN;\n";
        constexpr std::string_view Commit = "COMMIT;\n";

        template <typename Schema>
        std::vector<MigrationDescriptor> ExpectedMigrations()
        {
            std::vector<MigrationDescriptor> result;
            std::apply([&](auto... value)
            {
                (result.push_back({ std::string(decltype(value)::ID), std::string(decltype(value)::NAME), std::string(decltype(value)::PARENT_HASH),
                    std::string(decltype(value)::TARGET_HASH), std::string(decltype(value)::CONTENT_HASH), std::string(decltype(value)::SQL),
                    decltype(value)::TRANSACTIONAL }), ...);
            }, typename Schema::Migrations{});
            return result;
        }

        template <typename Schema>
        bool AnyOwnedTableExists(pqxx::transaction_base& transaction)
        {
            bool exists = false;
            std::apply([&](auto... table)
            {
                ((exists = exists || transaction.query_value<bool>(
                    "SELECT to_regclass($1) IS NOT NULL", pqxx::params{
                        std::string(decltype(table)::SCHEMA_NAME) + "." + std::string(decltype(table)::TABLE_NAME) })), ...);
            }, typename Schema::Tables{});
            return exists;
        }

        [[noreturn]] void HistoryError(std::string_view bundle, std::size_t ordinal, std::string_view reason)
        {
            throw std::runtime_error("Database migration history for bundle '" + std::string(bundle)
                + "' is invalid at ordinal " + std::to_string(ordinal) + ": " + std::string(reason));
        }
    }

    std::string_view MigrationBody(std::string_view sql)
    {
        if (!sql.starts_with(Begin) || !sql.ends_with(Commit) || sql.size() < Begin.size() + Commit.size())
            throw std::runtime_error("Generated migration SQL must have exact BEGIN;\\n and COMMIT;\\n wrappers");
        return sql.substr(Begin.size(), sql.size() - Begin.size() - Commit.size());
    }

    std::size_t ValidateMigrationHistory(std::string_view bundle, const std::vector<MigrationDescriptor>& expected,
        const std::vector<AppliedMigration>& applied, MigrationMode mode)
    {
        if (applied.size() > expected.size())
            HistoryError(bundle, expected.size() + 1, "database contains an unknown/extra migration");

        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            MigrationBody(expected[i].sql);
            if (!expected[i].transactional)
                HistoryError(bundle, i + 1, "non-transactional migration cannot be applied atomically");
            if (i > 0 && expected[i].parentHash != expected[i - 1].targetHash)
                HistoryError(bundle, i + 1, "generated parent chain is broken");
            if (i >= applied.size())
                continue;

            const auto& actual = applied[i];
            const auto& wanted = expected[i];
            if (actual.ordinal != i + 1) HistoryError(bundle, i + 1, "ordinal is changed or reordered");
            if (actual.id != wanted.id) HistoryError(bundle, i + 1, "migration ID is unknown, changed, or reordered");
            if (actual.name != wanted.name) HistoryError(bundle, i + 1, "migration name changed");
            if (actual.parentHash != wanted.parentHash) HistoryError(bundle, i + 1, "parent hash changed or diverged");
            if (actual.targetHash != wanted.targetHash) HistoryError(bundle, i + 1, "target hash changed or diverged");
            if (actual.contentHash != wanted.contentHash) HistoryError(bundle, i + 1, "content hash changed");
        }

        const std::size_t pending = expected.size() - applied.size();
        if (mode == MigrationMode::Validate && pending != 0)
            throw std::runtime_error("Bundle '" + std::string(bundle) + "' has " + std::to_string(pending)
                + " pending migration(s) in Validate mode");
        return pending;
    }

    void MigrationRunner::Run(DBType type, DBConnection& connection, MigrationMode mode)
    {
        Run(type, *connection.connection, mode);
    }

    void MigrationRunner::Run(DBType type, pqxx::connection& connection, MigrationMode mode, MigrationHook beforeLedger)
    {
        if (mode == MigrationMode::Disabled)
            return;
        switch (type)
        {
        case DBType::Auth: RunSchema<SchemaTraits<DBType::Auth>::Schema>(connection, SchemaTraits<DBType::Auth>::Name, mode, beforeLedger); break;
        case DBType::Character: RunSchema<SchemaTraits<DBType::Character>::Schema>(connection, SchemaTraits<DBType::Character>::Name, mode, beforeLedger); break;
        case DBType::World: RunSchema<SchemaTraits<DBType::World>::Schema>(connection, SchemaTraits<DBType::World>::Name, mode, beforeLedger); break;
        default: throw std::invalid_argument("No generated schema is available for the requested database bundle");
        }
    }

    template <typename Schema>
    void MigrationRunner::RunSchema(pqxx::connection& connection, std::string_view bundle, MigrationMode mode, const MigrationHook& beforeLedger)
    {
        if (mode == MigrationMode::Validate)
        {
            pqxx::read_transaction read(connection);
            const bool hasLedger = read.query_value<bool>("SELECT to_regclass('metagen.schema_migrations') IS NOT NULL");
            const bool hasState = read.query_value<bool>("SELECT to_regclass('metagen.bundle_state') IS NOT NULL");
            if (!hasLedger || !hasState)
                throw std::runtime_error("MetaGen catalog is missing in Validate mode");

            const auto expected = ExpectedMigrations<Schema>();
            const auto appliedRows = read.exec(R"sql(SELECT ordinal, migration_id, migration_name, parent_hash, target_hash, content_hash
                FROM metagen.schema_migrations WHERE bundle = $1 ORDER BY ordinal)sql", pqxx::params{ bundle });
            if (appliedRows.empty() && AnyOwnedTableExists<Schema>(read))
                throw std::runtime_error("Bundle '" + std::string(bundle) + "' has generated-owned tables but no migration ledger");

            std::vector<AppliedMigration> applied;
            applied.reserve(appliedRows.size());
            for (const auto& row : appliedRows)
            {
                applied.push_back({ row[0].as<std::size_t>(), row[1].as<std::string>(), row[2].as<std::string>(),
                    row[3].as<std::string>(), row[4].as<std::string>(), row[5].as<std::string>() });
            }
            ValidateMigrationHistory(bundle, expected, applied, mode);

            const auto state = read.exec("SELECT manifest_hash FROM metagen.bundle_state WHERE bundle = $1", pqxx::params{ bundle });
            if (state.size() != 1 || state.front().front().as<std::string_view>() != Schema::MANIFEST_HASH)
                throw std::runtime_error("Bundle state is missing or its manifest hash does not match the generated schema");
            read.commit();
            return;
        }

        pqxx::work work(connection);
        // Serialize bootstrap DDL as well as migration application. PostgreSQL's
        // IF NOT EXISTS does not make concurrent catalog creation race-free.
        work.exec("SELECT pg_advisory_xact_lock(hashtextextended('metagen.catalog.bootstrap', 0))");
        work.exec("CREATE SCHEMA IF NOT EXISTS metagen");
        work.exec(R"sql(CREATE TABLE IF NOT EXISTS metagen.schema_migrations (
            bundle text NOT NULL, ordinal bigint NOT NULL, migration_id text NOT NULL, migration_name text NOT NULL,
            parent_hash text NOT NULL, target_hash text NOT NULL, content_hash text NOT NULL,
            applied_at timestamptz NOT NULL DEFAULT clock_timestamp(), PRIMARY KEY (bundle, ordinal),
            UNIQUE (bundle, migration_id)))sql");
        work.exec(R"sql(CREATE TABLE IF NOT EXISTS metagen.bundle_state (
            bundle text PRIMARY KEY, manifest_hash text NOT NULL, updated_at timestamptz NOT NULL DEFAULT clock_timestamp()))sql");
        work.exec("SELECT pg_advisory_xact_lock(hashtextextended($1, 0))", pqxx::params{ bundle });
        const auto expected = ExpectedMigrations<Schema>();
        const auto appliedRows = work.exec(R"sql(SELECT ordinal, migration_id, migration_name, parent_hash, target_hash, content_hash
            FROM metagen.schema_migrations WHERE bundle = $1 ORDER BY ordinal)sql", pqxx::params{ bundle });

        if (appliedRows.empty() && AnyOwnedTableExists<Schema>(work))
            throw std::runtime_error("Bundle '" + std::string(bundle) + "' has generated-owned tables but no migration ledger. "
                "Automatic baseline adoption is unsafe; migrate the data to an empty database or perform a reviewed ledger adoption once typed catalog metadata is available.");
        std::vector<AppliedMigration> applied;
        applied.reserve(appliedRows.size());
        for (const auto& row : appliedRows)
        {
            applied.push_back({ row[0].as<std::size_t>(), row[1].as<std::string>(), row[2].as<std::string>(),
                row[3].as<std::string>(), row[4].as<std::string>(), row[5].as<std::string>() });
        }
        ValidateMigrationHistory(bundle, expected, applied, mode);

        const auto state = work.exec("SELECT manifest_hash FROM metagen.bundle_state WHERE bundle = $1", pqxx::params{ bundle });
        if (applied.size() == expected.size() && !applied.empty())
        {
            if (state.size() != 1 || state.front().front().as<std::string_view>() != Schema::MANIFEST_HASH)
                throw std::runtime_error("Bundle state is missing or its manifest hash does not match the generated schema");
        }

        for (std::size_t i = applied.size(); i < expected.size(); ++i)
        {
            const auto& migration = expected[i];
            work.exec(std::string(MigrationBody(migration.sql)));
            if (beforeLedger)
                beforeLedger(work, i + 1);
            work.exec(R"sql(INSERT INTO metagen.schema_migrations
                (bundle, ordinal, migration_id, migration_name, parent_hash, target_hash, content_hash)
                VALUES ($1, $2, $3, $4, $5, $6, $7))sql",
                pqxx::params{ bundle, i + 1, migration.id, migration.name, migration.parentHash, migration.targetHash, migration.contentHash });
        }

        work.exec(R"sql(INSERT INTO metagen.bundle_state (bundle, manifest_hash) VALUES ($1, $2)
            ON CONFLICT (bundle) DO UPDATE SET manifest_hash = EXCLUDED.manifest_hash, updated_at = clock_timestamp())sql",
            pqxx::params{ bundle, Schema::MANIFEST_HASH });
        const auto finalHash = work.query_value<std::string>("SELECT manifest_hash FROM metagen.bundle_state WHERE bundle = $1", pqxx::params{ bundle });
        if (finalHash != Schema::MANIFEST_HASH)
            throw std::runtime_error("Final bundle manifest hash does not match the generated schema manifest");
        work.commit();
    }
}
