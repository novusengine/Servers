#include "MapUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::Map
{
    namespace Loading
    {
        void InitMapTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Map Tables...");

            try
            {
                dbConnection->connection->prepare("MapLocationCreate", "INSERT INTO public.map_locations (name, map_id, position_x, position_y, position_z, position_o) VALUES ($1, $2, $3, $4, $5, $6) RETURNING id");
                dbConnection->connection->prepare("MapLocationDelete", "DELETE FROM public.map_locations WHERE id = $1");

                dbConnection->connection->prepare("SetMap", "INSERT INTO public.maps (id, flags, name, type, max_players) VALUES ($1, $2, $3, $4, $5) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, flags = EXCLUDED.flags, name = EXCLUDED.name, type = EXCLUDED.type, max_players = EXCLUDED.max_players");
                dbConnection->connection->prepare("SetMapLocation", "INSERT INTO public.map_locations (id, name, map_id, position_x, position_y, position_z, position_o) VALUES ($1, $2, $3, $4, $5, $6, $7) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, name = EXCLUDED.name, map_id = EXCLUDED.map_id, position_x = EXCLUDED.position_x, position_y = EXCLUDED.position_y, position_z = EXCLUDED.position_z, position_o = EXCLUDED.position_o");

                NC_LOG_INFO("Loaded Prepared Statements Map Tables\n");
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_CRITICAL("{0}", e.what());
                return;
            }
        }

        void LoadMapTables(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("-- Loading Map Tables --");

            u64 totalRows = 0;
            totalRows += LoadMaps(dbConnection, mapTables);
            totalRows += LoadMapLocations(dbConnection, mapTables);

            NC_LOG_INFO("-- Loaded Map Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadMaps(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("Loading Table 'maps'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.maps");
            u64 numRows = result[0][0].as<u64>();

            mapTables.idToDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'maps'");
                return 0;
            }

            mapTables.idToDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.maps", [&mapTables](u32 id, u32 flags, const std::string& name, u16 type, u16 maxPlayers)
            {
                auto& templateData = mapTables.idToDefinition[id];

                templateData.id = id;
                templateData.flags = flags;

                templateData.name = name;

                templateData.type = type;
                templateData.maxPlayers = maxPlayers;
            });

            NC_LOG_INFO("Loaded Table 'maps' ({0} Rows)", numRows);
            return numRows;
        }
        u64 LoadMapLocations(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("Loading Table 'map_locations'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.map_locations");
            u64 numRows = result[0][0].as<u64>();

            mapTables.locationIDToDefinition.clear();
            mapTables.locationNameHashToID.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'map_locations'");
                return 0;
            }

            mapTables.locationIDToDefinition.reserve(numRows);
            mapTables.locationNameHashToID.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.map_locations", [&mapTables](u32 id, const std::string& name, u16 mapID, f32 positionX, f32 positionY, f32 positionZ, f32 orientation)
            {
                std::string locationName = name;
                StringUtils::ToLower(locationName);
                u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

                if (mapTables.locationNameHashToID.contains(locationNameHash))
                {
                    NC_LOG_ERROR("- Skipped duplicate map location: '{0}'", name);
                    return;
                }

                auto& locationData = mapTables.locationIDToDefinition[id];

                locationData.id = id;
                locationData.name = name;

                locationData.mapID = mapID;
                locationData.positionX = positionX;
                locationData.positionY = positionY;
                locationData.positionZ = positionZ;
                locationData.orientation = orientation;

                mapTables.locationNameHashToID[locationNameHash] = id;
            });

            u32 numLoadedLocations = static_cast<u32>(mapTables.locationNameHashToID.size());
            NC_LOG_INFO("Loaded Table 'map_locations' ({0} Rows)", numLoadedLocations);
            return numRows;
        }
    }
    bool MapLocationCreate(pqxx::work& transaction, const std::string& name, u16 mapID, const vec3& position, f32 orientation, u32& locationID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("MapLocationCreate"), pqxx::params{ name, mapID, position.x, position.y, position.z, orientation });
            if (queryResult.empty())
                return false;

            locationID = queryResult[0][0].as<u32>();
            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
    bool MapLocationDelete(pqxx::work& transaction, u32 locationID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("MapLocationDelete"), pqxx::params{ locationID });
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