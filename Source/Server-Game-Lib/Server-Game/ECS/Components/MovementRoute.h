#pragma once

#include <Base/Types.h>

#include <Detour/DetourNavMesh.h>

namespace ECS::Components
{
    struct MovementRoute
    {
    public:
        static constexpr u32 MAX_POSITIONS = 32;

        dtPolyRef polyRefStart = 0;
        dtPolyRef polyRefEnd = 0;
        vec3 positions[MAX_POSITIONS];
    };
}
