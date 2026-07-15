#pragma once
#include "Server-Game/Gameplay/Faction/FactionRuntimeData.h"
#include "Server-Common/Database/Definitions.h"
#include "Server-Common/Database/DatabaseService.h"

#include <Base/Types.h>

#include <json/json.hpp>
#include <robinhood/robinhood.h>

#include <memory>

namespace ECS
{
    namespace Singletons
    {
        struct GameCache
        {
        public:
            nlohmann::json config;
            std::unique_ptr<Database::DatabaseService> database;

            Database::MapTables mapTables;
            Database::SpellTables spellTables;
            Database::ItemTables itemTables;
            Database::CharacterTables characterTables;
            Database::CreatureTables creatureTables;
            Database::FactionTables factionTables;
            std::shared_ptr<const Gameplay::Faction::FactionRuntimeData> factionRuntimeData;
            Database::PermissionTables permissionTables;
        };
    }

    enum class Result
    {
        Success,
        CommandPushed,
        GenericError,
        DatabaseError,
        DatabaseNotConnected,
        CharacterNotFound,
        CharacterAlreadyExists,
        ItemNotFound,
        ItemAlreadyExists,
    };
}
