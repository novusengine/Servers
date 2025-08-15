#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::Item
    {
        namespace Loading
        {
            void InitItemTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

            void LoadItemTables(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
            u64 LoadItemTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
            u64 LoadItemStatTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
            u64 LoadItemArmorTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
            u64 LoadItemWeaponTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
            u64 LoadItemShieldTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables);
        }

        bool ItemInstanceCreate(pqxx::work& work, u32 itemID, u64 ownerID, u16 count, u16 durability, u64& itemInstanceID);
        bool ItemInstanceDestroy(pqxx::work& work, u64 itemInstanceID);
    }
}