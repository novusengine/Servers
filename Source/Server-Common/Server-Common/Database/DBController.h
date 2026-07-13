#pragma once

#include "DatabaseConfiguration.h"

#include <pqxx/pqxx>

#include <array>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Database
{
    struct DBConnection
    {
        DBConnection(DBType type, const DBEntry& entry);
        DBConnection(const DBConnection&) = delete;
        DBConnection& operator=(const DBConnection&) = delete;

        bool EnsureConnected(const DBEntry& entry, const std::function<void(DBConnection&)>& prepare);
        pqxx::read_transaction NewReadTransaction(DBType expectedType);
        pqxx::work NewTransaction(DBType expectedType);

        DBType type = DBType::Invalid;
        std::unique_ptr<pqxx::connection> connection;
    };

    class DBController;

    class DBConnectionLease
    {
    public:
        DBConnectionLease() = default;
        DBConnectionLease(const DBConnectionLease&) = delete;
        DBConnectionLease& operator=(const DBConnectionLease&) = delete;
        DBConnectionLease(DBConnectionLease&& other) noexcept;
        DBConnectionLease& operator=(DBConnectionLease&& other) noexcept;
        ~DBConnectionLease();

        DBConnection* operator->() const { return _connection; }
        DBConnection& operator*() const { return *_connection; }
        explicit operator bool() const { return _connection != nullptr; }

    private:
        friend class DBController;
        DBConnectionLease(DBController* owner, u32 bundleIndex, u32 slotIndex, DBConnection* connection);
        void Release();

        DBController* _owner = nullptr;
        u32 _bundleIndex = 0;
        u32 _slotIndex = 0;
        DBConnection* _connection = nullptr;
    };

    class DBController
    {
    public:
        using PrepareCallback = std::function<void(DBConnection&)>;

        bool SetDBEntry(DBType type, const DBEntry& entry);
        bool SetPrepareCallback(DBType type, PrepareCallback callback);
        DBConnectionLease GetConnection(DBType type);
        std::unique_ptr<DBConnection> OpenDedicatedConnection(DBType type) const;

    private:
        friend class DBConnectionLease;
        struct Slot
        {
            std::unique_ptr<DBConnection> connection;
            bool leased = false;
            bool connecting = false;
        };
        struct BundlePool
        {
            DBEntry entry;
            bool configured = false;
            PrepareCallback prepare;
            std::vector<Slot> slots;
        };

        static std::optional<u32> Index(DBType type);
        void Release(u32 bundleIndex, u32 slotIndex);

        mutable std::mutex _mutex;
        std::condition_variable _connectionAvailable;
        std::array<BundlePool, static_cast<u32>(DBType::Count) - 1> _pools;
    };
}
