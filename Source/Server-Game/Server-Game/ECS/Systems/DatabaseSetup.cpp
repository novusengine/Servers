#include "DatabaseSetup.h"

#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/CurrencyUtils.h>
#include <Server-Common/Database/Util/ItemUtils.h>
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

        auto& gameCache = ctx.emplace<Singletons::GameCache>();

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

            if (!JsonUtils::LoadFromPathOrCreate(gameCache.config, fallback, "Data/config/Database.json"))
            {
                NC_LOG_CRITICAL("[Database] : Failed to create config file");
            }
        }

        // Setup GameCache Database Controller
        {
            gameCache.dbController = std::make_unique<Database::DBController>();

            nlohmann::json& connection = gameCache.config["Connection"];
            Database::DBEntry dbEntry(connection["address"], connection["port"], connection["database"], connection["user"], connection["password"]);

            gameCache.dbController->SetDBEntry(Database::DBType::Auth, dbEntry);
            gameCache.dbController->SetDBEntry(Database::DBType::Character, dbEntry);
            gameCache.dbController->SetDBEntry(Database::DBType::World, dbEntry);
        }

        auto authConnection = gameCache.dbController->GetConnection(Database::DBType::Auth);
        auto characterConnection = gameCache.dbController->GetConnection(Database::DBType::Character);
        auto worldConnection = gameCache.dbController->GetConnection(Database::DBType::World);

        if (authConnection == nullptr || characterConnection == nullptr || worldConnection == nullptr)
        {
            NC_LOG_CRITICAL("[Database] : Failed to connect to the database");
            return;
        }

        // Load Character Tables
        {
            Database::Util::Permission::InitPermissionTablesPreparedStatements(characterConnection);
            Database::Util::Currency::InitCurrencyTablesPreparedStatements(characterConnection);
            Database::Util::Item::Loading::InitItemTablesPreparedStatements(characterConnection);
            Database::Util::Character::Loading::InitCharacterTablesPreparedStatements(characterConnection);

            Database::Util::Permission::LoadPermissionTables(characterConnection, gameCache.permissionTables);
            Database::Util::Currency::LoadCurrencyTables(characterConnection, gameCache.currencyTables);
            Database::Util::Item::Loading::LoadItemTables(characterConnection, gameCache.itemTables);
            Database::Util::Character::Loading::LoadCharacterTables(characterConnection, gameCache.characterTables, gameCache.itemTables);
        }
    }

    void DatabaseSetup::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
    }
}