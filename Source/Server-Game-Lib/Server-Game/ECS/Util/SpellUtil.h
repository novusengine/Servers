#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <MetaGen/Shared/Spell/Spell.h>

#include <entt/fwd.hpp>

namespace ECS
{
    struct World;

    namespace Components
    {
        struct AuraEffectInfo;
        struct AuraInfo;
        struct ProcInfo;
        struct SpellEffectInfo;
        struct SpellInfo;
        struct SpellProcInfo;
    }

    namespace Singletons
    {
        struct GameCache;
        struct NetworkState;
        struct TimeState;
    }
}

namespace Scripting
{
    struct Zenith;
}

namespace ECS::Util::Spell
{
    bool SetupSpellProcInfo(World& world, Singletons::GameCache& gameCache, u32 spellID, entt::entity spellEntity);
    bool SetupAuraProcInfo(World& world, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, u32 spellID, entt::entity spellEntity);

    bool CanSpellProc(const Singletons::TimeState& timeState, Components::ProcInfo& procInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, MetaGen::Shared::Spell::SpellProcTypeMaskEnum typeMask, u64 lastProcTime);
    void CheckSpellProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::SpellEffectInfo& spellEffectInfo, Components::SpellProcInfo& spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, u32 spellID, entt::entity spellEntity, entt::entity casterEntity);
    void CheckSpellEffectProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::SpellEffectInfo& spellEffectInfo, Components::SpellProcInfo& spellProcInfo, u32 spellID, entt::entity spellEntity, entt::entity casterEntity, u32 effectIndex);
    
    void CheckAuraProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, Components::SpellProcInfo& spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum phaseType, u32 spellID, entt::entity auraEntity, entt::entity casterEntity, entt::entity targetEntity);
    void CheckAuraEffectProc(World& world, Scripting::Zenith* zenith, const Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Components::AuraEffectInfo& auraEffectInfo, Components::SpellProcInfo& spellProcInfo, u32 spellID, entt::entity auraEntity, entt::entity casterEntity, entt::entity targetEntity, u32 effectIndex);
}