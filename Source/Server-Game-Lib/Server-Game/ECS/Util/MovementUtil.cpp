#include "MovementUtil.h"

#include "Server-Game/ECS/Components/MovementIntent.h"
#include "Server-Game/ECS/Components/MovementPath.h"
#include "Server-Game/ECS/Components/MovementPathRequest.h"
#include "Server-Game/ECS/Components/MovementRoute.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <entt/entt.hpp>

#include <algorithm>

namespace ECS::Util::Movement
{
    namespace
    {
        bool IsPathRequestEarlier(World& world, entt::entity leftEntity, entt::entity rightEntity)
        {
            const auto* left = world.TryGet<Components::MovementPathRequest>(leftEntity);
            const auto* right = world.TryGet<Components::MovementPathRequest>(rightEntity);
            if (!left || !right)
                return left != nullptr;

            if (left->dueTime != right->dueTime)
                return left->dueTime < right->dueTime;

            return left->sequence < right->sequence;
        }

        void SwapPathRequestHeapEntries(World& world, u32 leftIndex, u32 rightIndex)
        {
            auto& movementScheduler = world.movementScheduler;
            std::swap(movementScheduler.pathRequestHeap[leftIndex], movementScheduler.pathRequestHeap[rightIndex]);

            if (Components::MovementPathRequest* left = world.TryGet<Components::MovementPathRequest>(movementScheduler.pathRequestHeap[leftIndex]))
                left->heapIndex = leftIndex;
            if (Components::MovementPathRequest* right = world.TryGet<Components::MovementPathRequest>(movementScheduler.pathRequestHeap[rightIndex]))
                right->heapIndex = rightIndex;
        }

        void SiftPathRequestUp(World& world, u32 index)
        {
            auto& heap = world.movementScheduler.pathRequestHeap;
            while (index > 0)
            {
                u32 parentIndex = (index - 1) / 2;
                if (!IsPathRequestEarlier(world, heap[index], heap[parentIndex]))
                    break;

                SwapPathRequestHeapEntries(world, index, parentIndex);
                index = parentIndex;
            }
        }

        void SiftPathRequestDown(World& world, u32 index)
        {
            auto& heap = world.movementScheduler.pathRequestHeap;
            while (true)
            {
                u32 leftIndex = index * 2 + 1;
                if (leftIndex >= heap.size())
                    return;

                u32 rightIndex = leftIndex + 1;
                u32 smallestIndex = leftIndex;
                if (rightIndex < heap.size() && IsPathRequestEarlier(world, heap[rightIndex], heap[leftIndex]))
                    smallestIndex = rightIndex;

                if (!IsPathRequestEarlier(world, heap[smallestIndex], heap[index]))
                    return;

                SwapPathRequestHeapEntries(world, index, smallestIndex);
                index = smallestIndex;
            }
        }

        void RemovePathRequestHeapEntry(World& world, u32 index)
        {
            auto& heap = world.movementScheduler.pathRequestHeap;
            u32 lastIndex = static_cast<u32>(heap.size() - 1);
            if (index == lastIndex)
            {
                heap.pop_back();
                return;
            }

            heap[index] = heap.back();
            heap.pop_back();

            if (Components::MovementPathRequest* movedRequest = world.TryGet<Components::MovementPathRequest>(heap[index]))
                movedRequest->heapIndex = index;

            if (index > 0 && IsPathRequestEarlier(world, heap[index], heap[(index - 1) / 2]))
                SiftPathRequestUp(world, index);
            else
                SiftPathRequestDown(world, index);
        }
    }

    void CancelPathRequest(World& world, entt::entity entity)
    {
        Components::MovementPathRequest* pathRequest = world.TryGet<Components::MovementPathRequest>(entity);
        if (!pathRequest)
            return;

        RemovePathRequestHeapEntry(world, pathRequest->heapIndex);
        world.Remove<Components::MovementPathRequest>(entity);

        if (Components::MovementPath* movementPath = world.TryGet<Components::MovementPath>(entity))
            movementPath->pathRequestPending = false;
    }

    void ResetPath(World& world, entt::entity entity, Components::MovementPath& movementPath, u32 intentRevision)
    {
        CancelPathRequest(world, entity);
        world.Remove<Components::MovementRoute>(entity);

        movementPath.polyRefCurrent = 0;
        movementPath.numPositions = 0;
        movementPath.currentPositionIndex = 0;
        movementPath.repathTimer = 0.0f;
        movementPath.networkUpdateTimer = 0.0f;
        movementPath.intentRevision = intentRevision;
        movementPath.pathRequestPending = false;
        movementPath.status = Components::MovementPathStatus::None;
        movementPath.failure = Components::MovementPathFailure::None;
    }

    void QueuePathRequest(World& world, entt::entity entity, f32 delay, u32 retryCount)
    {
        if (!world.ValidEntity(entity) || !world.AllOf<Components::MovementIntent, Components::MovementPath>(entity))
            return;

        CancelPathRequest(world, entity);

        auto& movementIntent = world.Get<Components::MovementIntent>(entity);
        auto& movementPath = world.Get<Components::MovementPath>(entity);
        world.Remove<Components::MovementRoute>(entity);

        movementPath.polyRefCurrent = 0;
        movementPath.numPositions = 0;
        movementPath.currentPositionIndex = 0;
        movementPath.pathRequestPending = true;
        movementPath.status = Components::MovementPathStatus::WaitingForPath;
        movementPath.failure = Components::MovementPathFailure::None;

        auto& movementScheduler = world.movementScheduler;
        auto& pathRequest = world.Emplace<Components::MovementPathRequest>(entity, Components::MovementPathRequest{
            .dueTime = movementScheduler.time + std::max(static_cast<f64>(delay), 0.0),
            .sequence = movementScheduler.nextSequence++,
            .intentRevision = movementIntent.revision,
            .retryCount = retryCount,
            .heapIndex = static_cast<u32>(movementScheduler.pathRequestHeap.size())
        });
        movementScheduler.pathRequestHeap.push_back(entity);
        SiftPathRequestUp(world, pathRequest.heapIndex);
        world.Remove<Tags::IsMovementActive>(entity);
    }

    void AdvancePathRequests(World& world, f32 deltaTime)
    {
        world.movementScheduler.time += std::max(static_cast<f64>(deltaTime), 0.0);
    }

    bool PopDuePathRequest(World& world, entt::entity& entity, u32& intentRevision, u32& retryCount)
    {
        auto& movementScheduler = world.movementScheduler;
        while (!movementScheduler.pathRequestHeap.empty())
        {
            entt::entity current = movementScheduler.pathRequestHeap.front();
            Components::MovementPathRequest* pathRequest = world.TryGet<Components::MovementPathRequest>(current);
            if (!pathRequest)
            {
                RemovePathRequestHeapEntry(world, 0);
                continue;
            }

            if (pathRequest->dueTime > movementScheduler.time)
                return false;

            entity = current;
            intentRevision = pathRequest->intentRevision;
            retryCount = pathRequest->retryCount;
            RemovePathRequestHeapEntry(world, 0);
            world.Remove<Components::MovementPathRequest>(current);

            if (Components::MovementPath* movementPath = world.TryGet<Components::MovementPath>(current))
                movementPath->pathRequestPending = false;

            return true;
        }

        return false;
    }

    void Follow(World& world, entt::entity entity, entt::entity target, const FollowParams& params)
    {
        if (world.AllOf<Tags::IsDead>(entity))
            return;

        auto& movementIntent = world.GetOrEmplace<Components::MovementIntent>(entity);
        movementIntent.type = Components::MovementIntentType::Follow;
        movementIntent.follow.target = target;
        movementIntent.follow.repathInterval = params.repathInterval;
        movementIntent.follow.repathDistance = params.repathDistance;
        movementIntent.follow.stopDistance = std::max(params.stopDistance, 0.0f);
        movementIntent.speed = params.speed;
        movementIntent.revision++;

        auto& movementPath = world.GetOrEmplace<Components::MovementPath>(entity);
        ResetPath(world, entity, movementPath, movementIntent.revision);
        QueuePathRequest(world, entity, 0.0f);
    }

    void MoveTo(World& world, entt::entity entity, const vec3& position, const PointParams& params)
    {
        if (world.AllOf<Tags::IsDead>(entity))
            return;

        auto& movementIntent = world.GetOrEmplace<Components::MovementIntent>(entity);
        movementIntent.type = Components::MovementIntentType::Point;
        movementIntent.point.position = position;
        movementIntent.point.repathDistance = params.repathDistance;
        movementIntent.point.clearOnReach = params.clearOnReach;
        movementIntent.speed = params.speed;
        movementIntent.revision++;

        auto& movementPath = world.GetOrEmplace<Components::MovementPath>(entity);
        ResetPath(world, entity, movementPath, movementIntent.revision);
        QueuePathRequest(world, entity, 0.0f);
    }

    void Wander(World& world, entt::entity entity, const vec3& center, const WanderParams& params)
    {
        if (world.AllOf<Tags::IsDead>(entity))
            return;

        auto& movementIntent = world.GetOrEmplace<Components::MovementIntent>(entity);
        movementIntent.type = Components::MovementIntentType::Wander;
        movementIntent.wander.center = center;
        movementIntent.wander.radius = params.radius;
        movementIntent.wander.minPause = params.minPause;
        movementIntent.wander.maxPause = params.maxPause;
        movementIntent.wander.repathDistance = params.repathDistance;
        movementIntent.speed = params.speed;
        movementIntent.revision++;

        auto& movementPath = world.GetOrEmplace<Components::MovementPath>(entity);
        ResetPath(world, entity, movementPath, movementIntent.revision);
        QueuePathRequest(world, entity, world.rng.NextF32() * 0.5f);
    }

    void Stop(World& world, entt::entity entity)
    {
        if (auto* movementIntent = world.TryGet<Components::MovementIntent>(entity))
        {
            movementIntent->type = Components::MovementIntentType::None;
            movementIntent->follow.target = entt::null;
            movementIntent->revision++;

            if (auto* movementPath = world.TryGet<Components::MovementPath>(entity))
                ResetPath(world, entity, *movementPath, movementIntent->revision);

            world.Remove<Tags::IsMovementActive>(entity);
        }
    }
}
