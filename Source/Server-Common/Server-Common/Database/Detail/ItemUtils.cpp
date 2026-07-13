#include "ItemUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>

#include <MetaGen/Postgres/World/Tables/ItemArmorTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemShieldTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemStatTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemWeaponTemplates.h>
#include <MetaGen/Postgres/Character/Tables/ItemInstances.h>

namespace Database::Detail::Item
{
    namespace Loading
    {
        void LoadItemTables(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("-- Loading Item Tables --");

            u64 totalRows = 0;
            totalRows += LoadItemTemplateTable(transaction, itemTables);
            totalRows += LoadItemStatTemplateTable(transaction, itemTables);
            totalRows += LoadItemArmorTemplateTable(transaction, itemTables);
            totalRows += LoadItemShieldTemplateTable(transaction, itemTables);
            totalRows += LoadItemWeaponTemplateTable(transaction, itemTables);

            NC_LOG_INFO("-- Loaded Item Tables ({0} Rows) --\n", totalRows);
        }

        u64 LoadItemTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_template'");

            itemTables.templateIDToTemplateDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::ItemTemplatesTable>(transaction, [&itemTables, &numRows](auto record)
            {
                itemTables.templateIDToTemplateDefinition[record.id] = {
                    .id = record.id,
                    .displayID = record.displayId,
                    .bind = static_cast<u8>(record.bind),
                    .rarity = static_cast<u8>(record.rarity),
                    .category = static_cast<u8>(record.category),
                    .type = static_cast<u8>(record.type),
                    .virtualLevel = record.virtualLevel,
                    .requiredLevel = record.requiredLevel,
                    .durability = record.durability,
                    .iconID = record.iconId,
                    .name = std::move(record.name),
                    .description = std::move(record.description),
                    .armor = record.armor,
                    .statTemplateID = record.statTemplateId,
                    .armorTemplateID = record.armorTemplateId,
                    .weaponTemplateID = record.weaponTemplateId,
                    .shieldTemplateID = record.shieldTemplateId
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'item_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemStatTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_stat_template'");

            itemTables.statTemplateIDToTemplateDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::ItemStatTemplatesTable>(transaction, [&itemTables, &numRows](const auto& record)
            {
                itemTables.statTemplateIDToTemplateDefinition[record.id] = {
                    .id = record.id,
                    .statTypes = { static_cast<u8>(record.statType1), static_cast<u8>(record.statType2), static_cast<u8>(record.statType3), static_cast<u8>(record.statType4), static_cast<u8>(record.statType5), static_cast<u8>(record.statType6), static_cast<u8>(record.statType7), static_cast<u8>(record.statType8) },
                    .statValues = { record.statValue1, record.statValue2, record.statValue3, record.statValue4, record.statValue5, record.statValue6, record.statValue7, record.statValue8 }
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'item_stat_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemArmorTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_armor_template'");
            itemTables.armorTemplateIDToTemplateDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::ItemArmorTemplatesTable>(transaction, [&itemTables, &numRows](const auto& record)
            {
                itemTables.armorTemplateIDToTemplateDefinition[record.id] = {
                    record.id, static_cast<u8>(record.equipType), record.bonusArmor
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'item_armor_template' ({0} Rows)", numRows);
            return numRows;
        }
        
        u64 LoadItemShieldTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_shield_template'");
            itemTables.shieldTemplateIDToTemplateDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::ItemShieldTemplatesTable>(transaction, [&itemTables, &numRows](const auto& record)
            {
                itemTables.shieldTemplateIDToTemplateDefinition[record.id] = {
                    record.id, record.bonusArmor, record.block
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'item_shield_template' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadItemWeaponTemplateTable(pqxx::read_transaction& transaction, Database::ItemTables& itemTables)
        {
            NC_LOG_INFO("Loading Table 'item_weapon_template'");
            itemTables.weaponTemplateIDToTemplateDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::ItemWeaponTemplatesTable>(transaction, [&itemTables, &numRows](const auto& record)
            {
                itemTables.weaponTemplateIDToTemplateDefinition[record.id] = {
                    record.id, static_cast<u8>(record.weaponStyle), record.minDamage, record.maxDamage, record.speed
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'item_weapon_template' ({0} Rows)", numRows);
            return numRows;
        }
    }

    MetaGen::Postgres::Character::ItemInstancesRecord ItemInstanceCreate(pqxx::work& work, u32 itemID, u64 ownerID, u16 count, u16 durability)
    {
        return Generated::Execute<MetaGen::Postgres::Character::ItemInstancesTable::Insert>(work, itemID, ownerID, count, durability);
    }
    bool ItemInstanceDestroy(pqxx::work& work, u64 itemInstanceID)
    {
        return Generated::Execute<MetaGen::Postgres::Character::ItemInstancesTable::Delete>(work, itemInstanceID).affected_rows() != 0;
    }
}
