#pragma once
#include <Base/Types.h>
#include <Base/Memory/SharedPool.h>

#include <pqxx/pqxx>

namespace Server::Database
{
	enum class DBType
	{
		Invalid,
		Auth,
		Character,
		World,
		Count = World
	};

	struct DBEntry
	{
	public:
		DBEntry(const std::string& hostAddress = "127.0.0.1", const u16 hostPort = 5432, const std::string& db = "postgres", const std::string& user = "postgres", const std::string& pass = "postgres") : address(hostAddress), port(hostPort), database(db), username(user), password(pass) 
		{
			connectionString = "hostaddr=" + hostAddress + " ";
			connectionString += "port=" + std::to_string(hostPort) + " ";
			connectionString += "user=" + user + " ";
			connectionString += "password=" + pass + " ";
			connectionString += "dbname=" + db + " ";
		}

	public:
		std::string address;
		u16 port;

		std::string username;
		std::string password;
		std::string database;

		std::string connectionString;

	private:
	};

	struct DBConnection
	{
	public:
		DBConnection() { }

		void Init(const std::string& connectionString)
		{
			if (connection != nullptr)
				return;

			try
			{
				connection = new pqxx::connection(connectionString);
			}
			catch (const std::exception& e)
			{
				DebugHandler::PrintWarning("{0}", e.what());
			}
        }

		pqxx::work Context() { return pqxx::work(*connection); }

	public:
		pqxx::connection* connection = nullptr;
	};

	class DBController
	{
	public:
		DBController() { }

		std::shared_ptr<DBConnection> GetConnection(const DBType type)
		{
			if (type <= DBType::Invalid || type > DBType::Count)
				return nullptr;

			u32 index = static_cast<u32>(type) - 1;
			const DBEntry& dbEntry = _dbEntries[index];
			SharedPool<DBConnection>& sharedPool = _dbConnections[index];

			auto connection = sharedPool.acquireOrCreate();
			connection->Init(dbEntry.connectionString);

			if (connection->connection == nullptr)
                return nullptr;
			
			return connection;
		}

		bool GetDBEntry(const DBType type, DBEntry& dbEntry)
		{
			if (type <= DBType::Invalid || type > DBType::Count)
				return false;

			u32 index = static_cast<u32>(type) - 1;
			dbEntry = _dbEntries[index];
			return true;
		}
		bool SetDBEntry(const DBType type, const DBEntry& dbEntry)
		{
			if (type <= DBType::Invalid || type > DBType::Count)
				return false;

			u32 index = static_cast<u32>(type) - 1;
			
			// Once we have established connections for the given DBType, we no longer allow SetDBEntry
			if (_dbConnectionsCount[index] > 0)
				return false;

			_dbEntries[index] = dbEntry;
			return true;
		}

	private:
		DBEntry _dbEntries[static_cast<u32>(DBType::Count)];
		u32 _dbConnectionsCount[static_cast<u32>(DBType::Count)] = { 0 };
		SharedPool<DBConnection> _dbConnections[static_cast<u32>(DBType::Count)];
	};
}