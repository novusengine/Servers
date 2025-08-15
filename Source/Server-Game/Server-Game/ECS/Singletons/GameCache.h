#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <json/json.hpp>
#include <robinhood/robinhood.h>

namespace Database
{
    class DBController;
}

namespace ECS
{
    namespace Singletons
    {
        struct GameCache
        {
        public:
            nlohmann::json config;
            std::unique_ptr<Database::DBController> dbController;

            Database::PermissionTables permissionTables;
            Database::CurrencyTables currencyTables;
            Database::ItemTables itemTables;
            Database::CharacterTables characterTables;
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