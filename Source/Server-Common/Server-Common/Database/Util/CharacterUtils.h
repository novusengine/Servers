#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::Character
    {
        namespace Loading
        {
            void InitCharacterTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

            void LoadCharacterTables(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables, Database::ItemTables& itemTables);
            u64 LoadCharacterData(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterPermissions(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterPermissionGroups(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterCurrency(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterItems(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables, Database::ItemTables& itemTables);
        }

        bool CharacterCreate(pqxx::work& transaction, const std::string& name, u16 racegenderclass, u64& characterID);
        bool CharacterDelete(pqxx::work& transaction, u64 characterID);
        bool CharacterAddItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot);
        bool CharacterDeleteItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID);
        bool CharacterSwapContainerSlots(pqxx::work& transaction, u64 characterID, u64 srcContainerID, u64 destContainerID, u16 srcSlot, u16 destSlot);
    }
}