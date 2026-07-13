#pragma once
#include <Base/Types.h>

#include <MetaGen/Shared/Spell/Spell.h>

#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct ProcInfo
    {
    public:
        u32 procDataID;
        u32 phaseMask;
        u64 typeMask;
        u64 hitMask;
        u64 flags;
        u64 effectMask;
        u64 lastProcTime;
        u32 internalCooldownMS;
        f32 procsPerMinute;
        f32 chanceToProc;
        i32 charges;
    };

    struct SpellProcInfo
    {
    public:
        u64 procEffectsMask = 0;

        std::array<u16, (MetaGen::Shared::Spell::SpellProcPhaseTypeEnumMeta::Type)MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::Count> phaseProcMask = { 0u, 0u, 0u, 0u, 0u, 0u };
        std::vector<ProcInfo> procInfos;
    };
}