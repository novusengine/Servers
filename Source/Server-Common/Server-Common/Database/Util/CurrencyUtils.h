#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

namespace Database
{
    struct DBConnection;

    namespace Util::Currency
    {
        void InitCurrencyTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

        void LoadCurrencyTables(std::shared_ptr<DBConnection>& dbConnection, Database::CurrencyTables& currencyTables);
        u64 LoadCurrencyTable(std::shared_ptr<DBConnection>& dbConnection, Database::CurrencyTables& currencyTables);
    }
}