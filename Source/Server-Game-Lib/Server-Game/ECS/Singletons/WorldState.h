#pragma once
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/PermissionInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldNavmeshData.h"
#include "Server-Game/ECS/Singletons/WorldSpatialIndex.h"
#include "Server-Game/ECS/Util/MovementScheduler.h"

#include <Base/Container/ConcurrentQueue.h>
#include <Base/Math/Math.h>
#include <Base/Memory/PacketArena.h>
#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Network/Define.h>

#include <Scripting/Zenith.h>

#include <memory>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>

namespace ECS
{
    struct World;

    namespace Util::Movement
    {
        void CancelPathRequest(World& world, entt::entity entity);
    }
}

namespace ECS
{
    struct WorldVisData
    {
    public:
        void Add(ObjectGUID guid, f32 x, f32 z)
        {
            _spatialIndex.Add(guid, vec2(x, z));
        }
        void Update(ObjectGUID guid, f32 x, f32 z)
        {
            _spatialIndex.Update(guid, vec2(x, z));
        }
        i32 GetInRect(const vec4& rect, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return _spatialIndex.GetInRect(rect, callback);
        }
        i32 GetInRadius(vec2 center, f32 radius, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return _spatialIndex.GetInRadius(center, radius, callback);
        }

        const robin_hood::unordered_map<ObjectGUID, entt::entity>& GetGUIDToEntityMap() const
        {
            return _guidToEntity;
        }
        const entt::entity GetEntity(ObjectGUID guid)
        {
            if (!HasEntity(guid))
                return entt::null;

            return _guidToEntity[guid];
        }
        bool HasEntity(ObjectGUID guid)
        {
            return _guidToEntity.contains(guid);
        }
        void AddEntity(ObjectGUID guid, entt::entity entity, vec2 position)
        {
            _guidToEntity[guid] = entity;
            Add(guid, position.x, position.y);
        }
        void RemoveEntity(ObjectGUID guid)
        {
            _guidToEntity.erase(guid);
            _spatialIndex.Remove(guid);
        }

    private:
        WorldSpatialIndex _spatialIndex;
        robin_hood::unordered_map<ObjectGUID, entt::entity> _guidToEntity;
    };
    struct World
    {
    public:
        World(std::shared_ptr<PacketArenaBudget> packetArenaBudget, const Singletons::PacketArenaConfig& packetArenaConfig)
            : registry(new entt::registry()), mapID(0), playerVisData(), creatureVisData(), packetArena(std::move(packetArenaBudget), packetArenaConfig.maxReservedBytes, packetArenaConfig.blockSize), packetArenaTrimTimer(packetArenaConfig.trimIntervalSeconds)
        {
            _typeToVisData =
            {
                { ObjectGUID::Type::Player, &playerVisData },
                { ObjectGUID::Type::Creature, &creatureVisData }
            };
        }

        entt::entity GetEntity(ObjectGUID guid)
        {
            auto it = _typeToVisData.find(guid.GetType());
            if (it == _typeToVisData.end() || !guid.IsValid())
                return entt::null;

            WorldVisData& visData = *it->second;
            return visData.GetEntity(guid);
        }
        bool HasEntity(ObjectGUID guid)
        {
            auto it = _typeToVisData.find(guid.GetType());
            if (it == _typeToVisData.end() || !guid.IsValid())
                return false;

            WorldVisData& visData = *it->second;
            return visData.HasEntity(guid);
        }
        void AddEntity(ObjectGUID guid, entt::entity entity, vec2 position)
        {
            auto it = _typeToVisData.find(guid.GetType());
            if (it == _typeToVisData.end() || !guid.IsValid())
                return;

            WorldVisData& visData = *it->second;
            visData.AddEntity(guid, entity, position);
        }
        void RemoveEntity(ObjectGUID guid)
        {
            auto it = _typeToVisData.find(guid.GetType());
            if (it == _typeToVisData.end() || !guid.IsValid())
                return;

            WorldVisData& visData = *it->second;
            visData.RemoveEntity(guid);
        }

        i32 GetPlayersInRect(const vec4& rect, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return playerVisData.GetInRect(rect, std::move(callback));
        }
        i32 GetCreaturesInRect(const vec4& rect, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return creatureVisData.GetInRect(rect, std::move(callback));
        }

        i32 GetPlayersInRadius(vec2 center, f32 radius, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return playerVisData.GetInRadius(center, radius, std::move(callback));
        }
        i32 GetCreaturesInRadius(vec2 center, f32 radius, std::function<bool(const ObjectGUID)>&& callback) const
        {
            return creatureVisData.GetInRadius(center, radius, std::move(callback));
        }

        [[nodiscard]] bool ValidEntity(entt::entity entity) { return registry->valid(entity); }
        [[nodiscard]] entt::entity CreateEntity() { return registry->create(); }
        void DestroyEntity(entt::entity entity)
        {
            if (!ValidEntity(entity))
                return;

            Util::Movement::CancelPathRequest(*this, entity);
            registry->destroy(entity);
        }

        template<typename... Types, typename... Exclude>
        [[nodiscard]] decltype(auto) View(entt::exclude_t<Exclude...> excludes = entt::exclude_t{}) { return registry->view<Types...>(excludes); }

        template<typename... Type>
        [[nodiscard]] decltype(auto) Get(const entt::entity entt) { return registry->get<Type...>(entt); }

        template<typename Type, typename... Args>
        [[nodiscard]] decltype(auto) GetOrEmplace(const entt::entity entt, Args&&... args) { return registry->get_or_emplace<Type>(entt, std::forward<Args>(args)...); }

        template<typename... Type>
        [[nodiscard]] decltype(auto) TryGet(const entt::entity entt) { return registry->try_get<Type...>(entt); }

        template<typename Type, typename... Args>
        decltype(auto) Emplace(const entt::entity entt, Args&&... args) { return registry->emplace<Type>(entt, std::forward<Args>(args)...); }

        template<typename Type, typename... Args>
        decltype(auto) EmplaceOrReplace(const entt::entity entt, Args&&... args) { return registry->emplace_or_replace<Type>(entt, std::forward<Args>(args)...); }
        
        template<typename... Type>
        [[nodiscard]] bool AllOf(const entt::entity entt) const { return registry->all_of<Type...>(entt); }

        template<typename... Type>
        [[nodiscard]] bool AnyOf(const entt::entity entt) const { return registry->any_of<Type...>(entt); }

        template<typename Type, typename... Other>
        decltype(auto) Remove(const entt::entity entt) { return registry->remove<Type, Other...>(entt); }

        template<typename Type, typename... Other>
        void Erase(const entt::entity entt)
        {
            registry->erase<Type, Other...>(entt);
        }

        // erase_if wrapper
        template<typename Func>
        void EraseIf(const entt::entity entt, Func func)
        {
            registry->erase_if(entt, std::move(func));
        }

        template<typename... Type>
        void Clear() { registry->clear<Type...>(); }

        template<typename Type, typename... Args>
        Type& EmplaceSingleton(Args&&... args) { return registry->ctx().emplace<Type>(std::forward<Args>(args)...); }

        template<typename Type>
        bool EraseSingleton(const entt::id_type id = entt::type_id<Type>().hash()) { return registry->ctx().erase<Type>(id); }

        template<typename Type>
        [[nodiscard]] Type& GetSingleton(const entt::id_type id = entt::type_id<Type>().hash()) { return registry->ctx().get<Type>(id); }

        // find wrapper
        template<typename Type>
        [[nodiscard]] Type* FindSingleton(const entt::id_type id = entt::type_id<Type>().hash()) { return registry->ctx().find<Type>(id); }

        // contains wrapper
        template<typename Type>
        [[nodiscard]] bool ContainsSingleton(const entt::id_type id = entt::type_id<Type>().hash()) const { return registry->ctx().contains<Type>(id); }

    public:
        static constexpr f32 DEFAULT_VISIBILITY_DISTANCE = 150.0f;

        entt::registry* registry;

        u32 mapID;
        Scripting::ZenithInfoKey zenithKey;
        Math::Xoroshiro128PP rng;
        WorldVisData playerVisData;
        WorldVisData creatureVisData;
        PacketArena packetArena;
        f32 packetArenaTrimTimer = 0.0f;
        Util::MovementScheduler movementScheduler;
        WorldNavmeshData navmeshData;

    private:
        robin_hood::unordered_map<ObjectGUID::Type, WorldVisData*> _typeToVisData;
    };

    struct WorldTransferRequest
    {
    public:
        Network::SocketID socketID;

        std::string characterName;
        u64 characterID;
        Components::PermissionSet accountPermissions;

        u32 targetMapID;
        vec3 targetPosition;
        f32 targetOrientation;
        bool useTargetPosition = false;
    };

    namespace Singletons
    {
        struct WorldState
        {
        public:
            WorldState(std::shared_ptr<PacketArenaBudget> packetArenaBudget, PacketArenaConfig packetArenaConfig)
                : _packetArenaBudget(std::move(packetArenaBudget)), _packetArenaConfig(std::move(packetArenaConfig)), _socketIDToMapID(1024), _worldTransferRequests(1024)
            {
                _mapIDToWorld.reserve(8192);
            }

            robin_hood::unordered_map<u32, World>::iterator begin()
            {
                return _mapIDToWorld.begin();
            }
            robin_hood::unordered_map<u32, World>::iterator end()
            {
                return _mapIDToWorld.end();
            }

            bool HasWorld(u32 mapID)
            {
                return _mapIDToWorld.contains(mapID);
            }
            World& GetWorld(u32 mapID)
            {
                return _mapIDToWorld.at(mapID);
            }
            bool GetWorld(u32 mapID, World*& world)
            {
                if (!HasWorld(mapID))
                    return false;

                world = &_mapIDToWorld.at(mapID);
                return true;
            }
            bool AddWorld(u32 mapID)
            {
                if (HasWorld(mapID))
                    return false;

                auto insertResult = _mapIDToWorld.try_emplace(mapID, _packetArenaBudget, _packetArenaConfig);
                World& world = insertResult.first->second;
                world.mapID = mapID;

                world.EmplaceSingleton<Events::MapNeedsInitialization>();

                return true;
            }
            bool RemoveWorld(u32 mapID)
            {
                if (!HasWorld(mapID))
                    return false;

                _mapIDToWorld.erase(mapID);
                return true;
            }

            bool GetMapIDFromSocket(Network::SocketID socketID, u32& mapID)
            {
                if (!_socketIDToMapID.contains(socketID))
                    return false;

                mapID = _socketIDToMapID[socketID];
                return true;
            }
            bool GetWorldFromSocket(Network::SocketID socketID, World*& world)
            {
                u32 mapID;
                if (!GetMapIDFromSocket(socketID, mapID))
                    return false;

                return GetWorld(mapID, world);
            }
            void SetMapIDForSocket(Network::SocketID socketID, u32 mapID)
            {
                _socketIDToMapID[socketID] = mapID;
            }
            void ClearMapIDForSocket(Network::SocketID socketID)
            {
                _socketIDToMapID.erase(socketID);
            }

            void AddWorldTransferRequest(const WorldTransferRequest&& request)
            {
                _worldTransferRequests.enqueue(request);
            }

            moodycamel::ConcurrentQueue<WorldTransferRequest>& GetWorldTransferRequests()
            {
                return _worldTransferRequests;
            }

        private:
            std::shared_ptr<PacketArenaBudget> _packetArenaBudget;
            PacketArenaConfig _packetArenaConfig;
            robin_hood::unordered_map<u32, World> _mapIDToWorld;
            robin_hood::unordered_map<Network::SocketID, u32> _socketIDToMapID;

            moodycamel::ConcurrentQueue<WorldTransferRequest> _worldTransferRequests;
        };
    }
}
