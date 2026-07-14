#include "CreatureUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Postgres/World/Tables/CreatureClassLevelStats.h>
#include <MetaGen/Postgres/World/Tables/CreatureTemplates.h>
#include <MetaGen/Postgres/World/Tables/Creatures.h>
namespace Database::Detail::Creature
{
    namespace Loading
    {
        void LoadCreatureTables(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("-- Loading Creature Tables --");

            u64 totalRows = 0;
            totalRows += LoadCreatureClassLevelStats(transaction, creatureTables);
            totalRows += LoadCreatureTemplates(transaction, creatureTables);

            NC_LOG_INFO("-- Loaded Creature Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadCreatureClassLevelStats(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("Loading Table 'creature_class_level_stats'");

            creatureTables.classLevelKeyToStats.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::CreatureClassLevelStatsTable>(transaction, [&creatureTables, &numRows](auto record)
            {
                const u32 key = MakeCreatureClassLevelKey(record.unitClass, record.level);
                creatureTables.classLevelKeyToStats.insert_or_assign(key, std::move(record));
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'creature_class_level_stats' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadCreatureTemplates(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("Loading Table 'creature_template'");

            creatureTables.templateIDToDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::CreatureTemplatesTable>(transaction, [&creatureTables, &numRows](auto record)
            {
                creatureTables.templateIDToDefinition.insert_or_assign(record.id, std::move(record));
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'creature_template' ({0} Rows)", numRows);
            return numRows;
        }
    }

    MetaGen::Postgres::World::CreaturesRecord CreatureCreate(pqxx::work& transaction, u32 templateID, u32 displayID, u32 mapID,
        const vec3& position, f32 orientation, const std::string& scriptName, u32 spawnTimeInSecMin,
        u32 spawnTimeInSecMax, f32 wanderDistance, u16 movementType)
    {
        return Generated::Execute<MetaGen::Postgres::World::CreaturesTable::Insert>(transaction,
            templateID, displayID, mapID, position.x, position.y, position.z, orientation, scriptName,
            spawnTimeInSecMin, spawnTimeInSecMax, wanderDistance, movementType);
    }
    bool CreatureDelete(pqxx::work& transaction, u64 creatureID)
    {
        return Generated::Execute<MetaGen::Postgres::World::CreaturesTable::Delete>(transaction, creatureID).affected_rows() != 0;
    }
    OperationResult<std::vector<MetaGen::Postgres::World::CreaturesRecord>> CreatureGetInfoByMap(DBController& dbController, u32 mapID)
    {
        return RunRead(dbController, DBType::World, "get_creatures_by_map", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::World::CreaturesTable::ByMap>(transaction, mapID);
        });
    }
}
