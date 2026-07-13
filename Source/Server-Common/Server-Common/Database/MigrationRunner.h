#pragma once

#include "DBController.h"
#include "SchemaTraits.h"

#include <pqxx/pqxx>

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace Database
{
    struct MigrationDescriptor
    {
        std::string id, name, parentHash, targetHash, contentHash, sql;
        bool transactional = true;
    };

    struct AppliedMigration
    {
        std::size_t ordinal = 0;
        std::string id, name, parentHash, targetHash, contentHash;
    };

    std::string_view MigrationBody(std::string_view wrappedSql);
    std::size_t ValidateMigrationHistory(std::string_view bundle, const std::vector<MigrationDescriptor>& expected,
        const std::vector<AppliedMigration>& applied, MigrationMode mode);

    class MigrationRunner
    {
    public:
        using MigrationHook = std::function<void(pqxx::work&, std::size_t)>;

        static void Run(DBType type, DBConnection& connection, MigrationMode mode);
        static void Run(DBType type, pqxx::connection& connection, MigrationMode mode, MigrationHook beforeLedger = {});

    private:
        template <typename Schema>
        static void RunSchema(pqxx::connection& connection, std::string_view bundle, MigrationMode mode, const MigrationHook& beforeLedger);
    };
}
