#include "DatabaseSetup.h"

#include "Gameserver/ECS/Singletons/DatabaseState.h"
#include "Gameserver/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/JsonUtils.h>

#include <Server/Database/DBController.h>

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

			fallback["Connection"] = {
				{ "address",  "127.0.0.1" },
				{ "port",  5432 },
				{ "database",  "postgres" },
				{ "user",  "postgres" },
				{ "password",  "postgres" }
			};

			if (!JsonUtils::LoadFromPathOrCreate(databaseState.config, fallback, "Data/config/Database.json"))
			{
				DebugHandler::PrintFatal("[Database] : Failed to create config file");
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
			auto conn = databaseState.controller->GetConnection(Server::Database::DBType::Character);

			if (conn)
			{
				for (auto [id, name] : conn->Context().query<i32, std::string>(
					"SELECT id, name FROM characters ORDER BY id"))
				{
					Server::Database::CharacterDefinition character;
					character.id = id;
					character.name = name;

					databaseState.characterIDToDefinition[id] = character;
					databaseState.characterNameToDefinition[name] = character;
				}
			}
		}
	}

	void DatabaseSetup::Update(entt::registry& registry, f32 deltaTime)
	{
		entt::registry::context& ctx = registry.ctx();
	}
}