#include "CharacterUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::Character
{
    void InitCharacterTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
    {
        NC_LOG_INFO("Loading Prepared Statements Character Tables...");

        dbConnection->connection->prepare("CreateCharacter", "INSERT INTO public.characters (name, racegenderclass) VALUES ($1, $2) RETURNING id");
        dbConnection->connection->prepare("DeleteCharacter", "DELETE FROM public.characters WHERE id = $1");
        dbConnection->connection->prepare("UpdateCharacterSetRaceGenderClass", "UPDATE public.characters SET racegenderclass = $2 WHERE id = $1");
        dbConnection->connection->prepare("UpdateCharacterSetLevel", "UPDATE public.characters SET level = $2 WHERE id = $1");

        dbConnection->connection->prepare("SetCharacterCurrency", "INSERT INTO public.character_currency (characterid, currencyid, value) VALUES ($1, $2, $3) ON CONFLICT(characterid, currencyid) DO UPDATE SET value = EXCLUDED.value");
        dbConnection->connection->prepare("DeleteCharacterCurrency", "DELETE FROM public.character_currency WHERE characterid = $1 AND currencyid = $2");

        dbConnection->connection->prepare("SetCharacterPermission", "INSERT INTO public.character_permissions (characterid, permissionid) VALUES ($1, $2) ON CONFLICT(characterid, permissionid) DO NOTHING");
        dbConnection->connection->prepare("DeleteCharacterPermission", "DELETE FROM public.character_permissions WHERE characterid = $1 AND permissionid = $2");

        dbConnection->connection->prepare("SetCharacterPermissionGroup", "INSERT INTO public.character_permission_groups (characterid, permissiongroupid) VALUES ($1, $2) ON CONFLICT(characterid, permissiongroupid) DO NOTHING");
        dbConnection->connection->prepare("DeleteCharacterPermissionGroup", "DELETE FROM public.character_permission_groups WHERE characterid = $1 AND permissiongroupid = $2");

        NC_LOG_INFO("Loaded Prepared Statements Character Tables\n");
    }

    void LoadCharacterTables(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
    {
        NC_LOG_INFO("-- Loading Character Tables --");

        u64 totalRows = 0;
        totalRows += LoadCharacterData(dbConnection, characterTables);
        totalRows += LoadCharacterPermissions(dbConnection, characterTables);
        totalRows += LoadCharacterPermissionGroups(dbConnection, characterTables);
        totalRows += LoadCharacterCurrency(dbConnection, characterTables);

        NC_LOG_INFO("-- Loaded Character Tables ({0} rows) --\n", totalRows);
    }

    u64 LoadCharacterData(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
    {
        NC_LOG_INFO("Loading Table 'characters'...");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.characters");
        u64 numRows = result[0][0].as<u64>();

        characterTables.idToDefinition.clear();
        characterTables.nameHashToID.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'characters'");
            return 0;
        }

        characterTables.idToDefinition.reserve(numRows);
        characterTables.nameHashToID.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.characters", [&characterTables](u64 id, u32 accountID, const std::string& name, u32 totalTime, u32 levelTime, u32 logoutTime, u32 flags, u16 raceGenderClass, u16 level, u64 experiencePoints, u16 mapID, f32 posX, f32 posY, f32 posZ, f32 orientation)
        {
            Database::CharacterDefinition character =
            {
                .id = id,
                .accountid = accountID,
                .name = name,
                .totalTime = totalTime,
                .levelTime = levelTime,
                .logoutTime = logoutTime,
                .flags = flags,
                .raceGenderClass = raceGenderClass,
                .level = level,
                .experiencePoints = experiencePoints,
                .mapID = mapID,
                .position = vec3(posX, posY, posZ),
                .orientation = orientation
            };

            u32 characterNameHash = StringUtils::fnv1a_32(character.name.c_str(), character.name.size());

            characterTables.idToDefinition.insert({ character.id, character });
            characterTables.nameHashToID.insert({ characterNameHash, character.id });
        });

        NC_LOG_INFO("Loaded Table 'characters' ({0} Rows)", numRows);
        return numRows;
    }
    u64 LoadCharacterPermissions(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
    {
        NC_LOG_INFO("Loading Table 'character_permissions'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.character_permissions");
        u64 numRows = result[0][0].as<u64>();

        characterTables.idToPermissions.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'character_permissions'");
            return 0;
        }

        characterTables.idToPermissions.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.character_permissions", [&characterTables](u64 id, u64 characterID, u16 permissionID)
        {
            std::vector<u16>& permissions = characterTables.idToPermissions[characterID];
            permissions.push_back(permissionID);
        });

        NC_LOG_INFO("Loaded Table 'character_permissions' ({0} Rows)", numRows);
        return numRows;
    }
    u64 LoadCharacterPermissionGroups(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
    {
        NC_LOG_INFO("Loading Table 'character_permission_groups'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.character_permission_groups");
        u64 numRows = result[0][0].as<u64>();

        characterTables.idToPermissionGroups.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'character_permission_groups'");
            return 0;
        }

        characterTables.idToPermissionGroups.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.character_permission_groups", [&characterTables](u64 id, u64 characterID, u16 permissionGroupID)
        {
            std::vector<u16>& permissionGroups = characterTables.idToPermissionGroups[characterID];
            permissionGroups.push_back(permissionGroupID);
        });

        NC_LOG_INFO("Loaded Table 'character_permission_groups' ({0} Rows)", numRows);
        return numRows;
    }
    u64 LoadCharacterCurrency(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
    {
        NC_LOG_INFO("Loading Table 'character_currency'");

        pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

        auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.character_currency");
        u64 numRows = result[0][0].as<u64>();

        characterTables.idToCurrency.clear();

        if (numRows == 0)
        {
            NC_LOG_INFO("Skipped Table 'character_currency'");
            return 0;
        }

        characterTables.idToCurrency.reserve(numRows);

        nonTransaction.for_stream("SELECT * FROM public.character_currency", [&characterTables](u64 id, u64 characterID, u16 currencyID, u64 value)
        {
            CharacterCurrency currency =
            {
                .currencyID = currencyID,
                .value = value
            };

            std::vector<CharacterCurrency>& characterCurrencies = characterTables.idToCurrency[characterID];
            characterCurrencies.push_back(currency);
        });

        NC_LOG_INFO("Loaded Table 'character_currency' ({0} Rows)", numRows);
        return numRows;
    }
}