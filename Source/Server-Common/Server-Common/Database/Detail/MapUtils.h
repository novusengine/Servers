#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/World/Tables/MapLocations.h>

#include <pqxx/pqxx>

namespace Database
{
    namespace Detail::Map
    {
        namespace Loading
        {
            void LoadMapTables(pqxx::read_transaction& transaction, Database::MapTables& mapTables);
            u64 LoadMaps(pqxx::read_transaction& transaction, Database::MapTables& mapTables);
            u64 LoadMapLocations(pqxx::read_transaction& transaction, Database::MapTables& mapTables);
        }

        MetaGen::Postgres::World::MapLocationsRecord MapLocationCreate(pqxx::work& transaction, const std::string& name, u32 mapID, const vec3& position, f32 orientation);
        bool MapLocationDelete(pqxx::work& transaction, u32 locationID);
    }
}
