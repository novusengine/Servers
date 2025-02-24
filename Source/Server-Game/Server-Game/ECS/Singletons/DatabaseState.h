#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <json/json.hpp>
#include <robinhood/robinhood.h>

namespace Database
{
    class DBController;
}

namespace ECS::Singletons
{
    struct DatabaseState
    {
    public:
        nlohmann::json config;
        std::unique_ptr<Database::DBController> controller;

        Database::PermissionTables permissionTables;
        Database::CurrencyTables currencyTables;
        Database::ItemTables itemTables;
        Database::CharacterTables characterTables;
    };
}