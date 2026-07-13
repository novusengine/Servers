#pragma once
#include "Server-Common/Database/Definitions.h"
#include "Server-Common/Database/ReliableExecution.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/World/Tables/ProximityTriggers.h>

#include <pqxx/pqxx>
#include <vector>

namespace Database
{
    namespace Detail::ProximityTrigger
    {
        MetaGen::Postgres::World::ProximityTriggersRecord ProximityTriggerCreate(pqxx::work& transaction, const std::string& name, u16 flags, u32 mapID, const vec3& position, const vec3& extents);
        bool ProximityTriggerDelete(pqxx::work& transaction, u32 triggerID);
        OperationResult<std::vector<MetaGen::Postgres::World::ProximityTriggersRecord>> ProximityTriggerGetInfoByMap(DBController& dbController, u32 mapID);
    }
}
