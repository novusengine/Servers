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
            u64 LoadCharacterPermissions(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterPermissionGroups(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
            u64 LoadCharacterCurrency(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables);
        }

        bool CharacterGetInfoByID(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, pqxx::result& result);
        bool CharacterGetInfoByName(std::shared_ptr<DBConnection>& dbConnection, const std::string& name, pqxx::result& result);
        bool CharacterCreate(pqxx::work& transaction, const std::string& name, u16 raceGenderClass, u64& characterID);
        bool CharacterDelete(pqxx::work& transaction, u64 characterID);
        bool CharacterSetRaceGenderClass(pqxx::work& transaction, u64 characterID, u16 raceGenderClass);
        bool CharacterSetLevel(pqxx::work& transaction, u64 characterID, u16 level);
        bool CharacterSetMapID(pqxx::work& transaction, u64 characterID, u32 mapID);
        bool CharacterSetPositionOrientation(pqxx::work& transaction, u64 characterID, const vec3& position, f32 orientation);
        bool CharacterAddItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot);
        bool CharacterDeleteItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID);
        bool CharacterSwapContainerSlots(pqxx::work& transaction, u64 characterID, u64 srcContainerID, u64 destContainerID, u16 srcSlot, u16 destSlot);
        bool CharacterGetItems(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, pqxx::result& result);
        bool CharacterGetItemsInContainer(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, u64 containerID, pqxx::result& result);
    }
}