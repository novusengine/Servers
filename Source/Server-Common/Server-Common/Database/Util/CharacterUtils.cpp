#include "CharacterUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::Character
{
    namespace Loading
    {
        void InitCharacterTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Character Tables...");

            try
            {
                dbConnection->connection->prepare("CharacterGetInfoByID", "SELECT * FROM public.characters WHERE id = $1");
                dbConnection->connection->prepare("CharacterGetInfoByName", "SELECT * FROM public.characters WHERE name = $1");
                dbConnection->connection->prepare("CharacterCreate", "INSERT INTO public.characters (name, race_gender_class) VALUES ($1, $2) RETURNING id");
                dbConnection->connection->prepare("CharacterDelete", "DELETE FROM public.characters WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetRaceGenderClass", "UPDATE public.characters SET race_gender_class = $2 WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetLevel", "UPDATE public.characters SET level = $2 WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetMapID", "UPDATE public.characters SET map_ID = $2 WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetPos", "UPDATE public.characters SET position_x = $2, position_y = $3, position_z = $4, position_o = $5 WHERE id = $1");

                dbConnection->connection->prepare("CharacterSetCurrency", "INSERT INTO public.character_currency (character_id, currency_id, value) VALUES ($1, $2, $3) ON CONFLICT(character_id, currency_id) DO UPDATE SET value = EXCLUDED.value");
                dbConnection->connection->prepare("CharacterDeleteCurrency", "DELETE FROM public.character_currency WHERE character_id = $1 AND currency_id = $2");
                dbConnection->connection->prepare("CharacterDeleteAllCurrency", "DELETE FROM public.character_currency WHERE character_id = $1");

                dbConnection->connection->prepare("CharacterSetPermission", "INSERT INTO public.character_permissions (character_id, permission_id) VALUES ($1, $2) ON CONFLICT(character_id, permission_id) DO NOTHING");
                dbConnection->connection->prepare("CharacterDeletePermission", "DELETE FROM public.character_permissions WHERE character_id = $1 AND permission_id = $2");
                dbConnection->connection->prepare("CharacterDeleteAllPermission", "DELETE FROM public.character_permissions WHERE character_id = $1");

                dbConnection->connection->prepare("CharacterSetPermissionGroup", "INSERT INTO public.character_permission_groups (character_id, permission_group_id) VALUES ($1, $2) ON CONFLICT(character_id, permission_group_id) DO NOTHING");
                dbConnection->connection->prepare("CharacterDeletePermissionGroup", "DELETE FROM public.character_permission_groups WHERE character_id = $1 AND permission_group_id = $2");
                dbConnection->connection->prepare("CharacterDeleteAllPermissionGroup", "DELETE FROM public.character_permission_groups WHERE character_id = $1");

                dbConnection->connection->prepare("CharacterAddItem", "INSERT INTO public.character_items (character_id, container, slot, item_instance_id) VALUES ($1, $2, $3, $4)");
                dbConnection->connection->prepare("CharacterDeleteItem", "DELETE FROM public.character_items WHERE character_id = $1 AND item_instance_id = $2");
                dbConnection->connection->prepare("CharacterSwapContainerSlots", "SELECT swap_container_slots($1, $2, $3, $4, $5);");
                dbConnection->connection->prepare("CharacterDeleteAllItem", "DELETE FROM public.character_items WHERE character_id = $1");
                dbConnection->connection->prepare("CharacterDeleteAllItemInstances", "DELETE FROM public.item_instances WHERE owner_id = $1");

                dbConnection->connection->prepare("CharacterGetItemInstances", "SELECT * FROM public.item_instances WHERE owner_id = $1 ORDER BY id ASC");
                dbConnection->connection->prepare("CharacterGetItemsInContainer", "SELECT * FROM public.character_items WHERE character_id = $1 and container = $2 ORDER BY slot ASC");

                NC_LOG_INFO("Loaded Prepared Statements Character Tables\n");
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_CRITICAL("{0}", e.what());
                return;
            }
        }

        void LoadCharacterTables(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("-- Loading Character Tables --");

            u64 totalRows = 0;
            totalRows += LoadCharacterPermissions(dbConnection, characterTables);
            totalRows += LoadCharacterPermissionGroups(dbConnection, characterTables);
            totalRows += LoadCharacterCurrency(dbConnection, characterTables);

            NC_LOG_INFO("-- Loaded Character Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadCharacterPermissions(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
        {
            NC_LOG_INFO("Loading Table 'character_permissions'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.character_permissions");
            u64 numRows = result[0][0].as<u64>();

            characterTables.charIDToPermissions.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'character_permissions'");
                return 0;
            }

            characterTables.charIDToPermissions.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.character_permissions", [&characterTables](u64 id, u64 characterID, u16 permissionID)
            {
                std::vector<u16>& permissions = characterTables.charIDToPermissions[characterID];
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

            characterTables.charIDToPermissionGroups.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'character_permission_groups'");
                return 0;
            }

            characterTables.charIDToPermissionGroups.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.character_permission_groups", [&characterTables](u64 id, u64 characterID, u16 permissionGroupID)
            {
                std::vector<u16>& permissionGroups = characterTables.charIDToPermissionGroups[characterID];
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

            characterTables.charIDToCurrency.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'character_currency'");
                return 0;
            }

            characterTables.charIDToCurrency.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.character_currency", [&characterTables](u64 id, u64 characterID, u16 currencyID, u64 value)
            {
                CharacterCurrency currency =
                {
                    .currencyID = currencyID,
                    .value = value
                };

                std::vector<CharacterCurrency>& characterCurrencies = characterTables.charIDToCurrency[characterID];
                characterCurrencies.push_back(currency);
            });

            NC_LOG_INFO("Loaded Table 'character_currency' ({0} Rows)", numRows);
            return numRows;
        }
    }

    bool CharacterGetInfoByID(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, pqxx::result& result)
    {
        try
        {
            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();
            result = nonTransaction.exec(pqxx::prepped("CharacterGetInfoByID"), pqxx::params{ characterID });
            if (result.empty())
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterGetInfoByName(std::shared_ptr<DBConnection>& dbConnection, const std::string& name, pqxx::result& result)
    {
        try
        {
            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();
            result = nonTransaction.exec(pqxx::prepped("CharacterGetInfoByName"), pqxx::params{ name });
            if (result.empty())
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterCreate(pqxx::work& transaction, const std::string& name, u16 raceGenderClass, u64& characterID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterCreate"), pqxx::params{ name, raceGenderClass });
            if (queryResult.empty())
                return false;

            characterID = queryResult[0][0].as<u64>();
            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterDelete(pqxx::work& transaction, u64 characterID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterDelete"), pqxx::params{ characterID });
            if (queryResult.affected_rows() == 0)
                return false;

            transaction.exec(pqxx::prepped("CharacterDeleteAllCurrency"), pqxx::params{ characterID });
            transaction.exec(pqxx::prepped("CharacterDeleteAllPermission"), pqxx::params{ characterID });
            transaction.exec(pqxx::prepped("CharacterDeleteAllPermissionGroup"), pqxx::params{ characterID });
            transaction.exec(pqxx::prepped("CharacterDeleteAllItem"), pqxx::params{ characterID });
            transaction.exec(pqxx::prepped("CharacterDeleteAllItemInstances"), pqxx::params{ characterID });

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterSetRaceGenderClass(pqxx::work& transaction, u64 characterID, u16 raceGenderClass)
    {
        try
        {
            auto params = pqxx::params{ characterID, raceGenderClass };
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetRaceGenderClass"), pqxx::params{ characterID, raceGenderClass });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterSetLevel(pqxx::work& transaction, u64 characterID, u16 level)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetLevel"), pqxx::params{ characterID, level });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterSetMapID(pqxx::work& transaction, u64 characterID, u32 mapID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetMapID"), pqxx::params{ characterID, mapID });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterSetPositionOrientation(pqxx::work& transaction, u64 characterID, const vec3& position, f32 orientation)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSetPos"), pqxx::params{ characterID, position.x, position.y, position.z, orientation });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterAddItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterAddItem"), pqxx::params{ characterID, containerID, slot, itemInstanceID });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterDeleteItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterDeleteItem"), pqxx::params{ characterID, itemInstanceID });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterSwapContainerSlots(pqxx::work& transaction, u64 characterID, u64 srcContainerID, u64 destContainerID, u16 srcSlot, u16 destSlot)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterSwapContainerSlots"), pqxx::params{ characterID, srcContainerID, destContainerID, srcSlot, destSlot });
            if (queryResult.affected_rows() == 0)
                return false;

            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }

    bool CharacterGetItems(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, pqxx::result& result)
    {
        try
        {
            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            result = nonTransaction.exec(pqxx::prepped("CharacterGetItemInstances"), characterID);
            return result.affected_rows() > 0;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
    bool CharacterGetItemsInContainer(std::shared_ptr<DBConnection>& dbConnection, u64 characterID, u64 containerID, pqxx::result& result)
    {
        try
        {
            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            result = nonTransaction.exec(pqxx::prepped("CharacterGetItemsInContainer"), pqxx::params{ characterID, containerID });
            return result.affected_rows() > 0;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
}