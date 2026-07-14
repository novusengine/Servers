#pragma once
#include "Server-Common/Database/Definitions.h"
#include "Server-Common/Database/ReliableExecution.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/Auth/Tables/Accounts.h>

#include <pqxx/pqxx>

namespace Database
{
    namespace Detail::Account
    {
        OperationResult<std::optional<MetaGen::Postgres::Auth::AccountsRecord>> AccountGetInfoByName(DBController& dbController, const std::string& name);
        OperationResult<PermissionAssignmentSnapshot> AccountGetPermissions(DBController& dbController, u64 accountID);
        OperationResult<PermissionTables> LoadPermissionTables(DBController& dbController);
        bool SynchronizePermissionTables(pqxx::work& transaction);
        MetaGen::Postgres::Auth::AccountsRecord AccountCreate(pqxx::work& transaction, const std::string& name, const std::string& email, u64 registrationTimestamp, unsigned char* blob, u32 blobSize);
    }
}
