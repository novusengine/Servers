#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>
#include <Network/Define.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
    struct ObjectInfo
    {
    public:
        ObjectGUID guid = ObjectGUID::Empty;
    };
}