#pragma once

#include <Base/Types.h>

#include <robinhood/robinhood.h>

#include <algorithm>
#include <optional>

namespace ECS::Components
{
    struct PermissionSet
    {
    public:
        void Merge(u32 permissionID, i64 value)
        {
            auto itr = values.find(permissionID);
            if (itr == values.end())
            {
                values.emplace(permissionID, value);
                return;
            }

            i64& current = itr->second;
            if (current <= 0)
            {
                if (value <= 0)
                    current = std::min(current, value);
                return;
            }

            current = value <= 0 ? value : std::max(current, value);
        }

        [[nodiscard]] std::optional<i64> GetValue(u32 permissionID) const
        {
            auto itr = values.find(permissionID);
            if (itr == values.end())
                return std::nullopt;
            return itr->second;
        }

        robin_hood::unordered_map<u32, i64> values;
    };

    struct AccountPermissionInfo
    {
        PermissionSet permissions;
    };

    struct CharacterPermissionInfo
    {
        PermissionSet accountPermissions;
        PermissionSet effectivePermissions;
    };
}
