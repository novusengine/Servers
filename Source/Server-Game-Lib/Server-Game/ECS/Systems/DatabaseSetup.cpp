#include "DatabaseSetup.h"

#include "Server-Game/ECS/Singletons/GameCache.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/JsonUtils.h>

#include <entt/entt.hpp>

#include <stdexcept>
#include <utility>

namespace ECS::Systems
{
    namespace
    {
        Database::MigrationMode ParseMigrationMode(const std::string& value)
        {
            if (value == "Migrate") return Database::MigrationMode::Migrate;
            if (value == "Validate") return Database::MigrationMode::Validate;
            if (value == "Disabled") return Database::MigrationMode::Disabled;
            throw std::runtime_error("Unknown database migrationMode '" + value + "'");
        }

        Database::DBEntry ReadBundle(const nlohmann::json& config, std::string_view bundle)
        {
            nlohmann::json merged = config.at("Connection");
            nlohmann::json bundleConfig = nlohmann::json::object();
            if (config.contains("Bundles") && config.at("Bundles").contains(bundle))
                bundleConfig = config.at("Bundles").at(bundle);
            if (bundleConfig.contains("Connection"))
                merged.merge_patch(bundleConfig.at("Connection"));
            Database::DBEntry entry(merged.at("address"), merged.at("port"), merged.at("database"),
                merged.at("user"), merged.at("password"),
                ParseMigrationMode(bundleConfig.value("migrationMode", "Migrate")),
                bundleConfig.value("createIfMissing", false));
            entry.connectTimeoutSeconds = merged.value("connectTimeoutSeconds", entry.connectTimeoutSeconds);
            entry.statementTimeoutMilliseconds = merged.value("statementTimeoutMilliseconds", entry.statementTimeoutMilliseconds);
            entry.lockTimeoutMilliseconds = merged.value("lockTimeoutMilliseconds", entry.lockTimeoutMilliseconds);
            entry.idleInTransactionTimeoutMilliseconds = merged.value("idleInTransactionTimeoutMilliseconds", entry.idleInTransactionTimeoutMilliseconds);
            entry.applicationName = merged.value("applicationName", entry.applicationName);
            entry.sslMode = merged.value("sslMode", entry.sslMode);
            entry.keepalivesIdleSeconds = merged.value("keepalivesIdleSeconds", entry.keepalivesIdleSeconds);
            entry.keepalivesIntervalSeconds = merged.value("keepalivesIntervalSeconds", entry.keepalivesIntervalSeconds);
            entry.keepalivesCount = merged.value("keepalivesCount", entry.keepalivesCount);
            entry.maximumPoolSize = merged.value("maximumPoolSize", entry.maximumPoolSize);
            entry.acquisitionTimeoutMilliseconds = merged.value("acquisitionTimeoutMilliseconds", entry.acquisitionTimeoutMilliseconds);
            entry.RefreshConnectionString();
            return entry;
        }

    }

    void DatabaseSetup::Init(entt::registry& registry)
    {
        auto& gameCache = registry.ctx().emplace<Singletons::GameCache>();
        nlohmann::ordered_json fallback = {
            { "Connection", {
                { "address", "127.0.0.1" }, { "port", 5432 }, { "database", "postgres" },
                { "user", "postgres" }, { "password", "postgres" },
                { "connectTimeoutSeconds", 10 }, { "statementTimeoutMilliseconds", 15000 },
                { "lockTimeoutMilliseconds", 5000 }, { "idleInTransactionTimeoutMilliseconds", 30000 },
                { "applicationName", "novus-server" }, { "sslMode", "prefer" },
                { "keepalivesIdleSeconds", 30 }, { "keepalivesIntervalSeconds", 10 }, { "keepalivesCount", 3 },
                { "maximumPoolSize", 4 }, { "acquisitionTimeoutMilliseconds", 2000 }
            } },
            { "Bundles", {
                { "Auth", { { "migrationMode", "Migrate" }, { "createIfMissing", false } } },
                { "Character", { { "migrationMode", "Migrate" }, { "createIfMissing", false } } },
                { "World", { { "migrationMode", "Migrate" }, { "createIfMissing", false } } }
            } }
        };
        if (!JsonUtils::LoadFromPathOrCreate(gameCache.config, fallback, "Data/config/Database.json"))
            throw std::runtime_error("Failed to load database configuration");

        gameCache.database = std::make_unique<Database::DatabaseService>();
        // The Game Server explicitly owns Auth temporarily because account login still executes here.
        gameCache.database->InitializeBundle(Database::DBType::Auth, ReadBundle(gameCache.config, "Auth"));
        gameCache.database->InitializeBundle(Database::DBType::Character, ReadBundle(gameCache.config, "Character"));
        gameCache.database->InitializeBundle(Database::DBType::World, ReadBundle(gameCache.config, "World"));
        auto worldResult = gameCache.database->LoadWorldCache();
        if (!worldResult)
            throw std::runtime_error("Failed to load the World database cache (" +
                std::string(Database::OperationFailureName(worldResult.Failure())) + ")");

        Database::WorldCacheSnapshot worldSnapshot = std::move(worldResult).Value();
        gameCache.mapTables = std::move(worldSnapshot.maps);
        gameCache.spellTables = std::move(worldSnapshot.spells);
        gameCache.itemTables = std::move(worldSnapshot.items);
        gameCache.creatureTables = std::move(worldSnapshot.creatures);
    }

    void DatabaseSetup::Update(entt::registry&, f32) {}
}
