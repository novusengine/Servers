#pragma once

#include <Server-Common/Database/OperationResult.h>

#include <Base/Types.h>

#include <algorithm>

namespace ECS::Util::DatabaseRetry
{
    constexpr bool IsDeferredRetryable(Database::OperationFailure failure)
    {
        return failure == Database::OperationFailure::Unavailable || failure == Database::OperationFailure::Conflict;
    }

    constexpr u64 DelaySeconds(u8 retryCount, u64 entropy, u8 maximumExponent = 3)
    {
        const u8 exponent = std::min(retryCount, maximumExponent);
        const u64 baseDelay = u64{ 1 } << exponent;
        const u64 jitter = entropy % (std::max<u64>(1, baseDelay / 2) + 1);
        return baseDelay + jitter;
    }
}
