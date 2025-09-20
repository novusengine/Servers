#include "SpellUtils.h"
#include "Server-Common/Database/DBController.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <pqxx/nontransaction>

namespace Database::Util::Spell
{
    namespace Loading
    {
        void InitSpellTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection)
        {
            NC_LOG_INFO("Loading Prepared Statements Spell Tables...");

            try
            {
                dbConnection->connection->prepare("SetSpell", "INSERT INTO public.spells (id, name, description, aura_description, icon_id, cast_time, cooldown) VALUES ($1, $2, $3, $4, $5, $6, $7) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, name = EXCLUDED.name, description = EXCLUDED.description, aura_description = EXCLUDED.aura_description, icon_id = EXCLUDED.icon_id, cast_time = EXCLUDED.cast_time, cooldown = EXCLUDED.cooldown");
                dbConnection->connection->prepare("SetSpellEffect", "INSERT INTO public.spell_effects (id, spell_id, effect_priority, effect_type, effect_value_1, effect_value_2, effect_value_3, effect_misc_value_1, effect_misc_value_2, effect_misc_value_3) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, spell_id = EXCLUDED.spell_id, effect_priority = EXCLUDED.effect_priority, effect_type = EXCLUDED.effect_type, effect_value_1 = EXCLUDED.effect_value_1, effect_value_2 = EXCLUDED.effect_value_2, effect_value_3 = EXCLUDED.effect_value_3, effect_misc_value_1 = EXCLUDED.effect_misc_value_1, effect_misc_value_2 = EXCLUDED.effect_misc_value_2, effect_misc_value_3 = EXCLUDED.effect_misc_value_3");

                NC_LOG_INFO("Loaded Prepared Statements Spell Tables\n");
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_CRITICAL("{0}", e.what());
                return;
            }
        }

        void LoadSpellTables(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("-- Loading Spell Tables --");

            u64 totalRows = 0;
            totalRows += LoadSpells(dbConnection, spellTables);
            totalRows += LoadSpellEffects(dbConnection, spellTables);

            NC_LOG_INFO("-- Loaded Spell Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadSpells(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spells'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.spells");
            u64 numRows = result[0][0].as<u64>();

            spellTables.idToDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'spells'");
                return 0;
            }

            spellTables.idToDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.spells", [&spellTables](u32 id, const std::string& name, const std::string& description, const std::string& auraDescription, u32 iconID, f32 castTime, f32 cooldown)
            {
                auto& spell = spellTables.idToDefinition[id];

                spell.id = id;
                spell.name = name;
                spell.description = description;
                spell.auraDescription = auraDescription;
                spell.iconID = iconID;
                spell.castTime = castTime;
                spell.cooldown = cooldown;
            });

            NC_LOG_INFO("Loaded Table 'spells' ({0} Rows)", numRows);
            return numRows;
        }
        u64 LoadSpellEffects(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_effects'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.spell_effects");
            u64 numRows = result[0][0].as<u64>();

            spellTables.spellIDToEffects.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'spell_effects'");
                return 0;
            }

            spellTables.spellIDToEffects.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.spell_effects ORDER BY spell_id ASC, effect_priority DESC", [&spellTables](u32 id, u32 spellID, u16 effect_priority, u16 effect_type, i32 effect_value_1, i32 effect_value_2, i32 effect_value_3, i32 effect_misc_value_1, i32 effect_misc_value_2, i32 effect_misc_value_3)
            {
                auto& effectList = spellTables.spellIDToEffects[id];

                auto& effect = effectList.emplace_back();
                effect.id = id;
                effect.spellID = spellID;
                effect.effectPriority = static_cast<u8>(effect_priority);
                effect.effectType = static_cast<u8>(effect_type);

                effect.effectValue1 = effect_value_1;
                effect.effectValue2 = effect_value_2;
                effect.effectValue3 = effect_value_3;
                effect.effectMiscValue1 = effect_misc_value_1;
                effect.effectMiscValue2 = effect_misc_value_2;
                effect.effectMiscValue3 = effect_misc_value_3;
            });

            NC_LOG_INFO("Loaded Table 'spell_effects' ({0} Rows)", numRows);
            return numRows;
        }
    }
}