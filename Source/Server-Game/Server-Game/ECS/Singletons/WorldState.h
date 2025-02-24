#pragma once
#include <Base/Container/ConcurrentQueue.h>

#include <Gameplay/GameDefine.h>

#include <Network/Define.h>

#include <memory>

#include <entt/entt.hpp>
#include <robinhood/robinhood.h>
#include <RTree/RTree.h>

namespace ECS
{
    struct World
    {
    public:
        void AddPlayer(GameDefine::ObjectGuid guid, f32 x, f32 z)
        {
            vec2 minPos = vec2(x, z) - 5.0f; // DEFAULT_VISIBILITY_DISTANCE;
            vec2 maxPos = vec2(x, z) + 5.0f; // DEFAULT_VISIBILITY_DISTANCE;
            visTreePlayers->Insert(reinterpret_cast<f32*>(&minPos), reinterpret_cast<f32*>(&maxPos), guid);
        }
        void RemovePlayer(GameDefine::ObjectGuid guid)
        {
            visTreePlayers->Remove(guid);
        }
        void UpdatePlayer(GameDefine::ObjectGuid guid, f32 x, f32 z)
        {
            RemovePlayer(guid);
            AddPlayer(guid, x, z);
        }
        i32 GetPlayersInRect(vec4 rect, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            return visTreePlayers->Search(reinterpret_cast<f32*>(&rect.x), reinterpret_cast<f32*>(&rect.z), callback);
        }
        i32 GetPlayersInRadius(f32 x, f32 z, f32 radius, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            vec2 center = vec2(x, z);
            return visTreePlayers->SearchRadius(reinterpret_cast<f32*>(&center), radius, callback);
        }

        void AddUnit(GameDefine::ObjectGuid guid, f32 x, f32 z)
        {
            vec2 minPos = vec2(x, z) - 5.0f; // DEFAULT_VISIBILITY_DISTANCE;
            vec2 maxPos = vec2(x, z) + 5.0f; // DEFAULT_VISIBILITY_DISTANCE;
            visTreeUnits->Insert(reinterpret_cast<f32*>(&minPos), reinterpret_cast<f32*>(&maxPos), guid);
        }
        void RemoveUnit(GameDefine::ObjectGuid guid)
        {
            visTreeUnits->Remove(guid);
        }
        void UpdateUnit(GameDefine::ObjectGuid guid, f32 x, f32 z)
        {
            RemoveUnit(guid);
            AddUnit(guid, x, z);
        }
        i32 GetUnitsInRect(vec4 rect, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            return visTreeUnits->Search(reinterpret_cast<f32*>(&rect.x), reinterpret_cast<f32*>(&rect.z), callback);
        }
        i32 GetUnitsInRadius(f32 x, f32 z, f32 radius, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            vec2 center = vec2(x, z);
            return visTreeUnits->SearchRadius(reinterpret_cast<f32*>(&center), radius, callback);
        }

        i32 GetEntitiesInRect(vec4 rect, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            i32 numPlayersInRect = visTreePlayers->Search(reinterpret_cast<f32*>(&rect.x), reinterpret_cast<f32*>(&rect.z), callback);
            i32 numUnitsInRect = visTreeUnits->Search(reinterpret_cast<f32*>(&rect.x), reinterpret_cast<f32*>(&rect.z), callback);

            i32 numEntitiesInRect = numPlayersInRect + numUnitsInRect;
            return numEntitiesInRect;
        }
        i32 GetEntitiesInRadius(f32 x, f32 z, f32 radius, std::function<bool(const GameDefine::ObjectGuid)>&& callback) const
        {
            vec2 center = vec2(x, z);

            i32 numPlayersInRect = visTreePlayers->SearchRadius(reinterpret_cast<f32*>(&center), radius, callback);
            i32 numUnitsInRect = visTreeUnits->SearchRadius(reinterpret_cast<f32*>(&center), radius, callback);

            i32 numEntitiesInRect = numPlayersInRect + numUnitsInRect;
            return numEntitiesInRect;
        }

        entt::entity GetEntity(GameDefine::ObjectGuid guid) const
        {
            if (!guidToEntity.contains(guid))
                return entt::null;

            return guidToEntity.at(guid);
        }

    public:
        static constexpr f32 DEFAULT_VISIBILITY_DISTANCE = 80.0f;

        u32 mapID;
        RTree<GameDefine::ObjectGuid, f32, 2>* visTreePlayers;
        RTree<GameDefine::ObjectGuid, f32, 2>* visTreeUnits;
        robin_hood::unordered_map<GameDefine::ObjectGuid, entt::entity> guidToEntity;
    };

    namespace Singletons
    {
        struct WorldState
        {
        public:
            bool HasWorld(u32 mapID)
            {
                return mapIDToWorld.contains(mapID);
            }
            World& GetWorld(u32 mapID)
            {
                return mapIDToWorld.at(mapID);
            }
            bool GetWorld(u32 mapID, World*& world)
            {
                if (!HasWorld(mapID))
                    return false;

                world = &mapIDToWorld[mapID];
                return true;
            }
            bool AddWorld(u32 mapID)
            {
                if (HasWorld(mapID))
                    return false;

                World world =
                {
                    .mapID = mapID,
                    .visTreePlayers = new RTree<GameDefine::ObjectGuid, f32, 2>(),
                    .visTreeUnits = new RTree<GameDefine::ObjectGuid, f32, 2>()
                };

                mapIDToWorld[mapID] = std::move(world);
                return true;
            }
            bool RemoveWorld(u32 mapID)
            {
                if (!HasWorld(mapID))
                    return false;

                mapIDToWorld.erase(mapID);
                return true;
            }

        public:
            robin_hood::unordered_map<u32, World> mapIDToWorld;
        };
    }
}