#include "PermissionUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>

#include <pqxx/nontransaction>

namespace  Database::Util::Permission
{
    void InitPermissionTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
    {
        NC_LOG_INFO("Loading Prepared Statements for Permission Tables...");

        NC_LOG_INFO("Loaded Prepared Statements for Permission Tables\n");
    }

    void LoadPermissionTables(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables)
    {
        NC_LOG_INFO("-- Loading Permission Tables --");

        u64 totalRows = 0;
        totalRows += LoadPermissionsTable(dbConnection, permissionTables);
        totalRows += LoadPermissionGroupsTable(dbConnection, permissionTables);
        totalRows += LoadPermissionGroupDataTable(dbConnection, permissionTables);

        NC_LOG_INFO("-- Loaded Permission Tables ({0} Rows) --\n", totalRows);
    }

    u64 LoadPermissionsTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables)
    {
        NC_LOG_INFO("Loading Table 'permissions'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.permissions");
        u64 numRows = result[0][0].as<u64>();

        permissionTables.idToDefinition.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'permissions'");
            return 0;
        }

        permissionTables.idToDefinition.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.permissions", [&permissionTables](u16 id, const std::string& name)
        {
            ::Database::Permission permission =
            {
                .id = id,
                .name = name
            };

            permissionTables.idToDefinition.insert({ id, permission });
        });

        NC_LOG_INFO("Loaded Table 'permissions' ({0} Rows)", numRows);
        return numRows;
    }
    u64 LoadPermissionGroupsTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables)
    {
        NC_LOG_INFO("Loading Table 'permission_groups'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.permission_groups");
        u64 numRows = result[0][0].as<u64>();

        permissionTables.groupIDToDefinition.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'permission_groups'");
            return 0;
        }

        permissionTables.groupIDToDefinition.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.permission_groups", [&permissionTables](u16 id, const std::string& name)
        {
            ::Database::PermissionGroup permissionGroup =
            {
                .id = id,
                .name = name
            };

            permissionTables.groupIDToDefinition.insert({ id, permissionGroup });
        });

        NC_LOG_INFO("Loaded Table 'permission_groups' ({0} Rows)", numRows);
        return numRows;
    }
    u64 LoadPermissionGroupDataTable(std::shared_ptr<DBConnection>& dbConnection, Database::PermissionTables& permissionTables)
    {
        NC_LOG_INFO("Loading Table 'permission_group_data'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.permission_group_data");
        u64 numRows = result[0][0].as<u64>();

        permissionTables.groupIDToData.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'permission_group_data'");
            return 0;
        }

        permissionTables.groupIDToData.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.permission_group_data", [&permissionTables](u32 id, u16 groupID, u16 permissionID)
        {
            ::Database::PermissionGroupData permissionGroupData =
            {
                .groupID = groupID,
                .permissionID = permissionID
            };

            std::vector<::Database::PermissionGroupData>& permissionGroupDatas = permissionTables.groupIDToData[groupID];
            permissionGroupDatas.push_back(permissionGroupData);
        });

        NC_LOG_INFO("Loaded Table 'permission_group_data' ({0} Rows)", numRows);
        return numRows;
    }
}