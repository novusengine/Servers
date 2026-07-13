#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct CharacterSpellCastInfo
    {
    public:
        entt::entity activeSpellEntity;
        entt::entity queuedSpellEntity;
    };
}