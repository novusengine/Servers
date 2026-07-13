#include "ProximityTriggerUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>

#include <MetaGen/Postgres/World/Tables/ProximityTriggers.h>
#include <Base/Util/StringUtils.h>

namespace Database::Detail::ProximityTrigger
{
    MetaGen::Postgres::World::ProximityTriggersRecord ProximityTriggerCreate(pqxx::work& transaction, const std::string& name, u16 flags, u32 mapID, const vec3& position, const vec3& extents)
    {
        return Generated::Execute<MetaGen::Postgres::World::ProximityTriggersTable::Insert>(transaction,
            name, flags, mapID, position.x, position.y, position.z, extents.x, extents.y, extents.z);
    }

    bool ProximityTriggerDelete(pqxx::work& transaction, u32 triggerID)
    {
        return Generated::Execute<MetaGen::Postgres::World::ProximityTriggersTable::Delete>(transaction, triggerID).affected_rows() != 0;
    }
    OperationResult<std::vector<MetaGen::Postgres::World::ProximityTriggersRecord>> ProximityTriggerGetInfoByMap(DBController& dbController, u32 mapID)
    {
        return RunRead(dbController, DBType::World, "get_proximity_triggers_by_map", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::World::ProximityTriggersTable::ByMap>(transaction, mapID);
        });
    }
}
