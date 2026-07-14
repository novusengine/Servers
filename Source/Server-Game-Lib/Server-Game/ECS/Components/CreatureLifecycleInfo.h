#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    enum class CreatureLifecycleState : u8
    {
        Alive,
        Corpse,
        Despawned
    };

    struct CreatureLifecycleInfo
    {
    public:
        CreatureLifecycleState state = CreatureLifecycleState::Alive;
        f32 timeRemaining = 0.0f;
        u32 spawnTimeInSecMin = 120;
        u32 spawnTimeInSecMax = 120;
    };
}
