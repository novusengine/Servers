#pragma once

#include <Base/Types.h>

#include <string_view>

namespace Gameplay::Faction
{
    inline constexpr f32 MAX_CREATURE_DETECTION_RANGE = 150.0f;
    inline constexpr f32 MAX_CREATURE_ASSISTANCE_RANGE = 40.0f;

    enum class CreatureAggressionPolicy : u8
    {
        Passive,
        Defensive,
        Aggressive
    };

    enum class CreatureAssistancePolicy : u8
    {
        None,
        SameFaction,
        Friendly
    };

    constexpr bool IsValidCreatureAggressionPolicy(u16 value)
    {
        return value <= static_cast<u16>(CreatureAggressionPolicy::Aggressive);
    }

    constexpr bool IsValidCreatureAssistancePolicy(u16 value)
    {
        return value <= static_cast<u16>(CreatureAssistancePolicy::Friendly);
    }

    constexpr std::string_view CreatureAggressionPolicyName(CreatureAggressionPolicy policy)
    {
        switch (policy)
        {
            case CreatureAggressionPolicy::Passive:
                return "Passive";
            case CreatureAggressionPolicy::Defensive:
                return "Defensive";
            case CreatureAggressionPolicy::Aggressive:
                return "Aggressive";
            default:
                return "Invalid";
        }
    }

    constexpr std::string_view CreatureAssistancePolicyName(CreatureAssistancePolicy policy)
    {
        switch (policy)
        {
            case CreatureAssistancePolicy::None:
                return "None";
            case CreatureAssistancePolicy::SameFaction:
                return "SameFaction";
            case CreatureAssistancePolicy::Friendly:
                return "Friendly";
            default:
                return "Invalid";
        }
    }
}
