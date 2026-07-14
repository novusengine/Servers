#pragma once

#include "Server-Game/ECS/Components/PermissionInfo.h"

#include <Server-Common/Database/Definitions.h>

#include <Base/Types.h>
#include <MetaGen/Server/Permission/Permission.h>

#include <optional>
#include <string_view>

namespace ECS::Singletons
{
    struct GameCache;
}

namespace ECS::Util::Permission
{
    void Merge(Components::PermissionSet& target, const Database::PermissionTables& tables,
        const Database::PermissionAssignmentSnapshot& assignments);
    [[nodiscard]] std::optional<i64> GetValue(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, MetaGen::Server::Permission::Permission permission);
    [[nodiscard]] std::optional<i64> GetValue(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, std::string_view permissionName);
    [[nodiscard]] bool HasPermission(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, MetaGen::Server::Permission::Permission permission);
    [[nodiscard]] bool HasPermission(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, std::string_view permissionName);
}
