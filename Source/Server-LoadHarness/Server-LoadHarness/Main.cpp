#include <Base/Types.h>

#include <asio/asio.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using asio::ip::tcp;

namespace
{
    enum class WorkloadMode
    {
        ConnectOnly,
        AuthenticatedIdle,
        ReplicationSlowReader
    };

    enum class CloseMode
    {
        Abortive,
        Graceful
    };

    struct Settings
    {
        std::string host = "127.0.0.1";
        u16 port = 4000;
        u32 sessionCount = 20000;
        u32 maxInFlightConnects = 512;
        u32 holdSeconds = 60;
        WorkloadMode mode = WorkloadMode::ConnectOnly;
        CloseMode closeMode = CloseMode::Abortive;
    };

    bool ParseU32(std::string_view value, u32& result)
    {
        const auto conversion = std::from_chars(value.data(), value.data() + value.size(), result);
        return conversion.ec == std::errc() && conversion.ptr == value.data() + value.size();
    }

    bool ParseArgument(const std::string_view argument, const std::string_view name, std::string_view& value)
    {
        const std::string prefix = "--" + std::string(name) + "=";
        if (!argument.starts_with(prefix))
            return false;

        value = argument.substr(prefix.size());
        return true;
    }

    bool ParseSettings(int argumentCount, char** arguments, Settings& settings)
    {
        for (int index = 1; index < argumentCount; index++)
        {
            const std::string_view argument(arguments[index]);
            std::string_view value;
            if (ParseArgument(argument, "host", value))
            {
                settings.host = value;
                continue;
            }

            u32 parsedValue = 0;
            if (ParseArgument(argument, "port", value) && ParseU32(value, parsedValue) && parsedValue <= std::numeric_limits<u16>::max())
            {
                settings.port = static_cast<u16>(parsedValue);
                continue;
            }
            if (ParseArgument(argument, "sessions", value) && ParseU32(value, parsedValue) && parsedValue > 0)
            {
                settings.sessionCount = parsedValue;
                continue;
            }
            if (ParseArgument(argument, "in-flight", value) && ParseU32(value, parsedValue) && parsedValue > 0)
            {
                settings.maxInFlightConnects = parsedValue;
                continue;
            }
            if (ParseArgument(argument, "hold-seconds", value) && ParseU32(value, parsedValue))
            {
                settings.holdSeconds = parsedValue;
                continue;
            }
            if (ParseArgument(argument, "mode", value))
            {
                if (value == "connect")
                    settings.mode = WorkloadMode::ConnectOnly;
                else if (value == "authenticated-idle")
                    settings.mode = WorkloadMode::AuthenticatedIdle;
                else if (value == "replication-slow-reader")
                    settings.mode = WorkloadMode::ReplicationSlowReader;
                else
                    return false;

                continue;
            }
            if (ParseArgument(argument, "close", value))
            {
                if (value == "abortive")
                    settings.closeMode = CloseMode::Abortive;
                else if (value == "graceful")
                    settings.closeMode = CloseMode::Graceful;
                else
                    return false;

                continue;
            }

            return false;
        }

        return true;
    }

    void PrintUsage()
    {
        std::cout << "Server-LoadHarness options:\n"
            << "  --mode=connect|authenticated-idle|replication-slow-reader\n"
            << "  --host=127.0.0.1 --port=4000 --sessions=20000 --in-flight=512 --hold-seconds=60 --close=abortive|graceful\n"
            << "  Abortive close is the default for repeatable high-session runs; graceful close leaves client ports in TIME_WAIT.\n"
            << "  Local 20k runs require a client ephemeral TCP port range large enough for every session.\n";
    }

    class ConnectOnlyWorkload
    {
    public:
        ConnectOnlyWorkload(asio::io_context& ioContext, tcp::endpoint endpoint, const Settings& settings)
            : _ioContext(ioContext), _endpoint(std::move(endpoint)), _settings(settings)
        {
            _sockets.reserve(_settings.sessionCount);
        }

        void Run()
        {
            const auto start = std::chrono::steady_clock::now();
            StartMoreConnections();
            _ioContext.run();

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            std::cout << "Connect workload completed: " << _sockets.size() << "/" << _settings.sessionCount
                << " active, " << _failedConnections << " failed, " << elapsed.count() << " ms elapsed.\n";

            if (_settings.holdSeconds > 0 && !_sockets.empty())
            {
                std::cout << "Holding sessions for " << _settings.holdSeconds << " seconds.\n";
                std::this_thread::sleep_for(std::chrono::seconds(_settings.holdSeconds));
            }

            asio::error_code error;
            for (const std::unique_ptr<tcp::socket>& socket : _sockets)
            {
                if (_settings.closeMode == CloseMode::Graceful)
                    socket->shutdown(tcp::socket::shutdown_both, error);
                else
                    socket->set_option(asio::socket_base::linger(true, 0), error);

                socket->close(error);
            }
        }

        [[nodiscard]] bool HasFailures() const
        {
            return _failedConnections > 0;
        }

    private:
        void StartMoreConnections()
        {
            while (_inFlightConnections < _settings.maxInFlightConnects && _attemptedConnections < _settings.sessionCount)
            {
                _attemptedConnections++;
                _inFlightConnections++;

                auto socket = std::make_unique<tcp::socket>(_ioContext);
                tcp::socket* socketPointer = socket.get();
                socketPointer->async_connect(_endpoint, [this, socket = std::move(socket)](const asio::error_code& error) mutable
                {
                    _inFlightConnections--;
                    if (error)
                    {
                        _failedConnections++;
                    }
                    else
                    {
                        _sockets.push_back(std::move(socket));
                    }

                    StartMoreConnections();
                });
            }
        }

    private:
        asio::io_context& _ioContext;
        tcp::endpoint _endpoint;
        const Settings& _settings;
        std::vector<std::unique_ptr<tcp::socket>> _sockets;
        u32 _attemptedConnections = 0;
        u32 _inFlightConnections = 0;
        u32 _failedConnections = 0;
    };

    int RunConnectOnly(const Settings& settings)
    {
        asio::io_context ioContext;
        tcp::resolver resolver(ioContext);
        asio::error_code error;
        const tcp::resolver::results_type endpoints = resolver.resolve(settings.host, std::to_string(settings.port), error);
        if (error || endpoints.empty())
        {
            std::cerr << "Failed to resolve " << settings.host << ':' << settings.port << ": " << error.message() << '\n';
            return 1;
        }

        std::cout << "Starting connect-only workload against " << settings.host << ':' << settings.port
            << " with " << settings.sessionCount << " sessions and " << settings.maxInFlightConnects << " in-flight connects.\n";
        ConnectOnlyWorkload workload(ioContext, endpoints.begin()->endpoint(), settings);
        workload.Run();
        return workload.HasFailures() ? 1 : 0;
    }
}

int main(int argumentCount, char** arguments)
{
    Settings settings;
    if (!ParseSettings(argumentCount, arguments, settings))
    {
        PrintUsage();
        return 2;
    }

    switch (settings.mode)
    {
        case WorkloadMode::ConnectOnly:
            return RunConnectOnly(settings);

        case WorkloadMode::AuthenticatedIdle:
            std::cerr << "The authenticated-idle workload needs a supplied test-account/authentication scenario.\n";
            return 2;

        case WorkloadMode::ReplicationSlowReader:
            std::cerr << "The replication slow-reader workload needs a supplied character/world packet scenario.\n";
            return 2;
    }

    return 2;
}
