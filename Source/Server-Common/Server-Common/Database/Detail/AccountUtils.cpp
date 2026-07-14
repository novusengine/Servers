#include "AccountUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Postgres/Auth/Tables/Accounts.h>
#include <MetaGen/Postgres/Auth/Tables/AccountPermissionGroups.h>
#include <MetaGen/Postgres/Auth/Tables/AccountPermissions.h>
#include <MetaGen/Postgres/Auth/Tables/PermissionGroupData.h>
#include <MetaGen/Postgres/Auth/Tables/PermissionGroups.h>
#include <MetaGen/Postgres/Auth/Tables/Permissions.h>
#include <MetaGen/Server/Permission/Permission.h>

#include <string>

namespace Database::Detail::Account
{
    OperationResult<std::optional<MetaGen::Postgres::Auth::AccountsRecord>> AccountGetInfoByName(DBController& dbController, const std::string& name)
    {
        return RunRead(dbController, DBType::Auth, "get_account_by_name", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::ByName>(transaction, name);
        });
    }

    OperationResult<PermissionAssignmentSnapshot> AccountGetPermissions(DBController& dbController, u64 accountID)
    {
        return RunRead(dbController, DBType::Auth, "get_account_permissions", [&](pqxx::read_transaction& transaction)
        {
            PermissionAssignmentSnapshot snapshot;
            auto permissions = Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionsTable::ByAccount>(transaction, accountID);
            snapshot.permissions.reserve(permissions.size());
            for (const auto& permission : permissions)
                snapshot.permissions.push_back({ permission.permissionId, permission.value });

            auto groups = Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionGroupsTable::ByAccount>(transaction, accountID);
            snapshot.permissionGroupIDs.reserve(groups.size());
            for (const auto& group : groups)
                snapshot.permissionGroupIDs.push_back(group.permissionGroupId);
            if (snapshot.permissionGroupIDs.empty())
            {
                snapshot.permissionGroupIDs.push_back(static_cast<u32>(
                    MetaGen::Server::Permission::DEFAULT_ACCOUNT_GROUP));
                snapshot.defaultGroupApplied = true;
            }
            return snapshot;
        });
    }

    OperationResult<PermissionTables> LoadPermissionTables(DBController& dbController)
    {
        return RunRead(dbController, DBType::Auth, "load_permission_tables", [&](pqxx::read_transaction& transaction)
        {
            PermissionTables tables;
            Generated::ForEach<MetaGen::Postgres::Auth::PermissionsTable>(transaction, [&](auto permission)
            {
                tables.permissionNameToID.insert_or_assign(permission.name, permission.id);
                tables.permissionIDToValueKind.insert_or_assign(permission.id, permission.valueKind);
                tables.permissionIDToDefaultValue.insert_or_assign(permission.id, permission.defaultValue);
                tables.permissionIDToName.insert_or_assign(permission.id, std::move(permission.name));
            });
            Generated::ForEach<MetaGen::Postgres::Auth::PermissionGroupsTable>(transaction, [&](auto group)
            {
                tables.permissionGroupNameToID.insert_or_assign(group.name, group.id);
                tables.permissionGroupIDToName.insert_or_assign(group.id, std::move(group.name));
            });
            Generated::ForEach<MetaGen::Postgres::Auth::PermissionGroupDataTable>(transaction, [&](const auto& groupPermission)
            {
                tables.permissionGroupIDToValues[groupPermission.groupId].insert_or_assign(groupPermission.permissionId, groupPermission.value);
            });
            return tables;
        });
    }

    bool SynchronizePermissionTables(pqxx::work& transaction)
    {
        using namespace MetaGen::Server::Permission;

        for (const PermissionDescriptor& permission : PERMISSIONS)
        {
            Generated::Execute<MetaGen::Postgres::Auth::PermissionsTable::Set>(transaction,
                static_cast<u32>(permission.id), std::string(permission.name), std::string(permission.category),
                static_cast<u16>(permission.valueKind), std::string(permission.description), permission.defaultValue);
        }

        for (const PermissionGroupDescriptor& group : PERMISSION_GROUPS)
        {
            const u32 groupID = static_cast<u32>(group.id);
            Generated::Execute<MetaGen::Postgres::Auth::PermissionGroupsTable::Set>(transaction,
                groupID, std::string(group.name), std::string(group.description), true);
            Generated::Execute<MetaGen::Postgres::Auth::PermissionGroupDataTable::DeleteByGroup>(transaction, groupID);
            for (u32 index = 0; index < group.numPermissions; ++index)
            {
                const PermissionGroupEntry& entry = group.permissions[index];
                Generated::Execute<MetaGen::Postgres::Auth::PermissionGroupDataTable::Set>(transaction,
                    groupID, static_cast<u32>(entry.permission), entry.value);
            }
        }
        return true;
    }

    MetaGen::Postgres::Auth::AccountsRecord AccountCreate(pqxx::work& transaction, const std::string& name, const std::string& email, u64 registrationTimestamp, unsigned char* blob, u32 blobSize)
    {
        auto binaryBlob = Bytebuffer::CreateReadOnlyView(blob, blobSize);
        return Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::Insert>(transaction,
            u64{ 0 }, name, email, registrationTimestamp, u64{ 0 }, std::optional<Bytebuffer>{ std::move(binaryBlob) });
    }

}
