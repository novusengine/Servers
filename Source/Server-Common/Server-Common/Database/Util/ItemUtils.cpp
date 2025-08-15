#include "ItemUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>

#include <pqxx/nontransaction>

namespace Database::Util::Item
{
    namespace Loading
    {
        void InitItemTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Item Tables...");

            dbConnection->connection->prepare("SetItemTemplate", "INSERT INTO public.item_template (id, displayid, bind, rarity, category, type, virtuallevel, requiredlevel, durability, iconid, name, description, armor, stattemplateid, armortemplateid, weapontemplateid, shieldtemplateid) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) ON CONFLICT(id) DO UPDATE SET displayid = EXCLUDED.displayid, bind = EXCLUDED.bind, rarity = EXCLUDED.rarity, category = EXCLUDED.category, type = EXCLUDED.type, virtuallevel = EXCLUDED.virtuallevel, requiredlevel = EXCLUDED.requiredlevel, durability = EXCLUDED.durability, iconid = EXCLUDED.iconid, name = EXCLUDED.name, description = EXCLUDED.description, armor = EXCLUDED.armor, stattemplateid = EXCLUDED.stattemplateid, armortemplateid = EXCLUDED.armortemplateid, weapontemplateid = EXCLUDED.weapontemplateid, shieldtemplateid = EXCLUDED.shieldtemplateid");
            dbConnection->connection->prepare("SetItemStatTemplate", "INSERT INTO public.item_stat_template (id, stattype1, stattype2, stattype3, stattype4, stattype5, stattype6, stattype7, stattype8, statvalue1, statvalue2, statvalue3, statvalue4, statvalue5, statvalue6, statvalue7, statvalue8) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) ON CONFLICT(id) DO UPDATE SET stattype1 = EXCLUDED.stattype1, stattype2 = EXCLUDED.stattype2, stattype3 = EXCLUDED.stattype3, stattype4 = EXCLUDED.stattype4, stattype5 = EXCLUDED.stattype5, stattype6 = EXCLUDED.stattype6, stattype7 = EXCLUDED.stattype7, stattype8 = EXCLUDED.stattype8, statvalue1 = EXCLUDED.statvalue1, statvalue2 = EXCLUDED.statvalue2, statvalue3 = EXCLUDED.statvalue3, statvalue4 = EXCLUDED.statvalue4, statvalue5 = EXCLUDED.statvalue5, statvalue6 = EXCLUDED.statvalue6, statvalue7 = EXCLUDED.statvalue7, statvalue8 = EXCLUDED.statvalue8");
            dbConnection->connection->prepare("SetItemArmorTemplate", "INSERT INTO public.item_armor_template (id, equiptype, bonusarmor) VALUES ($1, $2, $3) ON CONFLICT(id) DO UPDATE SET equiptype = EXCLUDED.equiptype, bonusarmor = EXCLUDED.bonusarmor");
            dbConnection->connection->prepare("SetItemWeaponTemplate", "INSERT INTO public.item_weapon_template (id, weaponstyle, mindamage, maxdamage, speed) VALUES ($1, $2, $3, $4, $5) ON CONFLICT(id) DO UPDATE SET weaponstyle = EXCLUDED.weaponstyle, mindamage = EXCLUDED.mindamage, maxdamage = EXCLUDED.maxdamage, speed = EXCLUDED.speed");
            dbConnection->connection->prepare("SetItemShieldTemplate", "INSERT INTO public.item_shield_template (id, bonusarmor, block) VALUES ($1, $2, $3) ON CONFLICT(id) DO UPDATE SET bonusarmor = EXCLUDED.bonusarmor, block = EXCLUDED.block");

            dbConnection->connection->prepare("ItemInstanceCreate", "INSERT INTO public.item_instances (itemid, ownerid, count, durability) VALUES ($1, $2, $3, $4) RETURNING id");
            dbConnection->connection->prepare("ItemInstanceDelete", "DELETE FROM public.item_instances WHERE id = $1");
            dbConnection->connection->prepare("ItemInstanceUpdate", "UPDATE public.item_instances SET ownerid = $2, count = $3, durability = $4 WHERE id = $1");
            dbConnection->connection->prepare("ItemInstanceSetOwner", "UPDATE public.item_instances SET ownerid = $2 WHERE id = $1");
            dbConnection->connection->prepare("ItemInstanceSetCount", "UPDATE public.item_instances SET count = $2 WHERE id = $1");
            dbConnection->connection->prepare("ItemInstanceSetDurability", "UPDATE public.item_instances SET durability = $2 WHERE id = $1");

            NC_LOG_INFO("Loaded Item Tables Prepared Statements\n");
        }

        void LoadItemTables(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("-- Loading Item Tables --");

            u64 totalRows = 0;
            totalRows += LoadItemTemplateTable(dbConnection, itemTables);
            totalRows += LoadItemStatTemplateTable(dbConnection, itemTables);
            totalRows += LoadItemArmorTemplateTable(dbConnection, itemTables);
            totalRows += LoadItemWeaponTemplateTable(dbConnection, itemTables);
            totalRows += LoadItemShieldTemplateTable(dbConnection, itemTables);

            NC_LOG_INFO("-- Loaded Item Tables ({0} Rows) --\n", totalRows);
        }

        u64 LoadItemTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.item_template");
            u64 numRows = result[0][0].as<u64>();

            itemTables.templateIDToTemplateDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'item_template'");
                return 0;
            }

            itemTables.templateIDToTemplateDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.item_template", [&itemTables](u32 id, u32 displayID, u16 bind, u16 rarity, u16 category, u16 type, u16 virtuallevel, u16 requiredLevel, u32 durability, u32 iconID, const std::string& name, const std::string& description, u32 armor, u32 statTemplateID, u32 armorTemplateID, u32 weaponTemplateID, u32 shieldTemplateID)
                {
                    ::GameDefine::Database::ItemTemplate itemTemplate =
                    {
                        .id = id,
                        .displayID = displayID,
                        .bind = static_cast<u8>(bind),
                        .rarity = static_cast<u8>(rarity),
                        .category = static_cast<u8>(category),
                        .type = static_cast<u8>(type),
                        .virtualLevel = virtuallevel,
                        .requiredLevel = requiredLevel,
                        .durability = durability,
                        .iconID = iconID,

                        .name = name,
                        .description = description,

                        .armor = armor,
                        .statTemplateID = statTemplateID,
                        .armorTemplateID = armorTemplateID,
                        .weaponTemplateID = weaponTemplateID,
                        .shieldTemplateID = shieldTemplateID
                    };

                    itemTables.templateIDToTemplateDefinition.insert({ id, itemTemplate });
                });

            NC_LOG_INFO("Loaded Table 'item_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemStatTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_stat_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.item_stat_template");
            u64 numRows = result[0][0].as<u64>();

            itemTables.statTemplateIDToTemplateDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'item_stat_template'");
                return 0;
            }

            itemTables.statTemplateIDToTemplateDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.item_stat_template", [&itemTables](u32 id, u16 statType1, u16 statType2, u16 statType3, u16 statType4, u16 statType5, u16 statType6, u16 statType7, u16 statType8, i32 statValue1, i32 statValue2, i32 statValue3, i32 statValue4, i32 statValue5, i32 statValue6, i32 statValue7, i32 statValue8)
                {
                    ::GameDefine::Database::ItemStatTemplate itemStatTemplate =
                    {
                        .id = id,
                        .statTypes = { (u8)statType1, (u8)statType2, (u8)statType3, (u8)statType4, (u8)statType5, (u8)statType6, (u8)statType7, (u8)statType8 },
                        .statValues = { statValue1, statValue2, statValue3, statValue4, statValue5, statValue6, statValue7, statValue8 }
                    };

                    itemTables.statTemplateIDToTemplateDefinition.insert({ id, itemStatTemplate });
                });

            NC_LOG_INFO("Loaded Table 'item_stat_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemArmorTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_armor_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.item_armor_template");
            u64 numRows = result[0][0].as<u64>();

            itemTables.armorTemplateIDToTemplateDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'item_armor_template'");
                return 0;
            }

            itemTables.armorTemplateIDToTemplateDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.item_armor_template", [&itemTables](u32 id, u16 equipType, u32 bonusArmor)
                {
                    ::GameDefine::Database::ItemArmorTemplate itemArmorTemplate =
                    {
                        .id = id,
                        .equipType = static_cast<u8>(equipType),
                        .bonusArmor = bonusArmor
                    };

                    itemTables.armorTemplateIDToTemplateDefinition.insert({ id, itemArmorTemplate });
                });

            NC_LOG_INFO("Loaded Table 'item_armor_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemWeaponTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_weapon_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.item_weapon_template");
            u64 numRows = result[0][0].as<u64>();

            itemTables.weaponTemplateIDToTemplateDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'item_weapon_template'");
                return 0;
            }

            itemTables.weaponTemplateIDToTemplateDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.item_weapon_template", [&itemTables](u32 id, u16 weaponStyle, u32 minDamage, u32 maxDamage, f32 speed)
                {
                    ::GameDefine::Database::ItemWeaponTemplate itemWeaponTemplate =
                    {
                        .id = id,
                        .weaponStyle = static_cast<u8>(weaponStyle),
                        .minDamage = minDamage,
                        .maxDamage = maxDamage,
                        .speed = speed
                    };

                    itemTables.weaponTemplateIDToTemplateDefinition.insert({ id, itemWeaponTemplate });
                });

            NC_LOG_INFO("Loaded Table 'item_weapon_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemShieldTemplateTable(std::shared_ptr<DBConnection>& dbConnection, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_shield_template'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.item_shield_template");
            u64 numRows = result[0][0].as<u64>();

            itemTables.shieldTemplateIDToTemplateDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'item_shield_template'");
                return 0;
            }

            itemTables.shieldTemplateIDToTemplateDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.item_shield_template", [&itemTables](u32 id, u32 bonusArmor, u32 block)
            {
                ::GameDefine::Database::ItemShieldTemplate itemShieldTemplate =
                {
                    .id = id,
                    .bonusArmor = bonusArmor,
                    .block = block
                };

                itemTables.shieldTemplateIDToTemplateDefinition.insert({ id, itemShieldTemplate });
            });

            NC_LOG_INFO("Loaded Table 'item_shield_template' ({0} Rows)", numRows);
            return numRows;
        }
    }

    bool ItemInstanceCreate(pqxx::work& work, u32 itemID, u64 ownerID, u16 count, u16 durability, u64& itemInstanceID)
    {
        try
        {
            auto result = work.exec(pqxx::prepped{ "ItemInstanceCreate" }, pqxx::params{ itemID, ownerID, count, durability });
            if (result.empty())
                return false;

            u64 id = result[0][0].as<u64>();
            itemInstanceID = id;
            return true;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
    bool ItemInstanceDestroy(pqxx::work& work, u64 itemInstanceID)
    {
        try
        {
            auto result = work.exec(pqxx::prepped{ "ItemInstanceDelete" }, pqxx::params{ itemInstanceID });
            return result.affected_rows() != 0;
        }
        catch (const pqxx::sql_error& e)
        {
            NC_LOG_WARNING("{0}", e.what());
            return false;
        }
    }
}