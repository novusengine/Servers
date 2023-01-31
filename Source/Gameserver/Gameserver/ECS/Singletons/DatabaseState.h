#pragma once
#include <Base/Types.h>

#include <json/json.hpp>
#include <robinhood/robinhood.h>

namespace Server::Database
{
	class DBController;

	struct CharacterDefinition
	{
		u32 id = std::numeric_limits<u32>().max();
		std::string name = "";
	};
}
namespace ECS::Singletons
{
	struct DatabaseState
	{
	public:
		nlohmann::json config;
		std::unique_ptr<Server::Database::DBController> controller;

		robin_hood::unordered_map<u32, Server::Database::CharacterDefinition> characterIDToDefinition;
		robin_hood::unordered_map<std::string, Server::Database::CharacterDefinition> characterNameToDefinition;
	};
}