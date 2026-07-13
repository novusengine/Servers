#pragma once
#include <Gameplay/GameDefine.h>

namespace ECS::Components
{
    struct ObjectInfo
    {
    public:
        ObjectGUID guid = ObjectGUID::Empty;
    };
}