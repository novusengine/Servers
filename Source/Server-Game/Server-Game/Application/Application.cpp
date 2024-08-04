#include "Application.h"
#include "Server-Game/ECS/Scheduler.h"
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/Scripting/LuaUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Types.h>
#include <Base/CVarSystem/CVarSystem.h>
#include <Base/Math/Math.h>
#include <Base/Util/CPUInfo.h>
#include <Base/Util/Timer.h>
#include <Base/Util/JsonUtils.h>
#include <Base/Util/DebugHandler.h>

//#include <tracy/Tracy.hpp>
#include <enkiTS/TaskScheduler.h>
#include <entt/entt.hpp>

AutoCVar_Int CVAR_TickRateLimit(CVarCategory::Client, "application.tickRateLimit", "enable tickrate limit", 1, CVarFlags::EditCheckbox);
AutoCVar_Int CVAR_TickRateLimitTarget(CVarCategory::Client, "application.tickRateLimitTarget", "target tickrate while limited", 30);
AutoCVar_Int CVAR_CpuReportDetailLevel(CVarCategory::Client, "cpuReportDetailLevel", "Sets the detail level for CPU info printing on startup. (0 = No Output, 1 = CPU Name, 2 = CPU Name + Feature Support)", 1);

Application::Application() : _messagesInbound(256), _messagesOutbound(256) { }
Application::~Application() 
{
    delete _ecsScheduler;
    delete _taskScheduler;
}

void Application::Start()
{
    if (_isRunning)
        return;

    _isRunning = true;

    std::thread applicationThread = std::thread(&Application::Run, this);
    applicationThread.detach();
}

void Application::Stop()
{
    if (!_isRunning)
        return;

    NC_LOG_INFO("Application : Shutdown Initiated");
    Cleanup();
    NC_LOG_INFO("Application : Shutdown Complete");

    MessageOutbound message(MessageOutbound::Type::Exit);
    _messagesOutbound.enqueue(message);
}

void Application::Cleanup()
{
}

void Application::PassMessage(MessageInbound& message)
{
    _messagesInbound.enqueue(message);
}

bool Application::TryGetMessageOutbound(MessageOutbound& message)
{
    bool messageFound = _messagesOutbound.try_dequeue(message);
    return messageFound;
}

void Application::Run()
{
    //tracy::SetThreadName("Application Thread");

    if (Init())
    {
        Timer timer;
        while (true)
        {
            auto& timeState = _registries.gameRegistry->ctx().get<ECS::Singletons::TimeState>();

            f32 deltaTime = timer.GetDeltaTime();
            timer.Tick();

            timeState.deltaTime = deltaTime;
            timeState.epochAtFrameStart = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();;

            if (!Tick(deltaTime))
                break;

            bool limitTickRate = CVAR_TickRateLimit.Get() == 1;
            if (limitTickRate)
            {
                f32 targetTickRate = Math::Max(static_cast<f32>(CVAR_TickRateLimitTarget.Get()), 10.0f);
                f32 targetDelta = 1.0f / targetTickRate;

                for (deltaTime = timer.GetDeltaTime(); deltaTime < targetDelta; deltaTime = timer.GetDeltaTime())
                {
                    std::this_thread::yield();
                }
            }

            //FrameMark;
        }
    }

    Stop();
}

bool Application::Init()
{
    // Setup CVar Config
    {
        std::filesystem::path currentPath = std::filesystem::current_path();
        NC_LOG_INFO("Current Path : {}", currentPath.string());
        std::filesystem::create_directories("Data/Config");

        nlohmann::ordered_json fallback;
        fallback["version"] = JsonUtils::CVAR_VERSION;
        if (JsonUtils::LoadFromPathOrCreate(_cvarJson, fallback, "Data/Config/CVar.json"))
        {
            JsonUtils::VerifyCVarsOrFallback(_cvarJson, fallback);
            JsonUtils::LoadCVarsFromJson(_cvarJson);
            JsonUtils::SaveCVarsToJson(_cvarJson);
            JsonUtils::SaveToPath(_cvarJson, "Data/Config/CVar.json");
        }
    }

    // Print CPU info
    CPUInfo cpuInfo = CPUInfo::Get();
    cpuInfo.Print(CVAR_CpuReportDetailLevel.Get());

    _taskScheduler = new enki::TaskScheduler();
    _taskScheduler->Initialize();
    ServiceLocator::SetTaskScheduler(_taskScheduler);

    _registries.gameRegistry = new entt::registry();
    ServiceLocator::SetEnttRegistries(&_registries);

    _ecsScheduler = new ECS::Scheduler();
    _ecsScheduler->Init(*_registries.gameRegistry);

    Scripting::LuaUtil::DoString("print(\"Hello World :o\")");
    return true;
}

bool Application::Tick(f32 deltaTime)
{
    MessageInbound message;
    while (_messagesInbound.try_dequeue(message))
    {
        assert(message.type != MessageInbound::Type::Invalid);

        switch (message.type)
        {
            case MessageInbound::Type::Print:
            {
                NC_LOG_INFO("{}", message.data);
                break;
            }

            case MessageInbound::Type::Ping:
            {
                MessageOutbound pongMessage(MessageOutbound::Type::Pong);
                _messagesOutbound.enqueue(pongMessage);

                NC_LOG_INFO("Main Thread -> Application Thread : Ping");
                break;
            }

            case MessageInbound::Type::DoString:
            {
                if (!Scripting::LuaUtil::DoString(message.data))
                {
                    NC_LOG_ERROR("Failed to run Lua DoString");
                }
                
                break;
            }

            case MessageInbound::Type::Exit:
                return false;

            default: break;
        }
    }

    _ecsScheduler->Update(*_registries.gameRegistry, deltaTime);

    if (CVarSystem::Get()->IsDirty())
    {
        JsonUtils::SaveCVarsToJson(_cvarJson);
        JsonUtils::SaveToPath(_cvarJson, "Data/Config/CVar.json");

        CVarSystem::Get()->ClearDirty();
    }

    return true;
}