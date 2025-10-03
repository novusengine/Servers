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
                dbConnection->connection->prepare("SetSpell", "INSERT INTO public.spells (id, name, description, aura_description, icon_id, cast_time, cooldown, duration) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, name = EXCLUDED.name, description = EXCLUDED.description, aura_description = EXCLUDED.aura_description, icon_id = EXCLUDED.icon_id, cast_time = EXCLUDED.cast_time, cooldown = EXCLUDED.cooldown, duration = EXCLUDED.duration");
                dbConnection->connection->prepare("SetSpellEffect", "INSERT INTO public.spell_effects (id, spell_id, effect_priority, effect_type, effect_value_1, effect_value_2, effect_value_3, effect_misc_value_1, effect_misc_value_2, effect_misc_value_3) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, spell_id = EXCLUDED.spell_id, effect_priority = EXCLUDED.effect_priority, effect_type = EXCLUDED.effect_type, effect_value_1 = EXCLUDED.effect_value_1, effect_value_2 = EXCLUDED.effect_value_2, effect_value_3 = EXCLUDED.effect_value_3, effect_misc_value_1 = EXCLUDED.effect_misc_value_1, effect_misc_value_2 = EXCLUDED.effect_misc_value_2, effect_misc_value_3 = EXCLUDED.effect_misc_value_3");
                dbConnection->connection->prepare("SetSpellProcData", "INSERT INTO public.spell_proc_data (id, phase_mask, type_mask, hit_mask, flags, procs_per_min, chance_to_proc, internal_cooldown_ms, charges) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, phase_mask = EXCLUDED.phase_mask, type_mask = EXCLUDED.type_mask, hit_mask = EXCLUDED.hit_mask, flags = EXCLUDED.flags, procs_per_min = EXCLUDED.procs_per_min, chance_to_proc = EXCLUDED.chance_to_proc, internal_cooldown_ms = EXCLUDED.internal_cooldown_ms, charges = EXCLUDED.charges");
                dbConnection->connection->prepare("SetSpellProcLink", "INSERT INTO public.spell_proc_link (id, spell_id, effect_mask, proc_data_id) VALUES ($1, $2, $3, $4) ON CONFLICT(id) DO UPDATE SET id = EXCLUDED.id, spell_id = EXCLUDED.spell_id, effect_mask = EXCLUDED.effect_mask, proc_data_id = EXCLUDED.proc_data_id");

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
            totalRows += LoadSpellProcData(dbConnection, spellTables);
            totalRows += LoadSpellProcLink(dbConnection, spellTables);

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

            nonTransaction.for_stream("SELECT * FROM public.spells", [&spellTables](u32 id, const std::string& name, const std::string& description, const std::string& auraDescription, u32 iconID, f32 castTime, f32 cooldown, f32 duration)
            {
                auto& spell = spellTables.idToDefinition[id];

                spell.id = id;
                spell.name = name;
                spell.description = description;
                spell.auraDescription = auraDescription;
                spell.iconID = iconID;
                spell.castTime = castTime;
                spell.cooldown = cooldown;
                spell.duration = duration;
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
                auto& effectList = spellTables.spellIDToEffects[spellID];

                u32 effectIndex = static_cast<u32>(effectList.effects.size());
                u32 effectIndexMask = (1 << effectIndex);

                auto& effect = effectList.effects.emplace_back();
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

                if (effect.effectType == (u8)Generated::SpellEffectTypeEnum::AuraPeriodicDamage ||
                    effect.effectType == (u8)Generated::SpellEffectTypeEnum::AuraPeriodicHeal)
                    return;

                effectList.regularEffectsMask |= effectIndexMask;
            });

            NC_LOG_INFO("Loaded Table 'spell_effects' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadSpellProcData(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_proc_data'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.spell_proc_data");
            u64 numRows = result[0][0].as<u64>();

            spellTables.procDataIDToDefinition.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'spell_proc_data'");
                return 0;
            }

            spellTables.procDataIDToDefinition.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.spell_proc_data", [&spellTables](u32 id, i32 phaseMask, i64 typeMask, i64 hitMask, i64 flags, f32 procsPerMinute, f32 chanceToProc, i32 internalCooldownMS, i32 charges)
            {
                // A proc must have a non zero PhaseMask, ExclusionMask. Charges must be non zero or -1 for infinite. It must also have either procs per minute or a chance to proc defined.
                bool isInvalidProc = phaseMask == 0 || typeMask == 0 || hitMask == 0 || charges == 0 || (procsPerMinute <= 0.0f && chanceToProc <= 0.0f);
                if (isInvalidProc) [[unlikely]]
                {
                    NC_LOG_WARNING("Invalid Spell Proc Data Definition (ID: {0})", id);
                    return;
                }

                auto& procData = spellTables.procDataIDToDefinition[id];
                procData.id = id;

                procData.phaseMask = static_cast<u32>(phaseMask);
                procData.typeMask = static_cast<u64>(typeMask);
                procData.hitMask = static_cast<u64>(hitMask);
                procData.flags = static_cast<u64>(flags);

                procData.procsPerMinute = procsPerMinute;
                procData.chanceToProc = chanceToProc;
                procData.internalCooldownMS = static_cast<u32>(internalCooldownMS);
                procData.charges = charges;
            });

            NC_LOG_INFO("Loaded Table 'spell_proc_data' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadSpellProcLink(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_proc_link'");

            pqxx::nontransaction nonTransaction = dbConnection->NewNonTransaction();

            auto result = nonTransaction.exec("SELECT COUNT(*) FROM public.spell_proc_link");
            u64 numRows = result[0][0].as<u64>();

            spellTables.spellIDToProcInfo.clear();

            if (numRows == 0)
            {
                NC_LOG_INFO("Skipped Table 'spell_proc_link'");
                return 0;
            }

            spellTables.spellIDToProcInfo.reserve(numRows);

            nonTransaction.for_stream("SELECT * FROM public.spell_proc_link ORDER BY spell_id ASC, proc_data_id ASC", [&spellTables](u32 id, u32 spellID, u64 effectMask, u32 procDataID)
            {
                auto procDataItr = spellTables.procDataIDToDefinition.find(procDataID);
                if (procDataItr == spellTables.procDataIDToDefinition.end()) [[unlikely]]
                {
                    NC_LOG_WARNING("Spell Proc Link {0} references invalid Proc Data ID {1}", id, procDataID);
                    return;
                }

                auto& effectList = spellTables.spellIDToEffects[spellID];
                auto& procInfo = spellTables.spellIDToProcInfo[spellID];

                effectList.regularEffectsMask &= ~effectMask;
                procInfo.procEffectsMask |= effectMask;

                u32 linkIndex = static_cast<u32>(procInfo.links.size());
                auto& procLink = procInfo.links.emplace_back();
                procLink.effectMask = effectMask;
                procLink.procData = procDataItr->second;

                u32 phaseMask = procLink.procData.phaseMask;
                while (phaseMask)
                {
                    u32 phaseType = std::countr_zero(phaseMask);
                    if (phaseType >= (Generated::SpellProcPhaseTypeEnumMeta::Type)Generated::SpellProcPhaseTypeEnum::Count)
                        break;

                    procInfo.phaseLinkMask[phaseType] |= (1u << linkIndex);

                    phaseMask &= ~(1u << phaseType);
                }
            });

            NC_LOG_INFO("Loaded Table 'spell_proc_link' ({0} Rows)", numRows);
            return numRows;
        }
    }
}