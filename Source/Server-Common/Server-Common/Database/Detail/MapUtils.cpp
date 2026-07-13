#include "MapUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Postgres/World/Tables/MapLocations.h>
#include <MetaGen/Postgres/World/Tables/Maps.h>

namespace Database::Detail::Map
{
    namespace Loading
    {
        void LoadMapTables(pqxx::read_transaction& transaction, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("-- Loading Map Tables --");

            u64 totalRows = 0;
            totalRows += LoadMaps(transaction, mapTables);
            totalRows += LoadMapLocations(transaction, mapTables);

            NC_LOG_INFO("-- Loaded Map Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadMaps(pqxx::read_transaction& transaction, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("Loading Table 'maps'");

            mapTables.idToDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::MapsTable>(transaction, [&mapTables, &numRows](auto record)
            {
                mapTables.idToDefinition[record.id] = { record.id, record.flags, std::move(record.internalName),
                    std::move(record.name), record.type, record.maxPlayers };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'maps' ({0} Rows)", numRows);
            return numRows;
        }
        u64 LoadMapLocations(pqxx::read_transaction& transaction, Database::MapTables& mapTables)
        {
            NC_LOG_INFO("Loading Table 'map_locations'");

            mapTables.locationIDToDefinition.clear();
            mapTables.locationNameHashToID.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::MapLocationsTable>(transaction, [&mapTables, &numRows](auto record)
            {
                std::string locationName = record.name;
                StringUtils::ToLower(locationName);
                u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

                if (mapTables.locationNameHashToID.contains(locationNameHash))
                {
                    NC_LOG_ERROR("- Skipped duplicate map location: '{0}'", record.name);
                    return;
                }
                mapTables.locationIDToDefinition[record.id] = { record.id, std::move(record.name), record.mapId,
                    record.positionX, record.positionY, record.positionZ, record.positionO };
                mapTables.locationNameHashToID[locationNameHash] = record.id;
                ++numRows;
            });

            u32 numLoadedLocations = static_cast<u32>(mapTables.locationNameHashToID.size());
            NC_LOG_INFO("Loaded Table 'map_locations' ({0} Rows)", numLoadedLocations);
            return numLoadedLocations;
        }
    }
    MetaGen::Postgres::World::MapLocationsRecord MapLocationCreate(pqxx::work& transaction, const std::string& name, u32 mapID, const vec3& position, f32 orientation)
    {
        return Generated::Execute<MetaGen::Postgres::World::MapLocationsTable::Insert>(transaction,
            name, mapID, position.x, position.y, position.z, orientation);
    }
    bool MapLocationDelete(pqxx::work& transaction, u32 locationID)
    {
        return Generated::Execute<MetaGen::Postgres::World::MapLocationsTable::Delete>(transaction, locationID).affected_rows() != 0;
    }
}
