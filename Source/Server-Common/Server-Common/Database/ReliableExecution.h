#pragma once

#include "DBController.h"
#include "OperationResult.h"

#include <Base/Util/DebugHandler.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace Database
{
    namespace Detail
    {
        template <typename Callback>
        auto RunReliable(std::string_view kind, std::string_view operation, Callback&& callback, int attempts)
            -> OperationResult<std::invoke_result_t<Callback>>
        {
            using Value = std::invoke_result_t<Callback>;
            try
            {
                const int boundedAttempts = std::max(1, attempts);
                for (int attempt = 0; attempt < boundedAttempts; ++attempt)
                {
                    try
                    {
                        return OperationResult<Value>(callback());
                    }
                    catch (const pqxx::in_doubt_error&) { throw; }
                    catch (const pqxx::statement_completion_unknown&) { throw; }
                    catch (const pqxx::protocol_violation&) { throw; }
                    catch (const pqxx::broken_connection&)
                    {
                        if (attempt + 1 == boundedAttempts)
                            throw;
                    }
                    catch (const pqxx::transaction_rollback&)
                    {
                        if (attempt + 1 == boundedAttempts)
                            throw;
                    }

                    const u32 baseDelayMilliseconds = 5u << std::min(attempt, 4);
                    const u64 entropy = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count())
                        ^ static_cast<u64>(std::hash<std::string_view>{}(operation));
                    const u32 jitterMilliseconds = static_cast<u32>(entropy % (baseDelayMilliseconds + 1));
                    std::this_thread::sleep_for(std::chrono::milliseconds(baseDelayMilliseconds + jitterMilliseconds));
                }
                throw std::logic_error("Database retry loop exhausted without an outcome");
            }
            catch (const pqxx::in_doubt_error& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' has an indeterminate completion outcome: {2}", kind, operation, e.what());
                return OperationResult<Value>(OperationFailure::Indeterminate);
            }
            catch (const pqxx::statement_completion_unknown& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' has an indeterminate statement outcome: {2}", kind, operation, e.what());
                return OperationResult<Value>(OperationFailure::Indeterminate);
            }
            catch (const pqxx::protocol_violation& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' failed because the protocol state is invalid: {2}", kind, operation, e.what());
                return OperationResult<Value>(OperationFailure::Indeterminate);
            }
            catch (const pqxx::broken_connection& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' failed after reconnect attempts: {2}", kind, operation, e.what());
                return OperationResult<Value>(OperationFailure::Unavailable);
            }
            catch (const pqxx::transaction_rollback& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' was rolled back after retry attempts (SQLSTATE {2}): {3}", kind, operation, e.sqlstate(), e.what());
                return OperationResult<Value>(OperationFailure::Conflict);
            }
            catch (const pqxx::sql_error& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' was rejected (SQLSTATE {2}): {3}", kind, operation, e.sqlstate(), e.what());
                return OperationResult<Value>(OperationFailure::Rejected);
            }
            catch (const std::exception& e)
            {
                NC_LOG_ERROR("Database {0} '{1}' failed: {2}", kind, operation, e.what());
                return OperationResult<Value>(OperationFailure::Failed);
            }
        }
    }

    template <typename Callback, typename Commit>
    auto RunTransactionWithCommit(DBController& controller, DBType bundle, std::string_view operation,
        Callback&& callback, Commit&& commit, int attempts = 3)
        -> OperationResult<std::invoke_result_t<Callback, pqxx::work&>>
    {
        using Value = std::invoke_result_t<Callback, pqxx::work&>;
        static_assert(!std::is_void_v<Value>, "RunTransaction callbacks must return their committed result");

        return Detail::RunReliable("operation", operation, [&]() -> Value
        {
            auto connection = controller.GetConnection(bundle);
            if (!connection)
                throw pqxx::broken_connection("Database bundle is not configured");
            auto transaction = connection->NewTransaction(bundle);
            Value value = callback(transaction);
            commit(transaction);
            return value;
        }, attempts);
    }

    template <typename Callback>
    auto RunTransaction(DBController& controller, DBType bundle, std::string_view operation, Callback&& callback, int attempts = 3)
        -> OperationResult<std::invoke_result_t<Callback, pqxx::work&>>
    {
        return RunTransactionWithCommit(controller, bundle, operation, std::forward<Callback>(callback),
            [](pqxx::work& transaction) { transaction.commit(); }, attempts);
    }

    template <typename Callback>
    auto RunRead(DBController& controller, DBType bundle, std::string_view operation, Callback&& callback, int attempts = 3)
        -> OperationResult<std::invoke_result_t<Callback, pqxx::read_transaction&>>
    {
        using Value = std::invoke_result_t<Callback, pqxx::read_transaction&>;
        static_assert(!std::is_void_v<Value>, "RunRead callbacks must return their buffered result");

        return Detail::RunReliable("read", operation, [&]() -> Value
        {
            auto connection = controller.GetConnection(bundle);
            if (!connection)
                throw pqxx::broken_connection("Database bundle is not configured");
            auto transaction = connection->NewReadTransaction(bundle);
            transaction.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
            Value value = callback(transaction);
            transaction.commit();
            return value;
        }, attempts);
    }
}
