#include "CollisionUtil.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/Transform.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ECS::Util::Collision
{
    // Convert a local-space AABB under TRS to a world-space (min,max) AABB.
    // Uses worldHalf = |R*S| * localHalf
    inline void LocalAABBToWorldAABB(const Components::Transform& t, const Components::AABB& local, vec3& outMin, vec3& outMax)
    {
        const quat q = glm::normalize(t.rotation);
        const mat3x3 R = glm::mat3_cast(q);

        // Guard against degenerate scale to avoid infinities on crazy pipelines
        const vec3 s = glm::max(t.scale, glm::vec3(1e-6f));
        const mat3x3 S = glm::mat3(glm::vec3(s.x, 0, 0), vec3(0, s.y, 0), vec3(0, 0, s.z));
        const mat3x3 M = R * S;

        const vec3 worldCenter = t.position + M * local.centerPos;

        // |M| as "componentwise abs on columns"
        const mat3x3 A = mat3x3(glm::abs(M[0]), glm::abs(M[1]), glm::abs(M[2]));
        const vec3 worldHalf = A * local.extents;

        outMin = worldCenter - worldHalf;
        outMax = worldCenter + worldHalf;
    }

    // Point-in-AABB
    bool IsInside(const Components::Transform& pointTransform, const Components::Transform& boxTransform, const Components::AABB& boxLocalAABB)
    {
        vec3 minW, maxW;
        LocalAABBToWorldAABB(boxTransform, boxLocalAABB, minW, maxW);

        const vec3 p = pointTransform.position;
        return (p.x >= minW.x && p.x <= maxW.x) &&
            (p.y >= minW.y && p.y <= maxW.y) &&
            (p.z >= minW.z && p.z <= maxW.z);
    }

    // AABB-vs-AABB overlap (both AABBs defined in their own local spaces)
    bool Overlaps(const Components::Transform& aT, const Components::AABB& aLocal, const Components::Transform& bT, const Components::AABB& bLocal)
    {
        vec3 aMin, aMax, bMin, bMax;
        LocalAABBToWorldAABB(aT, aLocal, aMin, aMax);
        LocalAABBToWorldAABB(bT, bLocal, bMin, bMax);

        return (aMin.x <= bMax.x && aMax.x >= bMin.x) &&
            (aMin.y <= bMax.y && aMax.y >= bMin.y) &&
            (aMin.z <= bMax.z && aMax.z >= bMin.z);
    }
}