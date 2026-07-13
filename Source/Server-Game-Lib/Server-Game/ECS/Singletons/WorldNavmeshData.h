#pragma once
#include <Base/Types.h>

#include <FileFormat/Novus/NavMesh/NavMesh.h>

#include <Detour/DetourNavMesh.h>

#include <memory>
#include <shared_mutex>

static_assert(sizeof(dtPolyRef) == sizeof(u64));
static_assert(sizeof(dtTileRef) == sizeof(u64));
static_assert(NavMesh::USE_64BIT_POLY_REFS);

namespace ECS
{
    class WorldNavmeshData
    {
    private:
        struct State;

    public:
        class QueryContext
        {
        public:
            QueryContext(QueryContext&& other) noexcept;
            QueryContext& operator=(QueryContext&& other) noexcept;
            ~QueryContext();

            QueryContext(const QueryContext&) = delete;
            QueryContext& operator=(const QueryContext&) = delete;

            explicit operator bool() const { return _query != nullptr; }

        private:
            friend class WorldNavmeshData;

            explicit QueryContext(const State* state);
            void Release();

            const State* _state = nullptr;
            std::shared_lock<std::shared_mutex> _lock;
            dtNavMeshQuery* _query = nullptr;
        };

        WorldNavmeshData();
        ~WorldNavmeshData();

        WorldNavmeshData(WorldNavmeshData&& other) noexcept;
        WorldNavmeshData& operator=(WorldNavmeshData&& other) noexcept;

        WorldNavmeshData(const WorldNavmeshData&) = delete;
        WorldNavmeshData& operator=(const WorldNavmeshData&) = delete;

        bool Initialize(u32 maxTiles, u32 maxPolys);
        void Cleanup();

        bool AddTile(u32 chunkX, u32 chunkY, const u8* navData, u32 navDataSize);
        bool AddHeightTile(u32 chunkX, u32 chunkY, const u8* heightData, u32 heightDataSize);
        bool RemoveTile(u32 chunkX, u32 chunkY);
        bool HasTile(u32 chunkX, u32 chunkY) const;
        bool HasHeightTile(u32 chunkX, u32 chunkY) const;
        u32 GetNumTiles() const;
        u32 GetNumHeightTiles() const;

        QueryContext CreateQueryContext() const;

        bool FindPath(const vec3& startPosition, const vec3& endPosition, const vec3& searchExtents, vec3* pathPositions, u32 pathCapacity, u32& pathCount, dtPolyRef* startRef = nullptr, dtPolyRef* endRef = nullptr) const;
        bool FindPath(QueryContext& queryContext, const vec3& startPosition, const vec3& endPosition, const vec3& searchExtents, vec3* pathPositions, u32 pathCapacity, u32& pathCount, dtPolyRef* startRef = nullptr, dtPolyRef* endRef = nullptr) const;
        bool MoveAlongSurface(dtPolyRef startRef, const vec3& startPosition, const vec3& endPosition, vec3& resultPosition, dtPolyRef& resultRef) const;
        bool GetTerrainHeight(const vec3& position, f32& height) const;

    private:
        std::unique_ptr<State> _state;
    };
}
