#include "CreatureUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

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
            totalRows += LoadCreatureTemplates(transaction, creatureTables);

            NC_LOG_INFO("-- Loaded Creature Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadCreatureTemplates(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("Loading Table 'creature_template'");

            creatureTables.templateIDToDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::CreatureTemplatesTable>(transaction, [&creatureTables, &numRows](auto record)
            {
                creatureTables.templateIDToDefinition[record.id] = {
                    record.id, record.flags, std::move(record.name), std::move(record.subname), record.displayId,
                    record.scale, record.minLevel, record.maxLevel, record.armorMod, record.healthMod,
                    record.resourceMod, record.damageMod, std::move(record.scriptName)
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'creature_template' ({0} Rows)", numRows);
            return numRows;
        }
    }

    MetaGen::Postgres::World::CreaturesRecord CreatureCreate(pqxx::work& transaction, u32 templateID, u32 displayID, u32 mapID, const vec3& position, f32 orientation, const std::string& scriptName)
    {
        return Generated::Execute<MetaGen::Postgres::World::CreaturesTable::Insert>(transaction,
            templateID, displayID, mapID, position.x, position.y, position.z, orientation, scriptName);
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
