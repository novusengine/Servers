#pragma once
#include <Base/Types.h>

#include <Detour/DetourNavMesh.h>

#include <cstddef>

namespace ECS::Components
{
    enum class MovementPathStatus : u8
    {
        None,
        Waiting,
        WaitingForPath,
        Moving,
        Reached,
        Failed
    };

    enum class MovementPathFailure : u8
    {
        None,
        InvalidIntent,
        PathNotFound,
        InvalidSurface
    };

    struct MovementPath
    {
    public:
        dtPolyRef polyRefCurrent = 0;
        vec3 requestedEndPosition = vec3(0.0f);
        f32 repathTimer = 0.0f;
        f32 networkUpdateTimer = 0.0f;
        u32 intentRevision = 0;
        u32 numPositions = 0;
        u32 currentPositionIndex = 0;
        bool pathRequestPending = false;
        MovementPathStatus status = MovementPathStatus::None;
        MovementPathFailure failure = MovementPathFailure::None;
    };

    static_assert(sizeof(MovementPath) <= 64);
}
