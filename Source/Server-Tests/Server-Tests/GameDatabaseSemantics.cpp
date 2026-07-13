#include <Server-Game/ECS/Util/DatabaseRetryUtil.h>

#include <catch2/catch2.hpp>

TEST_CASE("Game database retries only safe classified failures", "[Database][Game][Retry]")
{
    using Database::OperationFailure;
    CHECK(ECS::Util::DatabaseRetry::IsDeferredRetryable(OperationFailure::Unavailable));
    CHECK(ECS::Util::DatabaseRetry::IsDeferredRetryable(OperationFailure::Conflict));
    CHECK_FALSE(ECS::Util::DatabaseRetry::IsDeferredRetryable(OperationFailure::Indeterminate));
    CHECK_FALSE(ECS::Util::DatabaseRetry::IsDeferredRetryable(OperationFailure::Rejected));
    CHECK_FALSE(ECS::Util::DatabaseRetry::IsDeferredRetryable(OperationFailure::Failed));
}

TEST_CASE("Game database retry delay is bounded and jittered", "[Database][Game][Retry]")
{
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(0, 0) == 1);
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(0, 1) == 2);
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(3, 0) == 8);
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(3, 4) == 12);
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(20, 0) == 8);
    CHECK(ECS::Util::DatabaseRetry::DelaySeconds(20, 4) == 12);
}
