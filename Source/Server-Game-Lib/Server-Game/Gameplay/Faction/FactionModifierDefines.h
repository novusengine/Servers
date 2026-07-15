#pragma once

#include "Server-Game/Gameplay/Faction/FactionRuntimeDefines.h"

#include <Base/Types.h>

namespace Gameplay::Faction
{
    enum class ModifierField : u8
    {
        EffectiveFaction,
        LocalStanding,
        LocalReaction,
        PlayerReactionMinimum,
        PlayerReactionMaximum,
        UnitReaction,
        Count
    };

    enum class PerceptionOverrideFields : u8
    {
        None = 0,
        Standing = 1 << 0,
        Reaction = 1 << 1
    };

    struct ModifierContributor
    {
    public:
        u32 sourceAuraInstanceID = 0;
        u32 spellID = 0;
        u32 effectID = 0;
        ModifierField field = ModifierField::EffectiveFaction;
        u8 effectIndex = 0;
        u8 priority = 0;
        u64 applicationSequence = 0;
        FactionIndex factionSelector = NONE_FACTION_INDEX;
        i32 value = 0;
    };

    struct FactionModifierConflict
    {
    public:
        ModifierField field = ModifierField::EffectiveFaction;
        u8 priority = 0;
        u32 incumbentSpellID = 0;
        u32 incumbentEffectID = 0;
        i32 incumbentOutcome = 0;
        u64 incumbentApplicationSequence = 0;
        u32 contenderSpellID = 0;
        u32 contenderEffectID = 0;
        i32 contenderOutcome = 0;
        u64 contenderApplicationSequence = 0;
        u32 unitEntity = 0;
        u32 mapID = 0;
        u64 contentVersion = 0;
    };

    using FactionModifierConflictReporter = void (*)(const FactionModifierConflict& conflict);

    // A future asynchronous logging service will provide the audit sink. Modifier application must never perform I/O.
    void ReportFactionModifierConflict(const FactionModifierConflict& conflict);
}
