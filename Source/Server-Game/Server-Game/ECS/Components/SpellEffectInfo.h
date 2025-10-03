#pragma once
#include <Gameplay/GameDefine.h>

#include <vector>

namespace ECS::Components
{
    struct SpellEffectInfo
    {
    public:
        u64 regularEffectsMask = 0;
        std::vector<GameDefine::Database::SpellEffect> effects;
    };
}