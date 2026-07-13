#pragma once

#include <Base/Types.h>

#include <FileFormat/Shared.h>

#include <robinhood/robinhood.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace ECS
{
    class WorldSpatialIndex
    {
    public:
        void Add(ObjectGUID guid, vec2 position)
        {
            Remove(guid);

            auto [entryItr, inserted] = _entries.emplace(guid, Entry{});
            Entry& entry = entryItr->second;
            entry.position = position;
            InsertIntoSector(guid, entry);
        }

        void Update(ObjectGUID guid, vec2 position)
        {
            auto entryItr = _entries.find(guid);
            if (entryItr == _entries.end())
            {
                Add(guid, position);
                return;
            }

            Entry& entry = entryItr->second;
            entry.position = position;

            const u64 sectorKey = GetSectorKey(position);
            if (entry.sectorKey == sectorKey && ContainsLoose(*entry.node, position))
                return;

            RemoveFromNode(guid, entry);
            InsertIntoSector(guid, entry);
        }

        void Remove(ObjectGUID guid)
        {
            auto entryItr = _entries.find(guid);
            if (entryItr == _entries.end())
                return;

            RemoveFromNode(guid, entryItr->second);
            _entries.erase(entryItr);
        }

        i32 GetInRect(const vec4& rect, const std::function<bool(const ObjectGUID)>& callback) const
        {
            const i32 minSectorX = GetSectorCoordinate(rect.x);
            const i32 minSectorZ = GetSectorCoordinate(rect.y);
            const i32 maxSectorX = GetSectorCoordinate(rect.z);
            const i32 maxSectorZ = GetSectorCoordinate(rect.w);

            i32 count = 0;
            for (i32 sectorZ = minSectorZ; sectorZ <= maxSectorZ; sectorZ++)
            {
                for (i32 sectorX = minSectorX; sectorX <= maxSectorX; sectorX++)
                {
                    auto sectorItr = _sectors.find(GetSectorKey(sectorX, sectorZ));
                    if (sectorItr == _sectors.end() || !sectorItr->second.root)
                        continue;

                    if (!QueryRect(*sectorItr->second.root, rect, callback, count))
                        return count;
                }
            }

            return count;
        }

        i32 GetInRadius(vec2 center, f32 radius, const std::function<bool(const ObjectGUID)>& callback) const
        {
            if (radius < 0.0f)
                return 0;

            const i32 minSectorX = GetSectorCoordinate(center.x - radius);
            const i32 minSectorZ = GetSectorCoordinate(center.y - radius);
            const i32 maxSectorX = GetSectorCoordinate(center.x + radius);
            const i32 maxSectorZ = GetSectorCoordinate(center.y + radius);
            const f32 radiusSq = radius * radius;

            i32 count = 0;
            for (i32 sectorZ = minSectorZ; sectorZ <= maxSectorZ; sectorZ++)
            {
                for (i32 sectorX = minSectorX; sectorX <= maxSectorX; sectorX++)
                {
                    auto sectorItr = _sectors.find(GetSectorKey(sectorX, sectorZ));
                    if (sectorItr == _sectors.end() || !sectorItr->second.root)
                        continue;

                    if (!QueryRadius(*sectorItr->second.root, center, radius, radiusSq, callback, count))
                        return count;
                }
            }

            return count;
        }

    private:
        static constexpr f32 SECTOR_SIZE = Terrain::CHUNK_SIZE;
        static constexpr f32 LOOSE_FACTOR = 1.5f;
        static constexpr u32 MAX_DEPTH = 4;

        struct Node
        {
        public:
            vec2 center = vec2(0.0f);
            f32 halfSize = 0.0f;
            u32 depth = 0;
            std::array<std::unique_ptr<Node>, 4> children;
            std::vector<ObjectGUID> guids;
        };

        struct Sector
        {
        public:
            std::unique_ptr<Node> root;
        };

        struct Entry
        {
        public:
            vec2 position = vec2(0.0f);
            u64 sectorKey = 0;
            Node* node = nullptr;
            u32 nodeIndex = 0;
        };

        static i32 GetSectorCoordinate(f32 coordinate)
        {
            return static_cast<i32>(std::floor(coordinate / SECTOR_SIZE));
        }

        static u64 GetSectorKey(i32 sectorX, i32 sectorZ)
        {
            return (static_cast<u64>(static_cast<u32>(sectorX)) << 32) | static_cast<u32>(sectorZ);
        }

        static u64 GetSectorKey(vec2 position)
        {
            return GetSectorKey(GetSectorCoordinate(position.x), GetSectorCoordinate(position.y));
        }

        static bool ContainsLoose(const Node& node, vec2 position)
        {
            const f32 looseHalfSize = node.halfSize * LOOSE_FACTOR;
            return std::abs(position.x - node.center.x) <= looseHalfSize &&
                std::abs(position.y - node.center.y) <= looseHalfSize;
        }

        static bool IntersectsRect(const Node& node, const vec4& rect)
        {
            const f32 looseHalfSize = node.halfSize * LOOSE_FACTOR;
            return node.center.x - looseHalfSize <= rect.z &&
                node.center.x + looseHalfSize >= rect.x &&
                node.center.y - looseHalfSize <= rect.w &&
                node.center.y + looseHalfSize >= rect.y;
        }

        static bool IntersectsCircle(const Node& node, vec2 center, f32 radius)
        {
            const f32 looseHalfSize = node.halfSize * LOOSE_FACTOR;
            const f32 nearestX = std::clamp(center.x, node.center.x - looseHalfSize, node.center.x + looseHalfSize);
            const f32 nearestY = std::clamp(center.y, node.center.y - looseHalfSize, node.center.y + looseHalfSize);
            const f32 deltaX = center.x - nearestX;
            const f32 deltaY = center.y - nearestY;
            return deltaX * deltaX + deltaY * deltaY <= radius * radius;
        }

        Node& GetOrCreateRoot(i32 sectorX, i32 sectorZ)
        {
            Sector& sector = _sectors[GetSectorKey(sectorX, sectorZ)];
            if (!sector.root)
            {
                sector.root = std::make_unique<Node>();
                sector.root->center = vec2((static_cast<f32>(sectorX) + 0.5f) * SECTOR_SIZE, (static_cast<f32>(sectorZ) + 0.5f) * SECTOR_SIZE);
                sector.root->halfSize = SECTOR_SIZE * 0.5f;
            }

            return *sector.root;
        }

        Node& GetOrCreateChild(Node& node, u32 childIndex)
        {
            std::unique_ptr<Node>& child = node.children[childIndex];
            if (!child)
            {
                const f32 childHalfSize = node.halfSize * 0.5f;
                child = std::make_unique<Node>();
                child->center = node.center + vec2((childIndex & 1) != 0 ? childHalfSize : -childHalfSize, (childIndex & 2) != 0 ? childHalfSize : -childHalfSize);
                child->halfSize = childHalfSize;
                child->depth = node.depth + 1;
            }

            return *child;
        }

        Node& FindInsertionNode(Node& root, vec2 position)
        {
            Node* node = &root;
            while (node->depth < MAX_DEPTH)
            {
                u32 childIndex = position.x >= node->center.x ? 1u : 0u;
                if (position.y >= node->center.y)
                    childIndex |= 2u;

                node = &GetOrCreateChild(*node, childIndex);
            }

            return *node;
        }

        void InsertIntoSector(ObjectGUID guid, Entry& entry)
        {
            const i32 sectorX = GetSectorCoordinate(entry.position.x);
            const i32 sectorZ = GetSectorCoordinate(entry.position.y);
            Node& node = FindInsertionNode(GetOrCreateRoot(sectorX, sectorZ), entry.position);

            entry.sectorKey = GetSectorKey(sectorX, sectorZ);
            entry.node = &node;
            entry.nodeIndex = static_cast<u32>(node.guids.size());
            node.guids.push_back(guid);
        }

        void RemoveFromNode(ObjectGUID guid, Entry& entry)
        {
            Node& node = *entry.node;
            const u32 lastIndex = static_cast<u32>(node.guids.size() - 1);
            if (entry.nodeIndex != lastIndex)
            {
                ObjectGUID movedGUID = node.guids[lastIndex];
                node.guids[entry.nodeIndex] = movedGUID;
                _entries[movedGUID].nodeIndex = entry.nodeIndex;
            }

            node.guids.pop_back();
            entry.node = nullptr;
        }

        bool QueryRect(const Node& node, const vec4& rect, const std::function<bool(const ObjectGUID)>& callback, i32& count) const
        {
            if (!IntersectsRect(node, rect))
                return true;

            for (ObjectGUID guid : node.guids)
            {
                const Entry& entry = _entries.at(guid);
                if (entry.position.x < rect.x || entry.position.x > rect.z || entry.position.y < rect.y || entry.position.y > rect.w)
                    continue;

                count++;
                if (!callback(guid))
                    return false;
            }

            for (const std::unique_ptr<Node>& child : node.children)
            {
                if (child && !QueryRect(*child, rect, callback, count))
                    return false;
            }

            return true;
        }

        bool QueryRadius(const Node& node, vec2 center, f32 radius, f32 radiusSq, const std::function<bool(const ObjectGUID)>& callback, i32& count) const
        {
            if (!IntersectsCircle(node, center, radius))
                return true;

            for (ObjectGUID guid : node.guids)
            {
                const Entry& entry = _entries.at(guid);
                const vec2 delta = entry.position - center;
                if (delta.x * delta.x + delta.y * delta.y > radiusSq)
                    continue;

                count++;
                if (!callback(guid))
                    return false;
            }

            for (const std::unique_ptr<Node>& child : node.children)
            {
                if (child && !QueryRadius(*child, center, radius, radiusSq, callback, count))
                    return false;
            }

            return true;
        }

        robin_hood::unordered_map<u64, Sector> _sectors;
        robin_hood::unordered_map<ObjectGUID, Entry> _entries;
    };
}
