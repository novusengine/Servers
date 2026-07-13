#pragma once

#include <Base/Types.h>

#include <string>

namespace Database
{
    enum class DBType : u8
    {
        Invalid,
        Auth,
        Character,
        World,
        Count
    };

    enum class MigrationMode : u8
    {
        Migrate,
        Validate,
        Disabled
    };

    struct DBEntry
    {
        DBEntry() = default;
        DBEntry(std::string hostAddress, u16 hostPort, std::string db, std::string user, std::string pass,
            MigrationMode mode = MigrationMode::Migrate, bool allowCreate = false);
        void RefreshConnectionString();

        std::string address = "127.0.0.1";
        u16 port = 5432;
        std::string username = "postgres";
        std::string password = "postgres";
        std::string database = "postgres";
        std::string connectionString;
        MigrationMode migrationMode = MigrationMode::Migrate;
        bool createIfMissing = false;
        u32 connectTimeoutSeconds = 10;
        u32 statementTimeoutMilliseconds = 15000;
        u32 lockTimeoutMilliseconds = 5000;
        u32 idleInTransactionTimeoutMilliseconds = 30000;
        std::string applicationName = "novus-server";
        std::string sslMode = "prefer";
        u32 keepalivesIdleSeconds = 30;
        u32 keepalivesIntervalSeconds = 10;
        u32 keepalivesCount = 3;
        u32 maximumPoolSize = 4;
        u32 acquisitionTimeoutMilliseconds = 2000;
    };
}
