#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
    struct AccountInfo
    {
    public:
        u64 id;
        u64 flags;
        u64 creationTimestamp;
        u64 lastLoginTimestamp;

        std::string name;
        std::string email;
    };
}