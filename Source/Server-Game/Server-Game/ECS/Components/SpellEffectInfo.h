#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <robinhood/robinhood.h>

namespace ECS::Components
{
    struct SpellEffectInfo
    {
    public:
        std::vector<GameDefine::Database::SpellEffect> effects;
    };
}