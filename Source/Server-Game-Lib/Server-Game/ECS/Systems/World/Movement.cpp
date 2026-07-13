#include "Movement.h"

#include "Server-Game/ECS/Components/MovementIntent.h"
#include "Server-Game/ECS/Components/MovementPath.h"
#include "Server-Game/ECS/Components/MovementRoute.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/MovementUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <MetaGen/Shared/Packet/Packet.h>

#include <Base/CVarSystem/CVarSystem.h>
#include <Base/Util/DebugHandler.h>

#include <Detour/DetourNavMesh.h>

#include <enkiTS/TaskScheduler.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

#include <cmath>
#include <chrono>
#include <iterator>
#include <vector>

namespace ECS::Systems
{
    namespace
    {
        const vec3 MOVEMENT_POLY_PICK_EXTENTS = vec3(2.0f, 30.0f, 2.0f);
        constexpr f32 MOVEMENT_CORNER_REACHED_DISTANCE_SQ = 0.05f;
        constexpr f32 MOVEMENT_NETWORK_UPDATE_INTERVAL = 0.1f;
        constexpr f32 MOVEMENT_MAX_SIMULATION_DELTA = 0.1f;
        constexpr f32 MOVEMENT_PATH_RETRY_MIN_DELAY = 0.25f;
        constexpr f32 MOVEMENT_PATH_RETRY_MAX_DELAY = 5.0f;

        AutoCVar_Int CVAR_MovementMaxPathRequestsPerTick(
            CVarCategory::Server,
            "movement.maxPathRequestsPerTick",
            "Maximum number of navigation path requests processed per world tick.",
            1024);

        AutoCVar_Int CVAR_MovementPathQueryBudgetMicroseconds(
            CVarCategory::Server,
            "movement.pathQueryBudgetMicroseconds",
            "Maximum wall-clock time spent processing navigation path queries per world tick.",
            2000);

        AutoCVar_Int CVAR_MovementWorkerCount(
            CVarCategory::Server,
            "movement.workerCount",
            "Maximum concurrent Enki task partitions for a world's active movement calculation. One keeps it single-threaded; this caps shared-scheduler concurrency but does not reserve physical cores.",
            1);

        AutoCVar_Int CVAR_MovementWorkerMinJobs(
            CVarCategory::Server,
            "movement.workerMinJobs",
            "Minimum active movement jobs before dispatching work to EnkiTS.",
            512);

        AutoCVar_Int CVAR_MovementProfileEnabled(
            CVarCategory::Server,
            "movement.profileEnabled",
            "Collect and periodically log movement hot-path timings and counters.",
            0,
            CVarFlags::EditCheckbox);

        AutoCVar_Int CVAR_MovementProfileIntervalSeconds(
            CVarCategory::Server,
            "movement.profileIntervalSeconds",
            "Simulation-time interval between movement profile reports.",
            1);

        using MovementClock = std::chrono::steady_clock;

        struct MovementFrameProfile
        {
        public:
            u64 totalMicroseconds = 0;
            u64 pathDispatchMicroseconds = 0;
            u64 findPathMicroseconds = 0;
            u64 activeMovementMicroseconds = 0;
            u64 executeMovementMicroseconds = 0;
            u64 resultMovementMicroseconds = 0;
            u32 pathDequeues = 0;
            u32 pathQueries = 0;
            u32 pathSuccesses = 0;
            u32 pathFailures = 0;
            u32 activeMovers = 0;
        };

        u64 GetElapsedMicroseconds(MovementClock::time_point start)
        {
            return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(MovementClock::now() - start).count());
        }

        struct MovementDestination
        {
        public:
            vec3 position = vec3(0.0f);
            f32 repathInterval = 0.0f;
            f32 repathDistance = 0.0f;
            bool repathOnTimer = false;
            bool clearOnReach = false;
        };

        enum class MovementDestinationResult : u8
        {
            None,
            Invalid,
            Success
        };

        f32 NormalizeAngle(f32 angle)
        {
            while (angle > glm::pi<f32>())
                angle -= 2.0f * glm::pi<f32>();

            while (angle < -glm::pi<f32>())
                angle += 2.0f * glm::pi<f32>();

            return angle;
        }

        f32 GetWanderPause(World& world, const Components::MovementWanderIntent& wanderIntent)
        {
            f32 minPause = glm::max(wanderIntent.minPause, 0.0f);
            f32 maxPause = glm::max(wanderIntent.maxPause, minPause);
            return minPause + world.rng.NextF32() * (maxPause - minPause);
        }

        void SelectWanderDestination(World& world, const Components::MovementWanderIntent& wanderIntent, Components::MovementPath& movementPath)
        {
            f32 radius = glm::max(wanderIntent.radius, 0.0f);
            f32 angle = world.rng.NextF32() * 2.0f * glm::pi<f32>();
            f32 distance = std::sqrt(world.rng.NextF32()) * radius;

            movementPath.requestedEndPosition = wanderIntent.center + vec3(std::sin(angle) * distance, 0.0f, std::cos(angle) * distance);
        }

        MovementDestinationResult TryResolveDestination(World& world, const Components::MovementIntent& movementIntent, Components::MovementPath& movementPath, MovementDestination& destination)
        {
            switch (movementIntent.type)
            {
                case Components::MovementIntentType::Follow:
                {
                    if (!world.ValidEntity(movementIntent.follow.target))
                        return MovementDestinationResult::Invalid;

                    auto& targetTransform = world.Get<Components::Transform>(movementIntent.follow.target);
                    destination.position = targetTransform.position;
                    destination.repathInterval = movementIntent.follow.repathInterval;
                    destination.repathDistance = movementIntent.follow.repathDistance;
                    destination.repathOnTimer = true;
                    destination.clearOnReach = false;
                    return MovementDestinationResult::Success;
                }
                case Components::MovementIntentType::Point:
                {
                    destination.position = movementIntent.point.position;
                    destination.repathInterval = 0.0f;
                    destination.repathDistance = movementIntent.point.repathDistance;
                    destination.repathOnTimer = false;
                    destination.clearOnReach = movementIntent.point.clearOnReach;
                    return MovementDestinationResult::Success;
                }
                case Components::MovementIntentType::Wander:
                {
                    destination.position = movementPath.requestedEndPosition;
                    destination.repathInterval = 0.0f;
                    destination.repathDistance = movementIntent.wander.repathDistance;
                    destination.repathOnTimer = false;
                    destination.clearOnReach = false;
                    return MovementDestinationResult::Success;
                }
                case Components::MovementIntentType::None:
                default:
                    return MovementDestinationResult::None;
            }
        }

        bool ShouldRepath(const Components::MovementPath& movementPath, const MovementDestination& destination)
        {
            if (movementPath.numPositions == 0)
                return true;

            if (destination.repathOnTimer && movementPath.repathTimer <= 0.0f)
                return true;

            const f32 repathDistanceSq = destination.repathDistance * destination.repathDistance;
            return glm::distance2(movementPath.requestedEndPosition, destination.position) > repathDistanceSq;
        }

        f32 GetPathRetryDelay(World& world, u32 retryCount)
        {
            u32 exponent = glm::min(retryCount, 5u);
            f32 delay = glm::min(MOVEMENT_PATH_RETRY_MIN_DELAY * static_cast<f32>(1u << exponent), MOVEMENT_PATH_RETRY_MAX_DELAY);
            return delay + world.rng.NextF32() * MOVEMENT_PATH_RETRY_MIN_DELAY;
        }

        void ExecuteMovementStepRange(World& world, std::vector<Util::MovementStepJob>& stepJobs, std::vector<Util::MovementStepResult>& stepResults, bool profileEnabled, enki::TaskSetPartition range)
        {
            for (u32 index = range.start; index < range.end; index++)
            {
                const Util::MovementStepJob& job = stepJobs[index];
                Util::MovementStepResult& result = stepResults[index];

                vec3 toCorner = job.cornerTarget - job.previousPosition;
                f32 distance = std::sqrt(glm::dot(toCorner, toCorner));
                vec3 direction = distance > 0.0f ? toCorner / distance : vec3(0.0f);
                f32 moveDistance = glm::min(job.speed * job.simulationDelta, distance);
                vec3 finalPosition = job.previousPosition + direction * moveDistance;

                MovementClock::time_point heightSampleStart;
                if (profileEnabled)
                    heightSampleStart = MovementClock::now();

                f32 terrainHeight = 0.0f;
                if (world.navmeshData.GetTerrainHeight(finalPosition, terrainHeight))
                    finalPosition.y = terrainHeight;

                //if (profileEnabled)
                //    result.heightSampleMicroseconds = GetElapsedMicroseconds(heightSampleStart);

                result.guid = ObjectGUID::Empty;
                result.pitchYaw = job.previousPitchYaw;
                result.entity = entt::null;

                vec3 moveDirection = finalPosition - job.previousPosition;
                bool yawChanged = glm::length2(moveDirection) > 0.0f;
                if (yawChanged)
                {
                    moveDirection = glm::normalize(moveDirection);
                    result.pitchYaw.y = NormalizeAngle(std::atan2(moveDirection.x, moveDirection.z) + glm::pi<f32>());
                }

                bool moved = glm::length2(finalPosition - job.previousPosition) > 0.0f;
                if (moved)
                {
                    result.position = finalPosition;

                    auto& transform = world.Get<Components::Transform>(job.entity);
                    transform.position = result.position;
                    transform.pitchYaw = result.pitchYaw;
                }

                if (moved || yawChanged)
                {
                    result.guid = job.guid;
                    result.entity = job.entity;
                }
            }
        }

        void ExecuteMovementSteps(World& world, bool profileEnabled)
        {
            auto& movementScheduler = world.movementScheduler;
            auto& stepJobs = movementScheduler.stepJobs;
            auto& stepResults = movementScheduler.stepResults;
            stepResults.resize(stepJobs.size());
            if (stepJobs.empty())
                return;

            u32 configuredWorkers = static_cast<u32>(glm::max(CVAR_MovementWorkerCount.Get(), 1));
            u32 minimumJobs = static_cast<u32>(glm::max(CVAR_MovementWorkerMinJobs.Get(), 1));
            enki::TaskScheduler* taskScheduler = ServiceLocator::GetTaskScheduler();
            u32 workerCount = glm::min(configuredWorkers, taskScheduler->GetNumTaskThreads());
            if (workerCount <= 1 || stepJobs.size() < minimumJobs)
            {
                ExecuteMovementStepRange(world, stepJobs, stepResults, profileEnabled, enki::TaskSetPartition{ 0, static_cast<u32>(stepJobs.size()) });
                return;
            }

            enki::TaskSet taskSet(static_cast<u32>(stepJobs.size()), [&](enki::TaskSetPartition range, u32)
            {
                ExecuteMovementStepRange(world, stepJobs, stepResults, profileEnabled, range);
            });
            taskSet.m_MinRange = (static_cast<u32>(stepJobs.size()) + workerCount - 1) / workerCount;
            taskScheduler->AddTaskSetToPipe(&taskSet);
            taskScheduler->WaitforTask(&taskSet);
        }

        void ProcessPathRequests(World& world, Singletons::TimeState& timeState, MovementFrameProfile* profile)
        {
            Util::Movement::AdvancePathRequests(world, timeState.deltaTime);

            i32 configuredMaxRequests = CVAR_MovementMaxPathRequestsPerTick.Get();
            u32 maxRequests = configuredMaxRequests > 0 ? glm::min(static_cast<u32>(configuredMaxRequests), 1024u) : 0;
            i32 configuredBudgetMicroseconds = CVAR_MovementPathQueryBudgetMicroseconds.Get();
            if (maxRequests == 0 || configuredBudgetMicroseconds <= 0)
                return;

            auto navmeshQuery = world.navmeshData.CreateQueryContext();
            u32 maxDequeues = maxRequests * 4;
            auto pathQueryDeadline = std::chrono::steady_clock::now() + std::chrono::microseconds(configuredBudgetMicroseconds);
            u32 processed = 0;
            for (u32 dequeued = 0; dequeued < maxDequeues && processed < maxRequests; dequeued++)
            {
                if (std::chrono::steady_clock::now() >= pathQueryDeadline)
                    break;

                entt::entity entity = entt::null;
                u32 intentRevision = 0;
                u32 retryCount = 0;
                if (!Util::Movement::PopDuePathRequest(world, entity, intentRevision, retryCount))
                    break;

                if (profile)
                    profile->pathDequeues++;

                if (!world.ValidEntity(entity) ||
                    !world.AllOf<Components::Transform, Components::MovementIntent, Components::MovementPath>(entity))
                    continue;

                auto& transform = world.Get<Components::Transform>(entity);
                auto& movementIntent = world.Get<Components::MovementIntent>(entity);
                auto& movementPath = world.Get<Components::MovementPath>(entity);
                if (movementIntent.revision != intentRevision)
                    continue;

                if (movementIntent.type == Components::MovementIntentType::Wander)
                    SelectWanderDestination(world, movementIntent.wander, movementPath);

                MovementDestination destination;
                MovementDestinationResult destinationResult = TryResolveDestination(world, movementIntent, movementPath, destination);
                if (destinationResult == MovementDestinationResult::None)
                {
                    movementPath.status = Components::MovementPathStatus::None;
                    world.Remove<Components::MovementRoute>(entity);
                    world.Remove<Tags::IsMovementActive>(entity);
                    continue;
                }

                if (destinationResult == MovementDestinationResult::Invalid)
                {
                    movementPath.status = Components::MovementPathStatus::Failed;
                    movementPath.failure = Components::MovementPathFailure::InvalidIntent;
                    world.Remove<Components::MovementRoute>(entity);
                    world.Remove<Tags::IsMovementActive>(entity);
                    continue;
                }

                processed++;
                if (profile)
                    profile->pathQueries++;

                auto& movementRoute = world.GetOrEmplace<Components::MovementRoute>(entity);
                u32 pathCount = 0;
                MovementClock::time_point findPathStart;
                if (profile)
                    findPathStart = MovementClock::now();

                bool foundPath = world.navmeshData.FindPath(navmeshQuery, transform.position, destination.position, MOVEMENT_POLY_PICK_EXTENTS, movementRoute.positions, static_cast<u32>(std::size(movementRoute.positions)), pathCount, &movementRoute.polyRefStart, &movementRoute.polyRefEnd);
                if (profile)
                    profile->findPathMicroseconds += GetElapsedMicroseconds(findPathStart);

                if (foundPath)
                {
                    if (profile)
                        profile->pathSuccesses++;

                    movementPath.polyRefCurrent = movementRoute.polyRefStart;
                    movementPath.numPositions = pathCount;
                    movementPath.currentPositionIndex = 0;
                    movementPath.requestedEndPosition = destination.position;
                    movementPath.repathTimer = destination.repathInterval;
                    movementPath.status = Components::MovementPathStatus::Moving;
                    movementPath.failure = Components::MovementPathFailure::None;
                    world.GetOrEmplace<Tags::IsMovementActive>(entity);
                }
                else
                {
                    if (profile)
                        profile->pathFailures++;

                    world.Remove<Components::MovementRoute>(entity);
                    movementPath.status = Components::MovementPathStatus::Failed;
                    movementPath.failure = Components::MovementPathFailure::PathNotFound;
                    Util::Movement::QueuePathRequest(world, entity, GetPathRetryDelay(world, retryCount + 1), retryCount + 1);
                }
            }
        }
    }

    void Movement::HandleUpdate(World& world, Singletons::TimeState& timeState, Singletons::NetworkState& networkState)
    {
        const bool profileEnabled = CVAR_MovementProfileEnabled.Get() != 0;
        MovementFrameProfile frameProfile;
        MovementClock::time_point totalStart;
        if (profileEnabled)
            totalStart = MovementClock::now();

        MovementClock::time_point pathDispatchStart;
        if (profileEnabled)
            pathDispatchStart = MovementClock::now();

        ProcessPathRequests(world, timeState, profileEnabled ? &frameProfile : nullptr);
        if (profileEnabled)
            frameProfile.pathDispatchMicroseconds = GetElapsedMicroseconds(pathDispatchStart);

        MovementClock::time_point activeMovementStart;
        if (profileEnabled)
            activeMovementStart = MovementClock::now();

        world.movementScheduler.stepJobs.clear();
        auto movementView = world.View<Components::ObjectInfo, Components::Transform, Components::VisibilityInfo, Components::MovementIntent, Components::MovementPath, Components::MovementRoute, Tags::IsMovementActive>();
        movementView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::MovementIntent& movementIntent, Components::MovementPath& movementPath, Components::MovementRoute& movementRoute)
        {
            if (profileEnabled)
                frameProfile.activeMovers++;

            if (movementPath.intentRevision != movementIntent.revision)
            {
                Util::Movement::QueuePathRequest(world, entity, 0.0f);
                return;
            }

            movementPath.repathTimer = glm::max(movementPath.repathTimer - timeState.deltaTime, 0.0f);
            movementPath.networkUpdateTimer = glm::max(movementPath.networkUpdateTimer - timeState.deltaTime, 0.0f);

            MovementDestination destination;
            MovementDestinationResult destinationResult = TryResolveDestination(world, movementIntent, movementPath, destination);
            if (destinationResult == MovementDestinationResult::None)
            {
                world.Remove<Components::MovementRoute>(entity);
                world.Remove<Tags::IsMovementActive>(entity);
                return;
            }

            if (destinationResult == MovementDestinationResult::Invalid)
            {
                movementPath.status = Components::MovementPathStatus::Failed;
                movementPath.failure = Components::MovementPathFailure::InvalidIntent;
                world.Remove<Components::MovementRoute>(entity);
                world.Remove<Tags::IsMovementActive>(entity);
                return;
            }

            if (ShouldRepath(movementPath, destination))
            {
                Util::Movement::QueuePathRequest(world, entity, 0.0f);
                return;
            }

            if (movementPath.numPositions == 0)
                return;

            if (movementPath.polyRefCurrent == 0)
            {
                movementPath.status = Components::MovementPathStatus::Failed;
                movementPath.failure = Components::MovementPathFailure::InvalidSurface;
                Util::Movement::QueuePathRequest(world, entity, MOVEMENT_PATH_RETRY_MIN_DELAY);
                return;
            }

            vec3 cornerTarget = movementRoute.positions[movementPath.currentPositionIndex];

            vec3 toCorner = cornerTarget - transform.position;
            f32 distSq = glm::dot(toCorner, toCorner);
            if (distSq < MOVEMENT_CORNER_REACHED_DISTANCE_SQ)
            {
                if (movementPath.currentPositionIndex + 1 < movementPath.numPositions)
                {
                    movementPath.currentPositionIndex++;

                    cornerTarget = movementRoute.positions[movementPath.currentPositionIndex];
                    toCorner = cornerTarget - transform.position;
                    distSq = glm::dot(toCorner, toCorner);
                }
                else
                {
                    movementPath.status = Components::MovementPathStatus::Reached;
                    movementPath.failure = Components::MovementPathFailure::None;

                    if (destination.clearOnReach)
                    {
                        movementIntent.type = Components::MovementIntentType::None;
                        movementIntent.revision++;
                        movementPath.intentRevision = movementIntent.revision;
                        world.Remove<Components::MovementRoute>(entity);
                        world.Remove<Tags::IsMovementActive>(entity);
                    }
                    else if (movementIntent.type == Components::MovementIntentType::Wander)
                    {
                        Util::Movement::QueuePathRequest(world, entity, GetWanderPause(world, movementIntent.wander));
                    }
                    else if (movementIntent.type == Components::MovementIntentType::Follow)
                    {
                        Util::Movement::QueuePathRequest(world, entity, glm::max(destination.repathInterval, MOVEMENT_PATH_RETRY_MIN_DELAY));
                    }
                    else
                    {
                        world.Remove<Components::MovementRoute>(entity);
                        world.Remove<Tags::IsMovementActive>(entity);
                    }

                    return;
                }
            }

            f32 speed = glm::max(movementIntent.speed, 0.0f);
            if (speed <= 0.0f)
                return;

            f32 simulationDelta = glm::min(glm::max(timeState.deltaTime, 0.0f), MOVEMENT_MAX_SIMULATION_DELTA);
            world.movementScheduler.stepJobs.push_back(Util::MovementStepJob{
                .entity = entity,
                .guid = objectInfo.guid,
                .previousPosition = transform.position,
                .cornerTarget = cornerTarget,
                .previousPitchYaw = transform.pitchYaw,
                .speed = speed,
                .simulationDelta = simulationDelta
            });
        });

        MovementClock::time_point executeMovementStart;
        if (profileEnabled)
            executeMovementStart = MovementClock::now();

        ExecuteMovementSteps(world, profileEnabled);

        if (profileEnabled)
            frameProfile.executeMovementMicroseconds = GetElapsedMicroseconds(executeMovementStart);

        MovementClock::time_point resultMovementStart;
        if (profileEnabled)
            resultMovementStart = MovementClock::now();

        auto& stepJobs = world.movementScheduler.stepJobs;
        auto& stepResults = world.movementScheduler.stepResults;
        for (u32 index = 0; index < stepJobs.size(); index++)
        {
            const Util::MovementStepResult& result = stepResults[index];
            if (!world.ValidEntity(result.entity) || result.guid.GetType() != ObjectGUID::Type::Creature)
                continue;

            auto& visibilityInfo = world.Get<Components::VisibilityInfo>(result.entity);
            auto& movementPath = world.Get<Components::MovementPath>(result.entity);
            
            if (movementPath.networkUpdateTimer <= 0.0f && !visibilityInfo.visiblePlayers.empty())
            {
                const Components::MovementFlags movementFlags{
                    .forward = 1,
                    .grounded = 1
                };
            
                ECS::Util::Network::SendToNearby(networkState, world, result.entity, visibilityInfo, false, ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitMovePacket::PACKET_ID, result.guid), MetaGen::Shared::Packet::ServerUnitMovePacket{
                    .guid = result.guid,
                    .movementFlags = movementFlags.ToBitMask(),
                    .position = result.position,
                    .pitchYaw = result.pitchYaw,
                    .verticalVelocity = 0.0f
                });
                movementPath.networkUpdateTimer = MOVEMENT_NETWORK_UPDATE_INTERVAL;
            }

            world.creatureVisData.Update(result.guid, result.position.x, result.position.z);
        }

        if (profileEnabled)
        {
            frameProfile.resultMovementMicroseconds = GetElapsedMicroseconds(resultMovementStart);
            frameProfile.activeMovementMicroseconds = GetElapsedMicroseconds(activeMovementStart);
            frameProfile.totalMicroseconds = GetElapsedMicroseconds(totalStart);

            auto& stats = world.movementScheduler.profileStats;
            stats.elapsedSeconds += timeState.deltaTime > 0.0f ? static_cast<f64>(timeState.deltaTime) : 0.0;
            stats.tickCount++;
            stats.totalMicroseconds += frameProfile.totalMicroseconds;
            stats.pathDispatchMicroseconds += frameProfile.pathDispatchMicroseconds;
            stats.findPathMicroseconds += frameProfile.findPathMicroseconds;
            stats.activeMovementMicroseconds += frameProfile.activeMovementMicroseconds;
            stats.executeMovementMicroseconds += frameProfile.executeMovementMicroseconds;
            stats.resultMovementMicroseconds += frameProfile.resultMovementMicroseconds;
            stats.pathDequeues += frameProfile.pathDequeues;
            stats.pathQueries += frameProfile.pathQueries;
            stats.pathSuccesses += frameProfile.pathSuccesses;
            stats.pathFailures += frameProfile.pathFailures;
            stats.activeMovers += frameProfile.activeMovers;

            const f64 reportInterval = static_cast<f64>(glm::max(CVAR_MovementProfileIntervalSeconds.Get(), 1));
            if (stats.elapsedSeconds >= reportInterval)
            {
                const f64 ticks = static_cast<f64>(stats.tickCount);
                const f64 pathQueries = static_cast<f64>(stats.pathQueries);
                const f64 activeMovers = static_cast<f64>(stats.activeMovers);
                const f64 stepJobs = static_cast<f64>(stats.activeMovers);
                const f64 stepJobsActedUpon = static_cast<f64>(stats.activeMovers);
                NC_LOG_INFO(
                    "Movement profile [{} ticks]: total {:.2f} ms/tick, dispatch {:.2f}, findPath {:.2f} ({} queries, {:.3f} ms/query), active {:.2f}, execute {:.2f}, result {:.2f}, active movers {:.1f}/tick, queue {}",
                    stats.tickCount,
                    static_cast<f64>(stats.totalMicroseconds) / ticks / 1000.0,
                    static_cast<f64>(stats.pathDispatchMicroseconds) / ticks / 1000.0,
                    static_cast<f64>(stats.findPathMicroseconds) / ticks / 1000.0,
                    stats.pathQueries,
                    pathQueries > 0.0 ? static_cast<f64>(stats.findPathMicroseconds) / pathQueries / 1000.0 : 0.0,
                    static_cast<f64>(stats.activeMovementMicroseconds) / ticks / 1000.0,
                    static_cast<f64>(stats.executeMovementMicroseconds) / ticks / 1000.0,
                    static_cast<f64>(stats.resultMovementMicroseconds) / ticks / 1000.0,
                    activeMovers / ticks,
                    world.movementScheduler.pathRequestHeap.size());
                stats.Reset();
            }
        }
        else if (world.movementScheduler.profileStats.tickCount != 0)
        {
            world.movementScheduler.profileStats.Reset();
        }
    }
}
