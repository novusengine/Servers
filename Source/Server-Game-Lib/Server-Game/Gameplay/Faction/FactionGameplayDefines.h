#pragma once

#include <Base/Types.h>

namespace Gameplay::Faction
{
    enum class DiscoverySource : u8
    {
        Interaction,
        Target
    };

    enum class ReputationSourceType : u8
    {
        None,
        DiscoveryInteraction,
        DiscoveryTarget,
        CreatureKill,
        Quest,
        Spell,
        Item,
        Script,
        Administrative
    };

    struct ReputationSource
    {
    public:
        ReputationSourceType type = ReputationSourceType::None;
        u32 contentID = 0;
    };
}
