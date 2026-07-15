#include "FactionUtils.h"

#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>

namespace Database::Detail::Faction::Loading
{
    void LoadFactionTables(pqxx::read_transaction& transaction, FactionTables& factionTables)
    {
        NC_LOG_INFO("-- Loading Faction Tables --");

        factionTables.definitions = Generated::LoadAll<MetaGen::Postgres::World::FactionsTable>(transaction);
        factionTables.relations = Generated::LoadAll<MetaGen::Postgres::World::FactionRelationsTable>(transaction);
        factionTables.standings = Generated::LoadAll<MetaGen::Postgres::World::FactionStandingsTable>(transaction);
        factionTables.startingReputations = Generated::LoadAll<MetaGen::Postgres::World::FactionStartingReputationsTable>(transaction);
        factionTables.unitRaceFactions = Generated::LoadAll<MetaGen::Postgres::World::UnitRaceFactionsTable>(transaction);

        const size_t totalRows = factionTables.definitions.size() + factionTables.relations.size() + factionTables.standings.size() + factionTables.startingReputations.size() + factionTables.unitRaceFactions.size();
        NC_LOG_INFO("-- Loaded Faction Tables ({0} rows) --\n", totalRows);
    }
}
