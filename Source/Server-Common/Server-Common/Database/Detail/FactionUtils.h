#pragma once

#include "Server-Common/Database/Definitions.h"

#include <pqxx/pqxx>

namespace Database::Detail::Faction::Loading
{
    void LoadFactionTables(pqxx::read_transaction& transaction, FactionTables& factionTables);
}
