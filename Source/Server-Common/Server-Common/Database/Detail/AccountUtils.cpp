#include "AccountUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Postgres/Auth/Tables/Accounts.h>

namespace Database::Detail::Account
{
    OperationResult<std::optional<MetaGen::Postgres::Auth::AccountsRecord>> AccountGetInfoByName(DBController& dbController, const std::string& name)
    {
        return RunRead(dbController, DBType::Auth, "get_account_by_name", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::ByName>(transaction, name);
        });
    }

    MetaGen::Postgres::Auth::AccountsRecord AccountCreate(pqxx::work& transaction, const std::string& name, const std::string& email, u64 registrationTimestamp, unsigned char* blob, u32 blobSize)
    {
        auto binaryBlob = Bytebuffer::CreateReadOnlyView(blob, blobSize);
        return Generated::Execute<MetaGen::Postgres::Auth::AccountsTable::Insert>(transaction,
            u64{ 0 }, name, email, registrationTimestamp, u64{ 0 }, std::optional<Bytebuffer>{ std::move(binaryBlob) });
    }

}
