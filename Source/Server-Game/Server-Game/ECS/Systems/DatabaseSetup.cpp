#include "DatabaseSetup.h"

#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/JsonUtils.h>

#include <Server-Common/Database/DBController.h>

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
			databaseState.controller = std::make_unique<Server::Database::DBController>();

			nlohmann::json& connection = databaseState.config["Connection"];
			Server::Database::DBEntry dbEntry(connection["address"], connection["port"], connection["database"], connection["user"], connection["password"]);

			databaseState.controller->SetDBEntry(Server::Database::DBType::Auth, dbEntry);
			databaseState.controller->SetDBEntry(Server::Database::DBType::Character, dbEntry);
			databaseState.controller->SetDBEntry(Server::Database::DBType::World, dbEntry);
		}

		// Load Character Table
		{

			if (auto conn = databaseState.controller->GetConnection(Server::Database::DBType::Character))
			{
				conn->connection->prepare("CreateCharacter", "INSERT INTO characters (name, permissionlevel) VALUES ($1, $2) RETURNING id");
				conn->connection->prepare("DeleteCharacter", "DELETE FROM characters WHERE id = $1");

				for (auto& [id, name, permissionLevel] : conn->Context().query<u32, std::string, u16>("SELECT id, name, permissionlevel FROM characters ORDER BY id"))
				{
					Server::Database::CharacterDefinition character =
					{
						.id = id,
						.name = name,
						.permissionLevel = permissionLevel
					};

					databaseState.characterIDToDefinition.insert({ id, character });
					databaseState.characterNameToDefinition.insert({ name, character });
				}
			}
		}
	}

	void DatabaseSetup::Update(entt::registry& registry, f32 deltaTime)
	{
		entt::registry::context& ctx = registry.ctx();
	}
}