#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

namespace Database
{
    struct DBConnection;

    namespace Util::Character
    {
        void InitCharacterTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

        void LoadCharacterTables(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
        u64 LoadCharacterData(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
        u64 LoadCharacterPermissions(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
        u64 LoadCharacterPermissionGroups(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
        u64 LoadCharacterCurrency(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
    }
}