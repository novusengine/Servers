#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/Character/Tables/ItemInstances.h>

#include <pqxx/pqxx>

namespace Database
{
    namespace Detail::Item
    {
        namespace Loading
        {
            void LoadItemTables(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
            u64 LoadItemTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
            u64 LoadItemStatTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
            u64 LoadItemArmorTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
            u64 LoadItemShieldTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
            u64 LoadItemWeaponTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables);
        }

        MetaGen::Postgres::Character::ItemInstancesRecord ItemInstanceCreate(pqxx::work& work, u32 itemID, u64 ownerID, u16 count, u16 durability);
        bool ItemInstanceDestroy(pqxx::work& work, u64 itemInstanceID);
    }
}
