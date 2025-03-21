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
                dbConnection->connection->prepare("CharacterCreate", "INSERT INTO public.characters (name, racegenderclass) VALUES ($1, $2) RETURNING id");
                dbConnection->connection->prepare("CharacterDelete", "DELETE FROM public.characters WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetRaceGenderClass", "UPDATE public.characters SET racegenderclass = $2 WHERE id = $1");
                dbConnection->connection->prepare("CharacterSetLevel", "UPDATE public.characters SET level = $2 WHERE id = $1");

                dbConnection->connection->prepare("CharacterSetCurrency", "INSERT INTO public.character_currency (characterid, currencyid, value) VALUES ($1, $2, $3) ON CONFLICT(characterid, currencyid) DO UPDATE SET value = EXCLUDED.value");
                dbConnection->connection->prepare("CharacterDeleteCurrency", "DELETE FROM public.character_currency WHERE characterid = $1 AND currencyid = $2");
                dbConnection->connection->prepare("CharacterDeleteAllCurrency", "DELETE FROM public.character_currency WHERE characterid = $1");

                dbConnection->connection->prepare("CharacterSetPermission", "INSERT INTO public.character_permissions (characterid, permissionid) VALUES ($1, $2) ON CONFLICT(characterid, permissionid) DO NOTHING");
                dbConnection->connection->prepare("CharacterDeletePermission", "DELETE FROM public.character_permissions WHERE characterid = $1 AND permissionid = $2");
                dbConnection->connection->prepare("CharacterDeleteAllPermission", "DELETE FROM public.character_permissions WHERE characterid = $1");

                dbConnection->connection->prepare("CharacterSetPermissionGroup", "INSERT INTO public.character_permission_groups (characterid, permissiongroupid) VALUES ($1, $2) ON CONFLICT(characterid, permissiongroupid) DO NOTHING");
                dbConnection->connection->prepare("CharacterDeletePermissionGroup", "DELETE FROM public.character_permission_groups WHERE characterid = $1 AND permissiongroupid = $2");
                dbConnection->connection->prepare("CharacterDeleteAllPermissionGroup", "DELETE FROM public.character_permission_groups WHERE characterid = $1");

                dbConnection->connection->prepare("CharacterAddItem", "INSERT INTO public.character_items (charid, container, slot, iteminstanceid) VALUES ($1, $2, $3, $4)");
                dbConnection->connection->prepare("CharacterDeleteItem", "DELETE FROM public.character_items WHERE charid = $1 AND iteminstanceid = $2");
                dbConnection->connection->prepare("CharacterSwapContainerSlots", "SELECT swap_container_slots($1, $2, $3, $4, $5);");
                dbConnection->connection->prepare("CharacterDeleteAllItem", "DELETE FROM public.character_items WHERE charid = $1");
                dbConnection->connection->prepare("CharacterDeleteAllItemInstances", "DELETE FROM public.item_instances WHERE ownerid = $1");

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
            totalRows += LoadCharacterData(dbConnection, characterTables);
            totalRows += LoadCharacterPermissions(dbConnection, characterTables);
            totalRows += LoadCharacterPermissionGroups(dbConnection, characterTables);
            totalRows += LoadCharacterCurrency(dbConnection, characterTables);
            totalRows += LoadCharacterItems(dbConnection, characterTables, itemTables);

            NC_LOG_INFO("-- Loaded Character Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadCharacterData(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables)
        {
            NC_LOG_INFO("Loading Table 'characters'...");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.characters");
            u64 numRows = result[0][0].as<u64>();

            characterTables.charIDToDefinition.clear();
            characterTables.charNameHashToCharID.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'characters'");
                return 0;
            }

            characterTables.charIDToDefinition.reserve(numRows);
            characterTables.charNameHashToCharID.reserve(numRows);

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

                characterTables.charIDToDefinition.insert({ character.id, character });
                characterTables.charNameHashToCharID.insert({ characterNameHash, character.id });
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

        u64 LoadCharacterItems(std::shared_ptr<DBConnection>& dbConnection, Database::CharacterTables& characterTables, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'character_items'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.character_items");
            u64 numRows = result[0][0].as<u64>();

            characterTables.charIDToBaseContainer.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'character_items'");
                return 0;
            }

            characterTables.charIDToBaseContainer.reserve(numRows);
            itemTables.itemInstanceIDToContainer.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.character_items", [&characterTables, &itemTables](u64 charID, u64 containerID, u16 slot, u64 itemInstanceID)
            {
                if (!itemTables.itemInstanceIDToDefinition.contains(itemInstanceID))
                    return;

                if (containerID == 0)
                {
                    if (!characterTables.charIDToBaseContainer.contains(charID))
                        characterTables.charIDToBaseContainer[charID] = Container(CHARACTER_BASE_CONTAINER_SIZE);

                    Container& baseContainer = characterTables.charIDToBaseContainer[charID];

                    GameDefine::ObjectGuid itemObjectGuid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Item, itemInstanceID);
                    baseContainer.AddItemToSlot(itemObjectGuid, (u8)slot);
                }
                else
                {
                    const ItemInstance& containerItemInstance = itemTables.itemInstanceIDToDefinition[containerID];

                    if (!itemTables.templateIDToTemplateDefinition.contains(containerItemInstance.itemID))
                        return;

                    const GameDefine::Database::ItemTemplate& containerItemTemplate = itemTables.templateIDToTemplateDefinition[containerItemInstance.itemID];

                    // 5 == Container
                    bool isContainer = containerItemTemplate.category == 5;
                    u32 slots = containerItemTemplate.durability;
                    if (!isContainer || (isContainer && slots == 0))
                        return;

                    if (!itemTables.itemInstanceIDToContainer.contains(containerID))
                        itemTables.itemInstanceIDToContainer[containerID] = Container(slots);

                    Container& container = itemTables.itemInstanceIDToContainer[containerID];

                    GameDefine::ObjectGuid itemObjectGuid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Item, itemInstanceID);
                    container.AddItemToSlot(itemObjectGuid, (u8)slot);
                }
            });

            NC_LOG_INFO("Loaded Table 'character_items' ({0} Rows)", numRows);
            return numRows;
        }
    }

    bool CharacterCreate(pqxx::work& transaction, const std::string& name, u16 racegenderclass, u64& characterID)
    {
        try
        {
            auto queryResult = transaction.exec(pqxx::prepped("CharacterCreate"), pqxx::params{ name, racegenderclass });
            if (queryResult.empty())
                return false;

            u64 charID = queryResult[0][0].as<u64>();
            characterID = charID;
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
}