#include "CurrencyUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>

#include <pqxx/nontransaction>

namespace Database::Util::Currency
{
    namespace Loading
    {
        void InitCurrencyTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Currency Tables...");

            NC_LOG_INFO("Loaded Currency Tables Prepared Statements\n");
        }

        void LoadCurrencyTables(std::shared_ptr<DBConnection>& dbConnection, Database::CurrencyTables& currencyTables)
        {
            NC_LOG_INFO("-- Loading Currency Tables --");

            u64 totalRows = 0;
            totalRows += LoadCurrencyTable(dbConnection, currencyTables);

            NC_LOG_INFO("-- Loaded Currency Tables ({0} Rows) --\n", totalRows);
        }

        u64 LoadCurrencyTable(std::shared_ptr<DBConnection>& dbConnection, Database::CurrencyTables& currencyTables)
        {
            NC_LOG_INFO("Loading Table 'currency'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.currency");
            u64 numRows = result[0][0].as<u64>();

            currencyTables.idToDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'currency'");
                return 0;
            }

            currencyTables.idToDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.currency", [&currencyTables](u16 id, const std::string& name)
            {
                ::Database::Currency currency =
                {
                    .id = id,
                    .name = name
                };

                currencyTables.idToDefinition.insert({ id, currency });
            });

            NC_LOG_INFO("Loaded Table 'currency' ({0} Rows)", numRows);
            return numRows;
        }
    }
}