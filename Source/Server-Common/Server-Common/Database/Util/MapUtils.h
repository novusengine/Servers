#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::Map
    {
        namespace Loading
        {
            void InitMapTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

            void LoadMapTables(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables);
            u64 LoadMaps(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables);
            u64 LoadMapLocations(std::shared_ptr<DBConnection>& dbConnection, Database::MapTables& mapTables);
        }

        bool MapLocationCreate(pqxx::work& transaction, const std::string& name, u16 mapID, const vec3& position, f32 orientation, u32& locationID);
        bool MapLocationDelete(pqxx::work& transaction, u32 locationID);
    }
}