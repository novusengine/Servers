#include "SpellUtil.h"
#include "Cache/CacheUtil.h"
#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Components/SpellInfo.h"
#include "Server-Game/ECS/Components/SpellProcInfo.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Server/Lua/Lua.h>
#include <MetaGen/Shared/Spell/Spell.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

using namespace ECS;

namespace ECS::Util::Spell
{
    bool SetupSpellProcInfo(World& world, Singletons::GameCache& gameCache, u32 spellID, entt::entity spellEntity)
    {
        auto dbSpellProcInfoItr = gameCache.spellTables.spellIDToProcInfo.find(spellID);
        if (dbSpellProcInfoItr == gameCache.spellTables.spellIDToProcInfo.end())
            return false;

        auto& spellProcInfo = world.Emplace<Components::SpellProcInfo>(spellEntity);

        const Database::SpellProcInfo& dbSpellProcInfo = dbSpellProcInfoItr->second;
        u32 numProcLinks = static_cast<u32>(dbSpellProcInfo.links.size());
        spellProcInfo.procInfos.reserve(numProcLinks);

        for (const Database::SpellProcLink& procLink : dbSpellProcInfo.links)
        {
            Components::ProcInfo& procInfo = spellProcInfo.procInfos.emplace_back();

            procInfo.procDataID = procLink.procData.id;
            procInfo.phaseMask = procLink.procData.phaseMask;
            procInfo.typeMask = procLink.procData.typeMask;
            procInfo.hitMask = procLink.procData.hitMask;
            procInfo.flags = procLink.procData.flags;
            procInfo.effectMask = procLink.effectMask;
            procInfo.lastProcTime = 0;
            procInfo.internalCooldownMS = procLink.procData.internalCooldownMS;
            procInfo.procsPerMinute = procLink.procData.procsPerMinute;
            procInfo.chanceToProc = procLink.procData.chanceToProc;
            procInfo.charges = procLink.procData.charges;
        }

        spellProcInfo.procEffectsMask = dbSpellProcInfo.procEffectsMask;
        spellProcInfo.phaseProcMask = dbSpellProcInfo.phaseLinkMask;

        return true;
    }

    bool SetupAuraProcInfo(World& world, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, u32 spellID, entt::entity spellEntity)
    {
        auto dbSpellProcInfoItr = gameCache.spellTables.spellIDToProcInfo.find(spellID);
        if (dbSpellProcInfoItr == gameCache.spellTables.spellIDToProcInfo.end())
            return false;

        auto& spellProcInfo = world.Emplace<Components::SpellProcInfo>(spellEntity);

        const Database::SpellProcInfo& dbSpellProcInfo = dbSpellProcInfoItr->second;
        u32 numProcLinks = static_cast<u32>(dbSpellProcInfo.links.size());
        spellProcInfo.procInfos.reserve(numProcLinks);

        for (const Database::SpellProcLink& procLink : dbSpellProcInfo.links)
        {
            Components::ProcInfo& procInfo = spellProcInfo.procInfos.emplace_back();

            procInfo.procDataID = procLink.procData.id;
            procInfo.phaseMask = procLink.procData.phaseMask;
            procInfo.typeMask = procLink.procData.typeMask;
            procInfo.hitMask = procLink.procData.hitMask;
            procInfo.flags = procLink.procData.flags;
            procInfo.effectMask = procLink.effectMask;
            procInfo.lastProcTime = 0;
            procInfo.internalCooldownMS = procLink.procData.internalCooldownMS;
            procInfo.procsPerMinute = procLink.procData.procsPerMinute;
            procInfo.chanceToProc = procLink.procData.chanceToProc;
            procInfo.charges = procLink.procData.charges;
        }

        spellProcInfo.procEffectsMask = dbSpellProcInfo.procEffectsMask;
        spellProcInfo.phaseProcMask = dbSpellProcInfo.phaseLinkMask;

        // Remove Periodic Effects from Proc Effects Mask
        spellProcInfo.procEffectsMask &= ~(auraEffectInfo.periodicEffectsMask);

        return true;
    }

    bool CanSpellProc(const Singletons::TimeState& timeState, Components::ProcInfo& procInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum typeMask, u64 lastProcTime)
    {
        u32 phaseMask = (1u << static_cast<u32>(phaseType));

        bool phaseMaskPass = ((procInfo.phaseMask & (u32)phaseMask) != 0);
        bool typeMaskPass = ((procInfo.typeMask & (u32)typeMask) != 0);
        bool cooldownPass = (lastProcTime + procInfo.internalCooldownMS) <= timeState.epochAtFrameStart;
        bool chargesPass = (procInfo.charges != 0);

        bool canProcMask = phaseMaskPass && typeMaskPass && cooldownPass && chargesPass;
        return canProcMask;
    }
    void CheckSpellProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::SpellEffectInfo& spellEffectInfo, Components::SpellProcInfo& spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, u32 spellID, entt::entity spellEntity, entt::entity casterEntity)
    {
        auto* unitAuraInfo = world.TryGet<ECS::Components::UnitAuraInfo>(casterEntity);

        u16 phaseProcMask = spellProcInfo.phaseProcMask[(u8)phaseType];
        while (phaseProcMask)
        {
            u32 procIndex = std::countr_zero(phaseProcMask);
            phaseProcMask &= ~(1u << procIndex);

            ECS::Components::ProcInfo& procInfo = spellProcInfo.procInfos[procIndex];
            bool useUnitICD = unitAuraInfo && (procInfo.flags & (u64)MetaGen::Shared::Spell::SpellProcFlagEnum::UseUnitICD) != 0;
            u64 lastProcTime = useUnitICD ? unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] : procInfo.lastProcTime;

            if (!CanSpellProc(timeState, procInfo, phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum::All, lastProcTime))
                continue;

            // Perform Proc Chance Roll
            f32 roll = world.rng.NextF32();
            f32 chanceToProc = procInfo.chanceToProc;

            if (procInfo.procsPerMinute > 0.0f)
            {
                // PPM system: chance = (procsPerMinute * elapsedSeconds / 60)

                f32 secondsSinceLastProc = (timeState.epochAtFrameStart - procInfo.lastProcTime) * 0.001f;
                chanceToProc = (procInfo.procsPerMinute * secondsSinceLastProc) / 60.0f;
            }

            bool rollSucceeded = roll <= chanceToProc;
            if (rollSucceeded)
            {
                u16 numSpellEffects = static_cast<u16>(spellEffectInfo.effects.size());
                u64 effectMask = procInfo.effectMask;
                u32 effectIndex = 0;

                while (effectMask && effectIndex < numSpellEffects)
                {
                    effectIndex = std::countr_zero(effectMask);
                    u8 effectType = spellEffectInfo.effects[effectIndex].effectType;

                    zenith->CallEvent(MetaGen::Server::Lua::SpellEvent::OnHandleEffect, MetaGen::Server::Lua::SpellEventDataOnHandleEffect{
                        .casterID = entt::to_integral(casterEntity),
                        .spellEntity = entt::to_integral(spellEntity),
                        .spellID = spellID,
                        .procID = procInfo.procDataID,
                        .effectIndex = static_cast<u8>(effectIndex),
                        .effectType = effectType
                    });

                    effectMask &= ~(1ull << effectIndex);
                }

                bool usesCharges = procInfo.charges != -1;
                procInfo.charges = usesCharges ? glm::max(0, procInfo.charges - 1) : -1;
                procInfo.lastProcTime = timeState.epochAtFrameStart;
                if (unitAuraInfo && useUnitICD)
                {
                    unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] = procInfo.lastProcTime;
                }
            }
        }
    }
    void CheckSpellEffectProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::SpellEffectInfo& spellEffectInfo, Components::SpellProcInfo& spellProcInfo, u32 spellID, entt::entity spellEntity, entt::entity casterEntity, u32 effectIndex)
    {
        auto phaseType = MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnSpellHandleEffect;

        u64 effectIndexMask = (1ull << effectIndex);
        bool isProcEffect = (spellProcInfo.procEffectsMask & effectIndexMask) != 0;

        auto* unitAuraInfo = world.TryGet<ECS::Components::UnitAuraInfo>(casterEntity);

        // We can avoid a branch here by multiplying by isProcEffect (0 or 1)
        u16 phaseProcMask = spellProcInfo.phaseProcMask[(u8)phaseType] * isProcEffect;
        while (phaseProcMask)
        {
            u32 procIndex = std::countr_zero(phaseProcMask);
            phaseProcMask &= ~(1u << procIndex);

            ECS::Components::ProcInfo& procInfo = spellProcInfo.procInfos[procIndex];
            bool useUnitICD = unitAuraInfo && (procInfo.flags & (u64)MetaGen::Shared::Spell::SpellProcFlagEnum::UseUnitICD) != 0;
            u64 lastProcTime = useUnitICD ? unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] : procInfo.lastProcTime;

            if (!CanSpellProc(timeState, procInfo, phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum::All, lastProcTime))
                continue;

            // Perform Proc Chance Roll
            f32 roll = world.rng.NextF32();
            f32 chanceToProc = procInfo.chanceToProc;

            if (procInfo.procsPerMinute > 0.0f)
            {
                // PPM system: chance = (procsPerMinute * elapsedSeconds / 60)

                f32 secondsSinceLastProc = (timeState.epochAtFrameStart - procInfo.lastProcTime) * 0.001f;
                chanceToProc = (procInfo.procsPerMinute * secondsSinceLastProc) / 60.0f;
            }

            bool rollSucceeded = roll <= chanceToProc;
            if (rollSucceeded)
            {
                u8 effectType = spellEffectInfo.effects[effectIndex].effectType;

                zenith->CallEvent(MetaGen::Server::Lua::SpellEvent::OnHandleEffect, MetaGen::Server::Lua::SpellEventDataOnHandleEffect{
                    .casterID = entt::to_integral(casterEntity),
                    .spellEntity = entt::to_integral(spellEntity),
                    .spellID = spellID,
                    .procID = procInfo.procDataID,
                    .effectIndex = static_cast<u8>(effectIndex),
                    .effectType = effectType
                });

                bool usesCharges = procInfo.charges != -1;
                procInfo.charges = usesCharges ? glm::max(0, procInfo.charges - 1) : -1;
                procInfo.lastProcTime = timeState.epochAtFrameStart;
                if (unitAuraInfo && useUnitICD)
                {
                    unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] = procInfo.lastProcTime;
                }

                if (procInfo.charges == 0)
                {
                    // Remove from all phase masks

                    u32 phaseMask = procInfo.phaseMask;
                    u16 invProcIndexMask = ~(1u << procIndex);

                    while (phaseMask)
                    {
                        u32 phaseType = std::countr_zero(phaseMask);
                        spellProcInfo.phaseProcMask[phaseType] &= invProcIndexMask;
                        phaseMask &= ~(1u << phaseType);
                    }
                }
            }
        }
    }
    
    void CheckAuraProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, Components::SpellProcInfo& spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, u32 spellID, entt::entity auraEntity, entt::entity casterEntity, entt::entity targetEntity)
    {
        auto* unitAuraInfo = world.TryGet<ECS::Components::UnitAuraInfo>(casterEntity);

        u16 phaseProcMask = spellProcInfo.phaseProcMask[(u8)phaseType];
        while (phaseProcMask)
        {
            u32 procIndex = std::countr_zero(phaseProcMask);
            phaseProcMask &= ~(1u << procIndex);

            ECS::Components::ProcInfo& procInfo = spellProcInfo.procInfos[procIndex];
            bool useUnitICD = unitAuraInfo && (procInfo.flags & (u64)MetaGen::Shared::Spell::SpellProcFlagEnum::UseUnitICD) != 0;
            u64 lastProcTime = useUnitICD ? unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] : procInfo.lastProcTime;

            if (!CanSpellProc(timeState, procInfo, phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum::All, lastProcTime))
                continue;

            // Perform Proc Chance Roll
            f32 roll = world.rng.NextF32();
            f32 chanceToProc = procInfo.chanceToProc;

            if (procInfo.procsPerMinute > 0.0f)
            {
                // PPM system: chance = (procsPerMinute * elapsedSeconds / 60)

                f32 secondsSinceLastProc = (timeState.epochAtFrameStart - procInfo.lastProcTime) * 0.001f;
                chanceToProc = (procInfo.procsPerMinute * secondsSinceLastProc) / 60.0f;
            }

            bool rollSucceeded = roll <= chanceToProc;
            if (rollSucceeded)
            {
                u16 numSpellEffects = static_cast<u16>(auraEffectInfo.effects.size());
                u64 effectMask = procInfo.effectMask;
                u32 effectIndex = 0;

                while (effectMask && effectIndex < numSpellEffects)
                {
                    effectIndex = std::countr_zero(effectMask);
                    u8 effectType = auraEffectInfo.effects[effectIndex].type;

                    zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnHandleEffect, MetaGen::Server::Lua::AuraEventDataOnHandleEffect{
                        .casterID = entt::to_integral(casterEntity),
                        .targetID = entt::to_integral(targetEntity),
                        .auraEntity = entt::to_integral(auraEntity),
                        .spellID = spellID,
                        .procID = procInfo.procDataID,
                        .effectIndex = static_cast<u8>(effectIndex),
                        .effectType = effectType
                    });

                    effectMask &= ~(1ull << effectIndex);
                }

                bool usesCharges = procInfo.charges != -1;
                procInfo.charges = usesCharges ? glm::max(0, procInfo.charges - 1) : -1;
                procInfo.lastProcTime = timeState.epochAtFrameStart;
                if (unitAuraInfo && useUnitICD)
                {
                    unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] = procInfo.lastProcTime;
                }
            }
        }
    }
    void CheckAuraEffectProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, Components::SpellProcInfo& spellProcInfo, u32 spellID, entt::entity auraEntity, entt::entity casterEntity, entt::entity targetEntity, u32 effectIndex)
    {
        auto phaseType = MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnAuraHandleEffect;

        u64 effectIndexMask = (1ull << effectIndex);
        bool isProcEffect = (spellProcInfo.procEffectsMask & effectIndexMask) != 0;

        auto* unitAuraInfo = world.TryGet<ECS::Components::UnitAuraInfo>(casterEntity);

        // We can avoid a branch here by multiplying by isProcEffect (0 or 1)
        u16 phaseProcMask = spellProcInfo.phaseProcMask[(u8)phaseType] * isProcEffect;
        while (phaseProcMask)
        {
            u32 procIndex = std::countr_zero(phaseProcMask);
            phaseProcMask &= ~(1u << procIndex);

            ECS::Components::ProcInfo& procInfo = spellProcInfo.procInfos[procIndex];
            bool useUnitICD = unitAuraInfo && (procInfo.flags & (u64)MetaGen::Shared::Spell::SpellProcFlagEnum::UseUnitICD) != 0;
            u64 lastProcTime = useUnitICD ? unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] : procInfo.lastProcTime;

            if (!CanSpellProc(timeState, procInfo, phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum::All, lastProcTime))
                continue;

            // Perform Proc Chance Roll
            f32 roll = world.rng.NextF32();
            f32 chanceToProc = procInfo.chanceToProc;

            if (procInfo.procsPerMinute > 0.0f)
            {
                // PPM system: chance = (procsPerMinute * elapsedSeconds / 60)

                f32 secondsSinceLastProc = (timeState.epochAtFrameStart - procInfo.lastProcTime) * 0.001f;
                chanceToProc = (procInfo.procsPerMinute * secondsSinceLastProc) / 60.0f;
            }

            bool rollSucceeded = roll <= chanceToProc;
            if (rollSucceeded)
            {
                u8 effectType = auraEffectInfo.effects[effectIndex].type;

                zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnHandleEffect, MetaGen::Server::Lua::AuraEventDataOnHandleEffect{
                    .casterID = entt::to_integral(casterEntity),
                    .targetID = entt::to_integral(targetEntity),
                    .auraEntity = entt::to_integral(auraEntity),
                    .spellID = spellID,
                    .procID = procInfo.procDataID,
                    .effectIndex = static_cast<u8>(effectIndex),
                    .effectType = effectType
                });

                bool usesCharges = procInfo.charges != -1;
                procInfo.charges = usesCharges ? glm::max(0, procInfo.charges - 1) : -1;
                procInfo.lastProcTime = timeState.epochAtFrameStart;
                if (unitAuraInfo && useUnitICD)
                {
                    unitAuraInfo->procIDToLastProcTime[procInfo.procDataID] = procInfo.lastProcTime;
                }

                if (procInfo.charges == 0)
                {
                    // Remove from all phase masks

                    u32 phaseMask = procInfo.phaseMask;
                    u16 invProcIndexMask = ~(1u << procIndex);

                    while (phaseMask)
                    {
                        u32 phaseType = std::countr_zero(phaseMask);
                        spellProcInfo.phaseProcMask[phaseType] &= invProcIndexMask;
                        phaseMask &= ~(1u << phaseType);
                    }
                }
            }
        }
    }
}