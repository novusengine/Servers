#include "PermissionUtil.h"

#include "Server-Game/ECS/Singletons/GameCache.h"

#include <Base/Util/DebugHandler.h>

#include <string>

namespace ECS::Util::Permission
{
    namespace
    {
        std::optional<i64> GetValueByID(const Singletons::GameCache& gameCache,
            const Components::PermissionSet& permissions, u32 permissionID)
        {
            if (const auto value = permissions.GetValue(permissionID))
                return value;

            auto defaultItr = gameCache.permissionTables.permissionIDToDefaultValue.find(permissionID);
            if (defaultItr == gameCache.permissionTables.permissionIDToDefaultValue.end())
                return std::nullopt;
            return defaultItr->second;
        }
    }

    void Merge(Components::PermissionSet& target, const Database::PermissionTables& tables,
        const Database::PermissionAssignmentSnapshot& assignments)
    {
        for (u32 groupID : assignments.permissionGroupIDs)
        {
            if (!tables.permissionGroupIDToName.contains(groupID))
            {
                NC_LOG_WARNING("Ignoring unknown permission group ID {0}", groupID);
                continue;
            }

            auto groupItr = tables.permissionGroupIDToValues.find(groupID);
            if (groupItr == tables.permissionGroupIDToValues.end())
                continue;

            for (const auto& [permissionID, value] : groupItr->second)
            {
                if (tables.permissionIDToName.contains(permissionID))
                    target.Merge(permissionID, value);
                else
                    NC_LOG_WARNING("Ignoring unknown permission ID {0} in group {1}", permissionID, groupID);
            }
        }

        for (const Database::PermissionAssignment& permission : assignments.permissions)
        {
            if (tables.permissionIDToName.contains(permission.permissionID))
                target.Merge(permission.permissionID, permission.value);
            else
                NC_LOG_WARNING("Ignoring unknown direct permission ID {0}", permission.permissionID);
        }
    }

    std::optional<i64> GetValue(const Singletons::GameCache& gameCache, const Components::PermissionSet& permissions,
        MetaGen::Server::Permission::Permission permission)
    {
        return GetValueByID(gameCache, permissions, static_cast<u32>(permission));
    }

    std::optional<i64> GetValue(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, std::string_view permissionName)
    {
        auto itr = gameCache.permissionTables.permissionNameToID.find(std::string(permissionName));
        if (itr == gameCache.permissionTables.permissionNameToID.end())
            return std::nullopt;
        return GetValueByID(gameCache, permissions, itr->second);
    }

    bool HasPermission(const Singletons::GameCache& gameCache, const Components::PermissionSet& permissions,
        MetaGen::Server::Permission::Permission permission)
    {
        const auto value = GetValue(gameCache, permissions, permission);
        return value && *value > 0;
    }

    bool HasPermission(const Singletons::GameCache& gameCache,
        const Components::PermissionSet& permissions, std::string_view permissionName)
    {
        const auto value = GetValue(gameCache, permissions, permissionName);
        return value && *value > 0;
    }
}
