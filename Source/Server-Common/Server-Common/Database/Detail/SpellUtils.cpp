#include "SpellUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>

#include <MetaGen/Postgres/World/Tables/SpellProcData.h>
#include <MetaGen/Postgres/World/Tables/SpellEffects.h>
#include <MetaGen/Postgres/World/Tables/SpellProcLink.h>
#include <MetaGen/Postgres/World/Tables/Spells.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Shared/Spell/Spell.h>

namespace Database::Detail::Spell
{
    namespace Loading
    {
        void LoadSpellTables(pqxx::read_transaction& transaction, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("-- Loading Spell Tables --");

            u64 totalRows = 0;
            totalRows += LoadSpells(transaction, spellTables);
            totalRows += LoadSpellEffects(transaction, spellTables);
            totalRows += LoadSpellProcData(transaction, spellTables);
            totalRows += LoadSpellProcLink(transaction, spellTables);

            NC_LOG_INFO("-- Loaded Spell Tables ({0} rows) --\n", totalRows);
        }

        u64 LoadSpells(pqxx::read_transaction& transaction, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spells'");

            spellTables.idToDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::SpellsTable>(transaction, [&spellTables, &numRows](auto record)
            {
                spellTables.idToDefinition[record.id] = {
                    record.id, std::move(record.name), std::move(record.description), std::move(record.auraDescription),
                    record.iconId, record.castTime, record.cooldown, record.duration
                };
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'spells' ({0} Rows)", numRows);
            return numRows;
        }
        u64 LoadSpellEffects(pqxx::read_transaction& transaction, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_effects'");

            spellTables.spellIDToEffects.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::SpellEffectsTable>(transaction, [&spellTables, &numRows](const auto& record)
            {
                ++numRows;
                auto& effectList = spellTables.spellIDToEffects[record.spellId];

                u32 effectIndex = static_cast<u32>(effectList.effects.size());
                u32 effectIndexMask = (1 << effectIndex);

                auto& effect = effectList.effects.emplace_back();
                effect.id = record.id;
                effect.spellID = record.spellId;
                effect.effectPriority = static_cast<u8>(record.effectPriority);
                effect.effectType = static_cast<u8>(record.effectType);
                effect.effectValue1 = record.effectValue1;
                effect.effectValue2 = record.effectValue2;
                effect.effectValue3 = record.effectValue3;
                effect.effectMiscValue1 = record.effectMiscValue1;
                effect.effectMiscValue2 = record.effectMiscValue2;
                effect.effectMiscValue3 = record.effectMiscValue3;

                if (effect.effectType == (u8)MetaGen::Shared::Spell::SpellEffectTypeEnum::AuraPeriodicDamage ||
                    effect.effectType == (u8)MetaGen::Shared::Spell::SpellEffectTypeEnum::AuraPeriodicHeal)
                    return;

                effectList.regularEffectsMask |= effectIndexMask;
            });

            NC_LOG_INFO("Loaded Table 'spell_effects' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadSpellProcData(pqxx::read_transaction& transaction, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_proc_data'");

            spellTables.procDataIDToDefinition.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::SpellProcDataTable>(transaction, [&spellTables, &numRows](const auto& record)
            {
                // A proc must have a non zero PhaseMask, ExclusionMask. Charges must be non zero or -1 for infinite. It must also have either procs per minute or a chance to proc defined.
                bool isInvalidProc = record.phaseMask == 0 || record.typeMask == 0 || record.hitMask == 0 || record.charges == 0 || (record.procsPerMin <= 0.0f && record.chanceToProc <= 0.0f);
                if (isInvalidProc) [[unlikely]]
                {
                    NC_LOG_WARNING("Invalid Spell Proc Data Definition (ID: {0})", record.id);
                    return;
                }

                auto& procData = spellTables.procDataIDToDefinition[record.id];
                procData.id = record.id;
                procData.phaseMask = static_cast<u32>(record.phaseMask);
                procData.typeMask = static_cast<u64>(record.typeMask);
                procData.hitMask = static_cast<u64>(record.hitMask);
                procData.flags = static_cast<u64>(record.flags);
                procData.procsPerMinute = record.procsPerMin;
                procData.chanceToProc = record.chanceToProc;
                procData.internalCooldownMS = static_cast<u32>(record.internalCooldownMs);
                procData.charges = record.charges;
                ++numRows;
            });

            NC_LOG_INFO("Loaded Table 'spell_proc_data' ({0} Rows)", numRows);
            return numRows;
        }

        u64 LoadSpellProcLink(pqxx::read_transaction& transaction, Database::SpellTables& spellTables)
        {
            NC_LOG_INFO("Loading Table 'spell_proc_link'");

            spellTables.spellIDToProcInfo.clear();
            u64 numRows = 0;
            Generated::ForEach<MetaGen::Postgres::World::SpellProcLinkTable>(transaction, [&spellTables, &numRows](const auto& record)
            {
                ++numRows;
                auto procDataItr = spellTables.procDataIDToDefinition.find(record.procDataId);
                if (procDataItr == spellTables.procDataIDToDefinition.end()) [[unlikely]]
                {
                    NC_LOG_WARNING("Spell Proc Link {0} references invalid Proc Data ID {1}", record.id, record.procDataId);
                    return;
                }

                auto& effectList = spellTables.spellIDToEffects[record.spellId];
                auto& procInfo = spellTables.spellIDToProcInfo[record.spellId];

                effectList.regularEffectsMask &= ~record.effectMask;
                procInfo.procEffectsMask |= record.effectMask;

                u32 linkIndex = static_cast<u32>(procInfo.links.size());
                auto& procLink = procInfo.links.emplace_back();
                procLink.effectMask = record.effectMask;
                procLink.procData = procDataItr->second;

                u32 phaseMask = procLink.procData.phaseMask;
                while (phaseMask)
                {
                    u32 phaseType = std::countr_zero(phaseMask);
                    if (phaseType >= (MetaGen::Shared::Spell::SpellProcPhaseTypeEnumMeta::Type)MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::Count)
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
