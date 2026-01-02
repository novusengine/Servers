#include "DatabaseSetup.h"

#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/AccountUtils.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/CreatureUtils.h>
#include <Server-Common/Database/Util/CurrencyUtils.h>
#include <Server-Common/Database/Util/ItemUtils.h>
#include <Server-Common/Database/Util/MapUtils.h>
#include <Server-Common/Database/Util/PermissionUtils.h>
#include <Server-Common/Database/Util/ProximityTriggerUtils.h>
#include <Server-Common/Database/Util/SpellUtils.h>

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

            if (!JsonUtils::LoadFromPathOrCreate(gameCache.config, fallback, "Data/Config/Database.json"))
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
            Database::Util::Account::Loading::InitAccountTablesPreparedStatements(characterConnection);
            Database::Util::Map::Loading::InitMapTablesPreparedStatements(characterConnection);
            Database::Util::Permission::Loading::InitPermissionTablesPreparedStatements(characterConnection);
            Database::Util::Currency::Loading::InitCurrencyTablesPreparedStatements(characterConnection);
            Database::Util::Spell::Loading::InitSpellTablesPreparedStatements(characterConnection);
            Database::Util::Item::Loading::InitItemTablesPreparedStatements(characterConnection);
            Database::Util::Character::Loading::InitCharacterTablesPreparedStatements(characterConnection);
            Database::Util::Creature::Loading::InitCreatureTablesPreparedStatements(characterConnection);
            Database::Util::ProximityTrigger::Loading::InitProximityTriggersTablesPreparedStatements(characterConnection);

            Database::Util::Map::Loading::LoadMapTables(characterConnection, gameCache.mapTables);
            Database::Util::Permission::Loading::LoadPermissionTables(characterConnection, gameCache.permissionTables);
            Database::Util::Currency::Loading::LoadCurrencyTables(characterConnection, gameCache.currencyTables);
            Database::Util::Spell::Loading::LoadSpellTables(characterConnection, gameCache.spellTables);
            Database::Util::Item::Loading::LoadItemTables(characterConnection, gameCache.itemTables);
            Database::Util::Character::Loading::LoadCharacterTables(characterConnection, gameCache.characterTables, gameCache.itemTables);
            Database::Util::Creature::Loading::LoadCreatureTables(characterConnection, gameCache.creatureTables);
        }

        // Fix Sequence Keys in Public Scehma
        {
            pqxx::nontransaction nonTransaction = characterConnection->NewNonTransaction();
            nonTransaction.exec("SELECT fix_all_sequences('public')");
        }
    }

    void DatabaseSetup::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
    }
}
