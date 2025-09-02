#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::ProximityTrigger
    {
        namespace Loading
        {
            void InitProximityTriggersTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);
        }

        bool ProximityTriggerCreate(pqxx::work& transaction, const std::string& name, u16 flags, u16 mapID, const vec3& position, const vec3& extents, u32& triggerID);
        bool ProximityTriggerDelete(pqxx::work& transaction, u32 triggerID);
        bool ProximityTriggerGetInfoByMap(std::shared_ptr<DBConnection>& dbConnection, u16 mapID, pqxx::result& result);
    }
}