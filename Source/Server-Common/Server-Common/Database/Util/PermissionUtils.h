#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

namespace Database
{
    struct DBConnection;

    namespace Util::Permission
    {
        void InitPermissionTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

        void LoadPermissionTables(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables);
        u64 LoadPermissionsTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables);
        u64 LoadPermissionGroupsTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables);
        u64 LoadPermissionGroupDataTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables);
    }
}