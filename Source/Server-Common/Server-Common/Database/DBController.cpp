#include "DBController.h"

#include <Base/Util/DebugHandler.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace Database
{
    namespace
    {
        std::string QuoteConnectionValue(std::string_view value)
        {
            std::string result("'");
            result.reserve(value.size() + 2);
            for (const char character : value)
            {
                if (character == '\\' || character == '\'')
                    result.push_back('\\');
                result.push_back(character);
            }
            result.push_back('\'');
            return result;
        }

        std::unique_ptr<DBConnection> OpenConnection(DBType type, const DBEntry& entry)
        {
            try
            {
                return std::make_unique<DBConnection>(type, entry);
            }
            catch (const pqxx::broken_connection&)
            {
                if (!entry.createIfMissing || entry.database == "postgres")
                    throw;

                DBEntry maintenance = entry;
                maintenance.database = "postgres";
                maintenance.createIfMissing = false;
                maintenance.RefreshConnectionString();
                pqxx::connection connection(maintenance.connectionString);
                pqxx::nontransaction transaction(connection);
                if (transaction.query_value<bool>("SELECT EXISTS (SELECT 1 FROM pg_database WHERE datname = $1)",
                    pqxx::params{ entry.database }))
                    throw;
                try
                {
                    transaction.exec("CREATE DATABASE " + transaction.quote_name(entry.database));
                }
                catch (const pqxx::sql_error& exception)
                {
                    if (exception.sqlstate() != "42P04")
                        throw;
                }
                return std::make_unique<DBConnection>(type, entry);
            }
        }
    }

    DBEntry::DBEntry(std::string hostAddress, u16 hostPort, std::string db, std::string user, std::string pass,
        MigrationMode mode, bool allowCreate)
        : address(std::move(hostAddress)), port(hostPort), username(std::move(user)), password(std::move(pass)),
          database(std::move(db)), migrationMode(mode), createIfMissing(allowCreate)
    {
        RefreshConnectionString();
    }

    void DBEntry::RefreshConnectionString()
    {
        connectionString = "host=" + QuoteConnectionValue(address) + " port=" + std::to_string(port)
            + " user=" + QuoteConnectionValue(username) + " password=" + QuoteConnectionValue(password)
            + " dbname=" + QuoteConnectionValue(database) + " connect_timeout=" + std::to_string(connectTimeoutSeconds)
            + " application_name=" + QuoteConnectionValue(applicationName) + " sslmode=" + QuoteConnectionValue(sslMode)
            + " keepalives=1 keepalives_idle=" + std::to_string(keepalivesIdleSeconds)
            + " keepalives_interval=" + std::to_string(keepalivesIntervalSeconds)
            + " keepalives_count=" + std::to_string(keepalivesCount)
            + " options=" + QuoteConnectionValue("-c statement_timeout=" + std::to_string(statementTimeoutMilliseconds)
                + " -c lock_timeout=" + std::to_string(lockTimeoutMilliseconds)
                + " -c idle_in_transaction_session_timeout=" + std::to_string(idleInTransactionTimeoutMilliseconds));
    }

    DBConnection::DBConnection(DBType bundleType, const DBEntry& entry) : type(bundleType)
    {
        connection = std::make_unique<pqxx::connection>(entry.connectionString);
    }

    bool DBConnection::EnsureConnected(const DBEntry& entry, const std::function<void(DBConnection&)>& prepare)
    {
        if (connection && connection->is_open())
            return false;

        connection = std::make_unique<pqxx::connection>(entry.connectionString);
        try
        {
            if (prepare)
                prepare(*this);
            return true;
        }
        catch (...)
        {
            connection.reset();
            throw;
        }
    }

    pqxx::work DBConnection::NewTransaction(DBType expectedType)
    {
        if (type != expectedType)
            throw std::logic_error("Database connection bundle does not match the requested transaction operation");
        return pqxx::work(*connection);
    }

    pqxx::read_transaction DBConnection::NewReadTransaction(DBType expectedType)
    {
        if (type != expectedType)
            throw std::logic_error("Database connection bundle does not match the requested read transaction operation");
        return pqxx::read_transaction(*connection);
    }

    DBConnectionLease::DBConnectionLease(DBController* owner, u32 bundleIndex, u32 slotIndex, DBConnection* value)
        : _owner(owner), _bundleIndex(bundleIndex), _slotIndex(slotIndex), _connection(value) {}

    DBConnectionLease::DBConnectionLease(DBConnectionLease&& other) noexcept { *this = std::move(other); }
    DBConnectionLease& DBConnectionLease::operator=(DBConnectionLease&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            _owner = std::exchange(other._owner, nullptr);
            _bundleIndex = other._bundleIndex;
            _slotIndex = other._slotIndex;
            _connection = std::exchange(other._connection, nullptr);
        }
        return *this;
    }
    DBConnectionLease::~DBConnectionLease() { Release(); }
    void DBConnectionLease::Release()
    {
        if (_owner)
            _owner->Release(_bundleIndex, _slotIndex);
        _owner = nullptr;
        _connection = nullptr;
    }

    std::optional<u32> DBController::Index(DBType type)
    {
        if (type <= DBType::Invalid || type >= DBType::Count)
            return std::nullopt;
        return static_cast<u32>(type) - 1;
    }

    bool DBController::SetDBEntry(DBType type, const DBEntry& entry)
    {
        const auto index = Index(type);
        if (!index)
            return false;
        std::scoped_lock lock(_mutex);
        auto& pool = _pools[*index];
        if (!pool.slots.empty())
            return false;
        pool.entry = entry;
        pool.configured = true;
        return true;
    }

    bool DBController::SetPrepareCallback(DBType type, PrepareCallback callback)
    {
        const auto index = Index(type);
        if (!index)
            return false;
        std::scoped_lock lock(_mutex);
        auto& pool = _pools[*index];
        if (!pool.slots.empty())
            return false;
        pool.prepare = std::move(callback);
        return true;
    }

    DBConnectionLease DBController::GetConnection(DBType type)
    {
        const auto index = Index(type);
        if (!index)
            return {};
        std::unique_lock lock(_mutex);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(_pools[*index].entry.acquisitionTimeoutMilliseconds);
        bool waited = false;
        for (;;)
        {
            auto& pool = _pools[*index];
            if (!pool.configured)
                return {};

            for (u32 i = 0; i < pool.slots.size(); ++i)
            {
                auto& slot = pool.slots[i];
                if (slot.leased || slot.connecting)
                    continue;
                if (!slot.connection)
                {
                    slot.leased = true;
                    slot.connecting = true;
                    const DBEntry entry = pool.entry;
                    const PrepareCallback prepare = pool.prepare;
                    lock.unlock();
                    try
                    {
                        auto connection = OpenConnection(type, entry);
                        if (prepare)
                            prepare(*connection);
                        DBConnection* value = connection.get();
                        lock.lock();
                        auto& reserved = _pools[*index].slots[i];
                        reserved.connection = std::move(connection);
                        reserved.connecting = false;
                        lock.unlock();
                        NC_LOG_INFO("Opened database bundle {0} pool slot {1}", static_cast<u32>(type), i);
                        return DBConnectionLease(this, *index, i, value);
                    }
                    catch (...)
                    {
                        lock.lock();
                        auto& reserved = _pools[*index].slots[i];
                        reserved.connecting = false;
                        reserved.leased = false;
                        lock.unlock();
                        _connectionAvailable.notify_one();
                        throw;
                    }
                }
                slot.leased = true;
                DBConnection* connection = slot.connection.get();
                const DBEntry entry = pool.entry;
                const PrepareCallback prepare = pool.prepare;
                lock.unlock();
                try
                {
                    if (connection->EnsureConnected(entry, prepare))
                        NC_LOG_INFO("Reconnected database bundle {0} pool slot {1}", static_cast<u32>(type), i);
                    return DBConnectionLease(this, *index, i, connection);
                }
                catch (...)
                {
                    lock.lock();
                    pool.slots[i].leased = false;
                    lock.unlock();
                    _connectionAvailable.notify_one();
                    throw;
                }
            }

            if (pool.slots.size() < std::max<u32>(1, pool.entry.maximumPoolSize))
            {
                const u32 slotIndex = static_cast<u32>(pool.slots.size());
                auto& slot = pool.slots.emplace_back();
                slot.leased = true;
                slot.connecting = true;
                const DBEntry entry = pool.entry;
                const PrepareCallback prepare = pool.prepare;
                lock.unlock();
                try
                {
                    auto connection = OpenConnection(type, entry);
                    if (prepare)
                        prepare(*connection);
                    DBConnection* value = connection.get();
                    lock.lock();
                    auto& reserved = _pools[*index].slots[slotIndex];
                    reserved.connection = std::move(connection);
                    reserved.connecting = false;
                    lock.unlock();
                    NC_LOG_INFO("Opened database bundle {0} pool slot {1} ({2}/{3} occupied)",
                        static_cast<u32>(type), slotIndex, slotIndex + 1, std::max<u32>(1, entry.maximumPoolSize));
                    return DBConnectionLease(this, *index, slotIndex, value);
                }
                catch (...)
                {
                    lock.lock();
                    auto& reserved = _pools[*index].slots[slotIndex];
                    reserved.connecting = false;
                    reserved.leased = false;
                    lock.unlock();
                    _connectionAvailable.notify_one();
                    throw;
                }
            }

            if (!waited)
            {
                NC_LOG_WARNING("Database bundle {0} pool is full ({1} connections); waiting for a lease",
                    static_cast<u32>(type), pool.slots.size());
                waited = true;
            }
            if (_connectionAvailable.wait_until(lock, deadline) == std::cv_status::timeout)
            {
                NC_LOG_ERROR("Timed out acquiring database bundle {0} connection after {1} ms",
                    static_cast<u32>(type), pool.entry.acquisitionTimeoutMilliseconds);
                return {};
            }
        }
    }

    std::unique_ptr<DBConnection> DBController::OpenDedicatedConnection(DBType type) const
    {
        const auto index = Index(type);
        if (!index)
            return {};
        DBEntry entry;
        {
            std::scoped_lock lock(_mutex);
            if (!_pools[*index].configured)
                return {};
            entry = _pools[*index].entry;
        }
        return OpenConnection(type, entry);
    }

    void DBController::Release(u32 bundleIndex, u32 slotIndex)
    {
        std::scoped_lock lock(_mutex);
        _pools[bundleIndex].slots[slotIndex].leased = false;
        _connectionAvailable.notify_one();
    }
}
