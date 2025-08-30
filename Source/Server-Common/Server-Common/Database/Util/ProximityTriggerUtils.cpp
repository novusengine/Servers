#include "ProximityTriggerUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::ProximityTrigger
{
    namespace Loading
    {
        void InitProximityTriggersTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Proximity Triggers Tables...");

            try
            {
                dbConnection->connection->prepare("ProximityTriggerCreate", "INSERT INTO public.proximity_triggers (name, flags, map_id, position_x, position_y, position_z, extents_x, extents_y, extents_z) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id");
                dbConnection->connection->prepare("ProximityTriggerDelete", "DELETE FROM public.proximity_triggers WHERE id = $1");

                NC_LOG_INFO("Loaded Prepared Statements Proximity Triggers Tables\n");
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_CRITICAL("{0}", e.what());
                return;
            }
        }

        void LoadProximityTriggersTables(std::shared_ptr<DBConnection>& dbConnection, Database::ProximityTriggersTables& proximityTriggersTables)
        {
            NC_LOG_INFO("-- Loading Proximity Triggers Tables --");

            u64 totalRows = 0;
            totalRows += LoadProximityTriggers(dbConnection, proximityTriggersTables);

            NC_LOG_INFO("-- Loaded Proximity Triggers Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadProximityTriggers(std::shared_ptr<DBConnection>& dbConnection, Database::ProximityTriggersTables& proximityTriggersTables)
        {
            NC_LOG_INFO("Loading Table 'proximity_triggers'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.proximity_triggers");
            u64 numRows = result[0][0].as<u64>();

            proximityTriggersTables.triggers.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'proximity_triggers'");
                return 0;
            }

            proximityTriggersTables.triggers.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.proximity_triggers", [&proximityTriggersTables](u32 id, const std::string& name, u16 flags, u16 mapID, f32 positionX, f32 positionY, f32 positionZ, f32 extentsX, f32 extentsY, f32 extentsZ)
            {
                Database::ProximityTrigger trigger = 
                {
                    .id = id,
                    .name = name,
                    .flags = static_cast<Generated::ProximityTriggerFlagEnum>(flags),
                    .mapID = mapID,
                    .position = vec3(positionX, positionY, positionZ),
                    .extents = vec3(extentsX, extentsY, extentsZ)
                };
                proximityTriggersTables.triggers.push_back(trigger);
            });

            NC_LOG_INFO("Loaded Table 'proximity_triggers' ({0} Rows)", numRows);
            return numRows;
        }
    }

    bool ProximityTriggerCreate(pqxx::work& transaction, const std::string& name, u16 flags, u16 mapID, const vec3& position, const vec3& extents, u32& triggerID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("ProximityTriggerCreate"), pqxx::params{ name, flags, mapID, position.x, position.y, position.z, extents.x, extents.y, extents.z });
            if (queryResult.empty())
                return false;

            triggerID = queryResult[0][0].as<u32>();
            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool ProximityTriggerDelete(pqxx::work& transaction, u32 triggerID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("ProximityTriggerDelete"), pqxx::params{ triggerID });
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
}