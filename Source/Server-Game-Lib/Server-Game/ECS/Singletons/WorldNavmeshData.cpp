#include "WorldNavmeshData.h"

#include <Base/CVarSystem/CVarSystem.h>
#include <FileFormat/Novus/NavMesh/NavMesh.h>

#include <Detour/DetourAlloc.h>
#include <Detour/DetourNavMeshQuery.h>

#include <robinhood/robinhood.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace ECS
{
    namespace
    {
        constexpr i32 MAX_QUERY_NODES = 8192;
        constexpr i32 MAX_PATH_POLYS = 1024;
        constexpr u16 WALKABLE_POLY_FLAGS = 0x1;
        constexpr u32 MAX_64BIT_NAVMESH_TILES = 1u << DT_TILE_BITS;
        constexpr u32 MAX_64BIT_POLYS_PER_TILE = 1u << DT_POLY_BITS;
        constexpr f32 TERRAIN_POSITION_EPSILON = 0.01f;

        AutoCVar_Int CVAR_NavmeshTerrainHeightNormalization(
            CVarCategory::Server,
            "navmesh.terrainHeightNormalization",
            "Use extracted terrain height sidecars to normalize navmesh path and movement results.",
            1,
            CVarFlags::EditCheckbox);

        struct TerrainHeightTile
        {
            NavMesh::TerrainHeight::Header header;
            std::array<f32, NavMesh::TerrainHeight::HEIGHT_COUNT> heights;
            std::array<u64, NavMesh::TerrainHeight::HOLE_COUNT> holes;
        };

        enum class TerrainHeightQueryResult
        {
            Outside,
            NoHeight,
            Height
        };

        u32 GetChunkKey(u32 chunkX, u32 chunkY)
        {
            return chunkY * Terrain::CHUNK_NUM_PER_MAP_STRIDE + chunkX;
        }

        vec3 ToDetourPosition(const vec3& position)
        {
            return vec3(position.x, position.y, -position.z);
        }

        vec3 FromDetourPosition(const vec3& position)
        {
            return vec3(position.x, position.y, -position.z);
        }

        vec2 GetTerrainVertexPosition(u32 vertexID, f32 patchSize)
        {
            u32 vertexX = vertexID % 17;
            u32 vertexZ = vertexID / 17;
            bool isInnerVertex = vertexX > 8;
            f32 offsetX = isInnerVertex ? -8.5f : 0.0f;
            f32 offsetZ = isInnerVertex ? 0.5f : 0.0f;
            return vec2((static_cast<f32>(vertexX) + offsetX) * patchSize, (static_cast<f32>(vertexZ) + offsetZ) * patchSize);
        }

        bool InterpolateTriangleHeight(const vec2& point, const vec2& a, const vec2& b, const vec2& c, f32 heightA, f32 heightB, f32 heightC, f32& height)
        {
            f32 denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
            if (std::abs(denominator) <= std::numeric_limits<f32>::epsilon())
                return false;

            f32 weightA = ((b.y - c.y) * (point.x - c.x) + (c.x - b.x) * (point.y - c.y)) / denominator;
            f32 weightB = ((c.y - a.y) * (point.x - c.x) + (a.x - c.x) * (point.y - c.y)) / denominator;
            f32 weightC = 1.0f - weightA - weightB;
            height = weightA * heightA + weightB * heightB + weightC * heightC;
            return std::isfinite(height);
        }

        TerrainHeightQueryResult QueryTerrainHeightTile(const TerrainHeightTile& tile, f32 detourX, f32 detourZ, f32& height)
        {
            f32 localX = detourX - tile.header.originX;
            f32 localZ = detourZ - tile.header.originZ;
            if (localX < -TERRAIN_POSITION_EPSILON || localZ < -TERRAIN_POSITION_EPSILON ||
                localX > tile.header.chunkSize + TERRAIN_POSITION_EPSILON || localZ > tile.header.chunkSize + TERRAIN_POSITION_EPSILON)
                return TerrainHeightQueryResult::Outside;

            f32 maximumLocalPosition = std::nextafter(tile.header.chunkSize, 0.0f);
            localX = std::clamp(localX, 0.0f, maximumLocalPosition);
            localZ = std::clamp(localZ, 0.0f, maximumLocalPosition);

            f32 cellSize = tile.header.chunkSize / static_cast<f32>(Terrain::CHUNK_NUM_CELLS_PER_STRIDE);
            f32 patchSize = cellSize / 8.0f;
            u32 cellX = std::min(static_cast<u32>(localX / cellSize), Terrain::CHUNK_NUM_CELLS_PER_STRIDE - 1);
            u32 cellZ = std::min(static_cast<u32>(localZ / cellSize), Terrain::CHUNK_NUM_CELLS_PER_STRIDE - 1);
            f32 cellLocalX = localX - static_cast<f32>(cellX) * cellSize;
            f32 cellLocalZ = localZ - static_cast<f32>(cellZ) * cellSize;
            u32 patchX = std::min(static_cast<u32>(cellLocalX / patchSize), 7u);
            u32 patchZ = std::min(static_cast<u32>(cellLocalZ / patchSize), 7u);
            u32 patchID = patchX + patchZ * 8;
            u32 cellID = cellX + cellZ * Terrain::CHUNK_NUM_CELLS_PER_STRIDE;

            if ((tile.holes[cellID] & (1ull << patchID)) != 0)
                return TerrainHeightQueryResult::NoHeight;

            f32 patchFractionX = std::clamp((cellLocalX - static_cast<f32>(patchX) * patchSize) / patchSize, 0.0f, 1.0f);
            f32 patchFractionZ = std::clamp((cellLocalZ - static_cast<f32>(patchZ) * patchSize) / patchSize, 0.0f, 1.0f);
            f32 deltaX = patchFractionX - 0.5f;
            f32 deltaZ = patchFractionZ - 0.5f;

            u32 topLeft = patchX + patchZ * 17;
            u32 topRight = topLeft + 1;
            u32 bottomLeft = topLeft + 17;
            u32 bottomRight = bottomLeft + 1;
            u32 center = topLeft + 9;

            std::array<u32, 3> triangle;
            if (std::abs(deltaX) > std::abs(deltaZ))
                triangle = deltaX > 0.0f ? std::array<u32, 3>{ center, bottomRight, topRight } : std::array<u32, 3>{ center, topLeft, bottomLeft };
            else
                triangle = deltaZ > 0.0f ? std::array<u32, 3>{ center, bottomLeft, bottomRight } : std::array<u32, 3>{ center, topRight, topLeft };

            vec2 point(cellLocalX, cellLocalZ);
            u32 heightOffset = cellID * Terrain::CELL_TOTAL_GRID_SIZE;
            bool interpolated = InterpolateTriangleHeight(
                point,
                GetTerrainVertexPosition(triangle[0], patchSize),
                GetTerrainVertexPosition(triangle[1], patchSize),
                GetTerrainVertexPosition(triangle[2], patchSize),
                tile.heights[heightOffset + triangle[0]],
                tile.heights[heightOffset + triangle[1]],
                tile.heights[heightOffset + triangle[2]],
                height);
            return interpolated ? TerrainHeightQueryResult::Height : TerrainHeightQueryResult::NoHeight;
        }

        u64 AlignToFourBytes(u64 size)
        {
            return (size + 3) & ~3ull;
        }

        bool AddAlignedArraySize(u64& totalSize, u64 elementSize, i32 elementCount)
        {
            if (elementCount < 0)
                return false;

            u64 count = static_cast<u64>(elementCount);
            if (count > std::numeric_limits<u64>::max() / elementSize)
                return false;

            u64 arraySize = AlignToFourBytes(elementSize * count);
            if (totalSize > std::numeric_limits<u64>::max() - arraySize)
                return false;

            totalSize += arraySize;
            return true;
        }

        bool HasExpectedTileDataSize(const dtMeshHeader& header, u32 navDataSize)
        {
            if (header.polyCount <= 0 || header.maxLinkCount <= 0)
                return false;

            u64 expectedSize = AlignToFourBytes(sizeof(dtMeshHeader));
            if (!AddAlignedArraySize(expectedSize, sizeof(f32) * 3, header.vertCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(dtPoly), header.polyCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(dtLink), header.maxLinkCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(dtPolyDetail), header.detailMeshCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(f32) * 3, header.detailVertCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(u8) * 4, header.detailTriCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(dtBVNode), header.bvNodeCount) ||
                !AddAlignedArraySize(expectedSize, sizeof(dtOffMeshConnection), header.offMeshConCount))
                return false;

            return expectedSize == navDataSize;
        }
    }

    struct WorldNavmeshData::State
    {
        dtNavMesh* navMesh = nullptr;

        mutable std::shared_mutex navMeshMutex;
        mutable std::mutex queryPoolMutex;
        mutable std::vector<dtNavMeshQuery*> queryPool;

        robin_hood::unordered_map<u32, dtTileRef> chunkToTileRef;
        robin_hood::unordered_map<u32, std::unique_ptr<TerrainHeightTile>> chunkToHeightTile;
        u32 maxPolys = 0;

        bool GetTerrainHeight(f32 detourX, f32 detourZ, f32& height) const
        {
            if (!std::isfinite(detourX) || !std::isfinite(detourZ) ||
                detourX < -Terrain::MAP_HALF_SIZE - TERRAIN_POSITION_EPSILON ||
                detourZ < -Terrain::MAP_HALF_SIZE - TERRAIN_POSITION_EPSILON ||
                detourX > Terrain::MAP_HALF_SIZE + TERRAIN_POSITION_EPSILON ||
                detourZ > Terrain::MAP_HALF_SIZE + TERRAIN_POSITION_EPSILON)
                return false;

            i32 primaryChunkX = static_cast<i32>(std::floor((detourX + Terrain::MAP_HALF_SIZE) / Terrain::CHUNK_SIZE));
            i32 primaryChunkZ = static_cast<i32>(std::floor((detourZ + Terrain::MAP_HALF_SIZE) / Terrain::CHUNK_SIZE));
            primaryChunkX = std::clamp(primaryChunkX, 0, static_cast<i32>(Terrain::CHUNK_NUM_PER_MAP_STRIDE) - 1);
            primaryChunkZ = std::clamp(primaryChunkZ, 0, static_cast<i32>(Terrain::CHUNK_NUM_PER_MAP_STRIDE) - 1);

            auto primaryItr = chunkToHeightTile.find(GetChunkKey(static_cast<u32>(primaryChunkX), static_cast<u32>(primaryChunkZ)));
            if (primaryItr != chunkToHeightTile.end())
            {
                TerrainHeightQueryResult result = QueryTerrainHeightTile(*primaryItr->second, detourX, detourZ, height);
                if (result != TerrainHeightQueryResult::Outside)
                    return result == TerrainHeightQueryResult::Height;
            }

            for (i32 offsetZ = -1; offsetZ <= 1; offsetZ++)
            {
                for (i32 offsetX = -1; offsetX <= 1; offsetX++)
                {
                    i32 chunkX = primaryChunkX + offsetX;
                    i32 chunkZ = primaryChunkZ + offsetZ;
                    if ((offsetX == 0 && offsetZ == 0) || chunkX < 0 || chunkZ < 0 ||
                        chunkX >= static_cast<i32>(Terrain::CHUNK_NUM_PER_MAP_STRIDE) ||
                        chunkZ >= static_cast<i32>(Terrain::CHUNK_NUM_PER_MAP_STRIDE))
                        continue;

                    auto itr = chunkToHeightTile.find(GetChunkKey(static_cast<u32>(chunkX), static_cast<u32>(chunkZ)));
                    if (itr == chunkToHeightTile.end())
                        continue;

                    TerrainHeightQueryResult result = QueryTerrainHeightTile(*itr->second, detourX, detourZ, height);
                    if (result != TerrainHeightQueryResult::Outside)
                        return result == TerrainHeightQueryResult::Height;
                }
            }

            return false;
        }

        dtNavMeshQuery* AcquireQuery() const
        {
            std::scoped_lock lock(queryPoolMutex);
            if (!queryPool.empty())
            {
                dtNavMeshQuery* query = queryPool.back();
                queryPool.pop_back();
                return query;
            }

            dtNavMeshQuery* query = dtAllocNavMeshQuery();
            if (!query)
                return nullptr;

            if (dtStatusFailed(query->init(navMesh, MAX_QUERY_NODES)))
            {
                dtFreeNavMeshQuery(query);
                return nullptr;
            }

            return query;
        }

        void ReleaseQuery(dtNavMeshQuery* query) const
        {
            if (!query)
                return;

            std::scoped_lock lock(queryPoolMutex);
            queryPool.push_back(query);
        }

        void FreeQueries()
        {
            std::scoped_lock lock(queryPoolMutex);
            for (dtNavMeshQuery* query : queryPool)
            {
                dtFreeNavMeshQuery(query);
            }
            queryPool.clear();
        }
    };

    WorldNavmeshData::WorldNavmeshData()
        : _state(std::make_unique<State>())
    {
    }

    WorldNavmeshData::QueryContext::QueryContext(const State* state)
        : _state(state)
    {
        if (!_state)
            return;

        _lock = std::shared_lock(_state->navMeshMutex);
        if (!_state->navMesh || !(_query = _state->AcquireQuery()))
        {
            _state = nullptr;
            _lock.unlock();
        }
    }

    WorldNavmeshData::QueryContext::QueryContext(QueryContext&& other) noexcept
        : _state(other._state)
        , _lock(std::move(other._lock))
        , _query(other._query)
    {
        other._state = nullptr;
        other._query = nullptr;
    }

    WorldNavmeshData::QueryContext& WorldNavmeshData::QueryContext::operator=(QueryContext&& other) noexcept
    {
        if (this == &other)
            return *this;

        Release();
        _state = other._state;
        _lock = std::move(other._lock);
        _query = other._query;
        other._state = nullptr;
        other._query = nullptr;
        return *this;
    }

    WorldNavmeshData::QueryContext::~QueryContext()
    {
        Release();
    }

    void WorldNavmeshData::QueryContext::Release()
    {
        if (_query)
            _state->ReleaseQuery(_query);

        _query = nullptr;
        _state = nullptr;
    }

    WorldNavmeshData::~WorldNavmeshData()
    {
        Cleanup();
    }

    WorldNavmeshData::WorldNavmeshData(WorldNavmeshData&& other) noexcept = default;
    WorldNavmeshData& WorldNavmeshData::operator=(WorldNavmeshData&& other) noexcept
    {
        if (this == &other)
            return *this;

        Cleanup();
        _state = std::move(other._state);
        return *this;
    }

    WorldNavmeshData::QueryContext WorldNavmeshData::CreateQueryContext() const
    {
        return QueryContext(_state.get());
    }

    bool WorldNavmeshData::Initialize(u32 maxTiles, u32 maxPolys)
    {
        if (maxTiles == 0 || maxTiles > MAX_64BIT_NAVMESH_TILES ||
            maxPolys == 0 || maxPolys > MAX_64BIT_POLYS_PER_TILE)
            return false;

        if (!_state)
            _state = std::make_unique<State>();

        Cleanup();

        std::unique_lock lock(_state->navMeshMutex);

        dtNavMeshParams params{};
        params.orig[0] = -Terrain::MAP_HALF_SIZE;
        params.orig[1] = 0.0f;
        params.orig[2] = -Terrain::MAP_HALF_SIZE;
        params.tileWidth = Terrain::CHUNK_SIZE;
        params.tileHeight = Terrain::CHUNK_SIZE;
        params.maxTiles = static_cast<i32>(maxTiles);
        params.maxPolys = static_cast<i32>(maxPolys);

        _state->navMesh = dtAllocNavMesh();
        if (!_state->navMesh)
            return false;

        if (dtStatusFailed(_state->navMesh->init(&params)))
        {
            dtFreeNavMesh(_state->navMesh);
            _state->navMesh = nullptr;
            return false;
        }

        _state->maxPolys = maxPolys;
        return true;
    }

    void WorldNavmeshData::Cleanup()
    {
        if (!_state)
            return;

        std::unique_lock lock(_state->navMeshMutex);

        _state->FreeQueries();

        if (_state->navMesh)
        {
            for (const auto& itr : _state->chunkToTileRef)
            {
                _state->navMesh->removeTile(itr.second, nullptr, nullptr);
            }

            dtFreeNavMesh(_state->navMesh);
            _state->navMesh = nullptr;
        }

        _state->chunkToTileRef.clear();
        _state->chunkToHeightTile.clear();
        _state->maxPolys = 0;
    }

    bool WorldNavmeshData::AddTile(u32 chunkX, u32 chunkY, const u8* navData, u32 navDataSize)
    {
        if (!_state || !navData || navDataSize < sizeof(dtMeshHeader) || navDataSize > static_cast<u32>(std::numeric_limits<i32>::max()))
            return false;

        if (chunkX >= Terrain::CHUNK_NUM_PER_MAP_STRIDE || chunkY >= Terrain::CHUNK_NUM_PER_MAP_STRIDE)
            return false;

        const dtMeshHeader* header = reinterpret_cast<const dtMeshHeader*>(navData);
        if (header->magic != DT_NAVMESH_MAGIC || header->version != DT_NAVMESH_VERSION)
            return false;

        if (header->x != static_cast<i32>(chunkX) || header->y != static_cast<i32>(chunkY) || header->layer != static_cast<i32>(NavMesh::TILE_LAYER))
            return false;

        if (!HasExpectedTileDataSize(*header, navDataSize))
            return false;

        u8* ownedNavData = static_cast<u8*>(dtAlloc(navDataSize, DT_ALLOC_PERM));
        if (!ownedNavData)
            return false;

        std::memcpy(ownedNavData, navData, navDataSize);

        std::unique_lock lock(_state->navMeshMutex);

        if (!_state->navMesh)
        {
            dtFree(ownedNavData);
            return false;
        }

        if (header->polyCount > static_cast<i32>(_state->maxPolys))
        {
            dtFree(ownedNavData);
            return false;
        }

        u32 chunkKey = GetChunkKey(chunkX, chunkY);
        if (_state->chunkToTileRef.contains(chunkKey))
        {
            dtFree(ownedNavData);
            return true;
        }

        dtTileRef tileRef = 0;
        dtStatus status = _state->navMesh->addTile(ownedNavData, static_cast<i32>(navDataSize), DT_TILE_FREE_DATA, 0, &tileRef);
        if (dtStatusFailed(status))
        {
            dtFree(ownedNavData);
            return false;
        }

        _state->chunkToTileRef[chunkKey] = tileRef;
        return true;
    }

    bool WorldNavmeshData::AddHeightTile(u32 chunkX, u32 chunkY, const u8* heightData, u32 heightDataSize)
    {
        if (!_state || !heightData || heightDataSize < sizeof(NavMesh::TerrainHeight::Header))
            return false;

        if (chunkX >= Terrain::CHUNK_NUM_PER_MAP_STRIDE || chunkY >= Terrain::CHUNK_NUM_PER_MAP_STRIDE)
            return false;

        NavMesh::TerrainHeight::Header header;
        std::memcpy(&header, heightData, sizeof(header));

        if (!NavMesh::TerrainHeight::IsValidHeader(header) ||
            header.chunkX != chunkX || header.chunkY != chunkY ||
            !std::isfinite(header.originX) || !std::isfinite(header.originZ) || !std::isfinite(header.chunkSize) ||
            std::abs(header.chunkSize - Terrain::CHUNK_SIZE) > TERRAIN_POSITION_EPSILON ||
            header.reserved != 0)
            return false;

        u64 expectedSize = static_cast<u64>(sizeof(NavMesh::TerrainHeight::Header)) +
            static_cast<u64>(sizeof(f32)) * header.heightCount +
            static_cast<u64>(sizeof(u64)) * header.holeCount;
        if (expectedSize != heightDataSize)
            return false;

        f32 expectedOriginX = -Terrain::MAP_HALF_SIZE + static_cast<f32>(chunkX) * Terrain::CHUNK_SIZE;
        f32 expectedOriginZ = -Terrain::MAP_HALF_SIZE + static_cast<f32>(chunkY) * Terrain::CHUNK_SIZE;
        if (std::abs(header.originX - expectedOriginX) > TERRAIN_POSITION_EPSILON ||
            std::abs(header.originZ - expectedOriginZ) > TERRAIN_POSITION_EPSILON)
            return false;

        auto tile = std::make_unique<TerrainHeightTile>();
        tile->header = header;
        const u8* heightBytes = heightData + sizeof(NavMesh::TerrainHeight::Header);
        const u8* holeBytes = heightBytes + sizeof(f32) * header.heightCount;
        std::memcpy(tile->heights.data(), heightBytes, sizeof(f32) * header.heightCount);
        std::memcpy(tile->holes.data(), holeBytes, sizeof(u64) * header.holeCount);

        for (f32 height : tile->heights)
        {
            if (!std::isfinite(height))
                return false;
        }

        std::unique_lock lock(_state->navMeshMutex);

        u32 chunkKey = GetChunkKey(chunkX, chunkY);
        if (!_state->navMesh || !_state->chunkToTileRef.contains(chunkKey))
            return false;

        if (_state->chunkToHeightTile.contains(chunkKey))
            return true;

        _state->chunkToHeightTile[chunkKey] = std::move(tile);
        return true;
    }

    bool WorldNavmeshData::RemoveTile(u32 chunkX, u32 chunkY)
    {
        if (!_state)
            return false;

        if (chunkX >= Terrain::CHUNK_NUM_PER_MAP_STRIDE || chunkY >= Terrain::CHUNK_NUM_PER_MAP_STRIDE)
            return false;

        std::unique_lock lock(_state->navMeshMutex);

        if (!_state->navMesh)
            return false;

        auto itr = _state->chunkToTileRef.find(GetChunkKey(chunkX, chunkY));
        if (itr == _state->chunkToTileRef.end())
            return false;

        if (dtStatusFailed(_state->navMesh->removeTile(itr->second, nullptr, nullptr)))
            return false;

        _state->chunkToHeightTile.erase(GetChunkKey(chunkX, chunkY));
        _state->chunkToTileRef.erase(itr);
        return true;
    }

    bool WorldNavmeshData::HasTile(u32 chunkX, u32 chunkY) const
    {
        if (!_state || chunkX >= Terrain::CHUNK_NUM_PER_MAP_STRIDE || chunkY >= Terrain::CHUNK_NUM_PER_MAP_STRIDE)
            return false;

        std::shared_lock lock(_state->navMeshMutex);
        return _state->chunkToTileRef.contains(GetChunkKey(chunkX, chunkY));
    }

    bool WorldNavmeshData::HasHeightTile(u32 chunkX, u32 chunkY) const
    {
        if (!_state || chunkX >= Terrain::CHUNK_NUM_PER_MAP_STRIDE || chunkY >= Terrain::CHUNK_NUM_PER_MAP_STRIDE)
            return false;

        std::shared_lock lock(_state->navMeshMutex);
        return _state->chunkToHeightTile.contains(GetChunkKey(chunkX, chunkY));
    }

    u32 WorldNavmeshData::GetNumTiles() const
    {
        if (!_state)
            return 0;

        std::shared_lock lock(_state->navMeshMutex);
        return static_cast<u32>(_state->chunkToTileRef.size());
    }

    u32 WorldNavmeshData::GetNumHeightTiles() const
    {
        if (!_state)
            return 0;

        std::shared_lock lock(_state->navMeshMutex);
        return static_cast<u32>(_state->chunkToHeightTile.size());
    }

    bool WorldNavmeshData::FindPath(const vec3& startPosition, const vec3& endPosition, const vec3& searchExtents, vec3* pathPositions, u32 pathCapacity, u32& pathCount, dtPolyRef* startRef, dtPolyRef* endRef) const
    {
        QueryContext queryContext = CreateQueryContext();
        return FindPath(queryContext, startPosition, endPosition, searchExtents, pathPositions, pathCapacity, pathCount, startRef, endRef);
    }

    bool WorldNavmeshData::FindPath(QueryContext& queryContext, const vec3& startPosition, const vec3& endPosition, const vec3& searchExtents, vec3* pathPositions, u32 pathCapacity, u32& pathCount, dtPolyRef* startRef, dtPolyRef* endRef) const
    {
        pathCount = 0;
        if (startRef)
            *startRef = 0;
        if (endRef)
            *endRef = 0;

        if (!_state || queryContext._state != _state.get() || !queryContext._query || !pathPositions || pathCapacity == 0 || pathCapacity > static_cast<u32>(MAX_PATH_POLYS))
            return false;

        dtNavMeshQuery* query = queryContext._query;

        dtQueryFilter filter;
        filter.setIncludeFlags(WALKABLE_POLY_FLAGS);
        filter.setExcludeFlags(0);

        vec3 detourStart = ToDetourPosition(startPosition);
        vec3 detourEnd = ToDetourPosition(endPosition);

        dtPolyRef nearestStartRef = 0;
        dtPolyRef nearestEndRef = 0;
        if (dtStatusFailed(query->findNearestPoly(&detourStart.x, &searchExtents.x, &filter, &nearestStartRef, nullptr)) || nearestStartRef == 0)
            return false;
        if (dtStatusFailed(query->findNearestPoly(&detourEnd.x, &searchExtents.x, &filter, &nearestEndRef, nullptr)) || nearestEndRef == 0)
            return false;

        if (dtStatusFailed(query->closestPointOnPoly(nearestStartRef, &detourStart.x, &detourStart.x, nullptr)))
            return false;
        if (dtStatusFailed(query->closestPointOnPoly(nearestEndRef, &detourEnd.x, &detourEnd.x, nullptr)))
            return false;

        std::array<dtPolyRef, MAX_PATH_POLYS> polygonPath;
        i32 polygonPathCount = 0;
        dtStatus pathStatus = query->findPath(nearestStartRef, nearestEndRef, &detourStart.x, &detourEnd.x, &filter, polygonPath.data(), &polygonPathCount, static_cast<i32>(polygonPath.size()));
        if (dtStatusFailed(pathStatus) ||
            dtStatusDetail(pathStatus, DT_PARTIAL_RESULT | DT_BUFFER_TOO_SMALL | DT_OUT_OF_NODES) ||
            polygonPathCount == 0)
            return false;

        vec3 pathEnd = detourEnd;
        if (polygonPath[polygonPathCount - 1] != nearestEndRef)
        {
            if (dtStatusFailed(query->closestPointOnPoly(polygonPath[polygonPathCount - 1], &detourEnd.x, &pathEnd.x, nullptr)))
                return false;
        }

        thread_local std::array<f32, MAX_PATH_POLYS * 3> detourPath;
        i32 straightPathCount = 0;
        dtStatus straightPathStatus = query->findStraightPath(&detourStart.x, &pathEnd.x, polygonPath.data(), polygonPathCount, detourPath.data(), nullptr, nullptr, &straightPathCount, static_cast<i32>(pathCapacity));
        if (dtStatusFailed(straightPathStatus) ||
            dtStatusDetail(straightPathStatus, DT_BUFFER_TOO_SMALL | DT_PARTIAL_RESULT) ||
            straightPathCount == 0)
            return false;

        for (i32 i = 0; i < straightPathCount; i++)
        {
            vec3 detourPosition(detourPath[i * 3], detourPath[i * 3 + 1], detourPath[i * 3 + 2]);
            f32 terrainHeight = 0.0f;
            if (CVAR_NavmeshTerrainHeightNormalization.Get() != 0 &&
                _state->GetTerrainHeight(detourPosition.x, detourPosition.z, terrainHeight))
                detourPosition.y = terrainHeight;

            pathPositions[i] = FromDetourPosition(detourPosition);
        }

        pathCount = static_cast<u32>(straightPathCount);
        if (startRef)
            *startRef = nearestStartRef;
        if (endRef)
            *endRef = nearestEndRef;

        return true;
    }

    bool WorldNavmeshData::MoveAlongSurface(dtPolyRef startRef, const vec3& startPosition, const vec3& endPosition, vec3& resultPosition, dtPolyRef& resultRef) const
    {
        resultRef = 0;
        if (!_state || startRef == 0)
            return false;

        std::shared_lock lock(_state->navMeshMutex);

        if (!_state->navMesh)
            return false;

        dtNavMeshQuery* query = _state->AcquireQuery();
        if (!query)
            return false;

        struct QueryReleaser
        {
            const State* state;
            dtNavMeshQuery* query;

            ~QueryReleaser()
            {
                state->ReleaseQuery(query);
            }
        } queryReleaser{ _state.get(), query };

        if (!_state->navMesh->isValidPolyRef(startRef))
            return false;

        dtQueryFilter filter;
        filter.setIncludeFlags(WALKABLE_POLY_FLAGS);
        filter.setExcludeFlags(0);

        vec3 detourStart = ToDetourPosition(startPosition);
        vec3 detourEnd = ToDetourPosition(endPosition);
        vec3 detourResult;

        std::array<dtPolyRef, 32> visited;
        i32 visitedCount = 0;
        dtStatus moveStatus = query->moveAlongSurface(startRef, &detourStart.x, &detourEnd.x, &filter, &detourResult.x, visited.data(), &visitedCount, static_cast<i32>(visited.size()));
        if (dtStatusFailed(moveStatus) || dtStatusDetail(moveStatus, DT_BUFFER_TOO_SMALL) || visitedCount == 0)
            return false;

        dtPolyRef finalRef = visited[visitedCount - 1];
        f32 surfaceHeight = 0.0f;
        if (dtStatusFailed(query->getPolyHeight(finalRef, &detourResult.x, &surfaceHeight)))
        {
            vec3 closestPoint;
            if (dtStatusFailed(query->closestPointOnPoly(finalRef, &detourResult.x, &closestPoint.x, nullptr)))
                return false;

            detourResult = closestPoint;
        }
        else
        {
            detourResult.y = surfaceHeight;
        }

        f32 terrainHeight = 0.0f;
        if (CVAR_NavmeshTerrainHeightNormalization.Get() != 0 &&
            _state->GetTerrainHeight(detourResult.x, detourResult.z, terrainHeight))
            detourResult.y = terrainHeight;

        resultPosition = FromDetourPosition(detourResult);
        resultRef = finalRef;
        return true;
    }

    bool WorldNavmeshData::GetTerrainHeight(const vec3& position, f32& height) const
    {
        if (!_state || CVAR_NavmeshTerrainHeightNormalization.Get() == 0)
            return false;

        vec3 detourPosition = ToDetourPosition(position);
        return _state->GetTerrainHeight(detourPosition.x, detourPosition.z, height);
    }
}
