#include "DatabaseSetup.h"

#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/CurrencyUtils.h>
#include <Server-Common/Database/Util/PermissionUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/JsonUtils.h>
#include <Base/Util/StringUtils.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void DatabaseSetup::Init(entt::registry& registry)
    {
        entt::registry::context& ctx = registry.ctx();

        Singletons::DatabaseState& databaseState = ctx.emplace<Singletons::DatabaseState>();

        // Setup Database Settings
        {
            nlohmann::ordered_json fallback;

            fallback["Connection"] = 
            {
                { "address",  "127.0.0.1" },
                { "port",  5432 },
                { "database",  "postgres" },
                { "user",  "postgres" },
                { "password",  "postgres" }
            };

            if (!JsonUtils::LoadFromPathOrCreate(databaseState.config, fallback, "Data/config/Database.json"))
            {
                NC_LOG_CRITICAL("[Database] : Failed to create config file");
            }
        }

        // Setup DatabaseState
        {
            databaseState.controller = std::make_unique<Database::DBController>();

            nlohmann::json& connection = databaseState.config["Connection"];
            Database::DBEntry dbEntry(connection["address"], connection["port"], connection["database"], connection["user"], connection["password"]);

            databaseState.controller->SetDBEntry(Database::DBType::Auth, dbEntry);
            databaseState.controller->SetDBEntry(Database::DBType::Character, dbEntry);
            databaseState.controller->SetDBEntry(Database::DBType::World, dbEntry);
        }

        auto authConnection = databaseState.controller->GetConnection(Database::DBType::Auth);
        auto characterConnection = databaseState.controller->GetConnection(Database::DBType::Character);
        auto worldConnection = databaseState.controller->GetConnection(Database::DBType::World);

        if (authConnection == nullptr || characterConnection == nullptr || worldConnection == nullptr)
        {
            NC_LOG_CRITICAL("[Database] : Failed to connect to the database");
            return;
        }

        // Load Character Tables
        {
            Database::Util::Permission::InitPermissionTablesPreparedStatements(characterConnection);
            Database::Util::Currency::InitCurrencyTablesPreparedStatements(characterConnection);
            Database::Util::Character::InitCharacterTablesPreparedStatements(characterConnection);

            Database::Util::Permission::LoadPermissionTables(characterConnection, databaseState.permissionTables);
            Database::Util::Currency::LoadCurrencyTables(characterConnection, databaseState.currencyTables);
            Database::Util::Character::LoadCharacterTables(characterConnection, databaseState.characterTables);
        }
    }

    void DatabaseSetup::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
    }
}