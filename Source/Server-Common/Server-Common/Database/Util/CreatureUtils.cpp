#include "CreatureUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::Creature
{
    namespace Loading
    {
        void InitCreatureTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Creature Tables...");

            try
            {
                dbConnection->connection->prepare("CreatureCreate", "INSERT INTO public.creatures (template_id, display_id, map_id, position_x, position_y, position_z, position_o) VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id");
                dbConnection->connection->prepare("CreatureDelete", "DELETE FROM public.creatures WHERE id = $1");
                dbConnection->connection->prepare("CreatureGetInfoByMap", "SELECT * FROM public.creatures WHERE map_id = $1");

                NC_LOG_INFO("Loaded Prepared Statements Creature Tables\n");
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_CRITICAL("{0}", e.what());
                return;
            }
        }

        void LoadCreatureTables(std::shared_ptr<DBConnection>& dbConnection, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("-- Loading Creature Tables --");

            u64 totalRows = 0;
            totalRows += LoadCreatureTemplates(dbConnection, creatureTables);

            NC_LOG_INFO("-- Loaded Creature Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadCreatureTemplates(std::shared_ptr<DBConnection>& dbConnection, Database::CreatureTables& creatureTables)
        {
            NC_LOG_INFO("Loading Table 'creature_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.creature_templates");
            u64 numRows = result[0][0].as<u64>();

            creatureTables.templateIDToDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'creature_template'");
                return 0;
            }

            creatureTables.templateIDToDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.creature_templates", [&creatureTables](u32 id, u32 flags, const std::string& name, const std::string& subname, u32 displayID, u16 minLevel, u16 maxLevel, f32 scale, f32 armorMod, f32 healthMod, f32 resourceMod, f32 damageMod)
            {
                auto& templateData = creatureTables.templateIDToDefinition[id];

                templateData.id = id;
                templateData.flags = flags;

                templateData.name = name;
                templateData.subname = subname;

                templateData.displayID = displayID;
                templateData.scale = scale;

                templateData.minLevel = minLevel;
                templateData.maxLevel = maxLevel;
                templateData.armorMod = armorMod;
                templateData.healthMod = healthMod;
                templateData.resourceMod = resourceMod;
                templateData.damageMod = damageMod;
            });

            NC_LOG_INFO("Loaded Table 'creature_template' ({0} Rows)", numRows);
            return numRows;
        }
    }

    bool CreatureCreate(pqxx::work& transaction, u32 templateID, u32 displayID, u16 mapID, const vec3& position, f32 orientation, u64& creatureID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CreatureCreate"), pqxx::params{ templateID, displayID, mapID, position.x, position.y, position.z, orientation });
            if (queryResult.empty())
                return false;

            creatureID = queryResult[0][0].as<u64>();
            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
    bool CreatureDelete(pqxx::work& transaction, u64 creatureID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CreatureDelete"), pqxx::params{ creatureID });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
    bool CreatureGetInfoByMap(std::shared_ptr<DBConnection>& dbConnection, u16 mapID, pqxx::result& result)
    {
        try
        {
            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();
            result = nonTransaction.exec(pqxx::prepped("CreatureGetInfoByMap"), pqxx::params{ mapID });
            if (result.empty())
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
}