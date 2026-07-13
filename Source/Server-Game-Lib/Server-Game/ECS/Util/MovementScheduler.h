#pragma once

#include <Base/Types.h>

#include <vector>

#include <entt/entity/entity.hpp>

namespace ECS::Util
{
    struct MovementStepJob
    {
    public:
        entt::entity entity = entt::null;
        ObjectGUID guid;

        vec3 previousPosition = vec3(0.0f);
        vec3 cornerTarget = vec3(0.0f);
        vec2 previousPitchYaw = vec2(0.0f);
        f32 speed = 0.0f;
        f32 simulationDelta = 0.0f;
    };

    struct MovementStepResult
    {
    public:
        ObjectGUID guid;
        vec3 position = vec3(0.0f);
        vec2 pitchYaw = vec2(0.0f);
        entt::entity entity = entt::null;
    };

    struct MovementProfileStats
    {
    public:
        void Reset()
        {
            *this = {};
        }

        f64 elapsedSeconds = 0.0;
        u64 tickCount = 0;
        u64 totalMicroseconds = 0;
        u64 pathDispatchMicroseconds = 0;
        u64 findPathMicroseconds = 0;
        u64 activeMovementMicroseconds = 0;
        u64 executeMovementMicroseconds = 0;
        u64 resultMovementMicroseconds = 0;
        u64 pathDequeues = 0;
        u64 pathQueries = 0;
        u64 pathSuccesses = 0;
        u64 pathFailures = 0;
        u64 activeMovers = 0;
    };

    struct MovementScheduler
    {
    public:
        void ReservePathRequests(u32 capacity)
        {
            pathRequestHeap.reserve(capacity);
        }

        f64 time = 0.0;
        u64 nextSequence = 0;
        std::vector<entt::entity> pathRequestHeap;
        std::vector<MovementStepJob> stepJobs;
        std::vector<MovementStepResult> stepResults;
        MovementProfileStats profileStats;
    };
}
