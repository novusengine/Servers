#pragma once
#include <Base/Types.h>

namespace ECS
{
    namespace Components
    {
        struct Transform;
        struct AABB;
    }
}

namespace ECS::Util::Collision
{
    bool IsInside(const Components::Transform& pointTransform, const Components::Transform& boxTransform, const Components::AABB& boxLocalAABB);
    bool Overlaps(const Components::Transform& aT, const Components::AABB& aLocal, const Components::Transform& bT, const Components::AABB& bLocal);
}