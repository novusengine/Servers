#include "Application.h"
#include "Gameserver/ECS/Scheduler.h"
#include "Gameserver/Scripting/LuaUtil.h"
#include "Gameserver/Util/ServiceLocator.h"

#include <Base/Types.h>
#include <Base/CVarSystem/CVarSystem.h>
#include <Base/Math/Math.h>
#include <Base/Util/Timer.h>
#include <Base/Util/JsonUtils.h>
#include <Base/Util/DebugHandler.h>

//#include <tracy/Tracy.hpp>
#include <enkiTS/TaskScheduler.h>
#include <entt/entt.hpp>

AutoCVar_Int CVAR_TickRateLimit("application.tickRateLimit", "enable tickrate limit", 1, CVarFlags::EditCheckbox);
AutoCVar_Int CVAR_TickRateLimitTarget("application.tickRateLimitTarget", "target tickrate while limited", 30);

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

	DebugHandler::Print("Application : Shutdown Initiated");
	Cleanup();
	DebugHandler::Print("Application : Shutdown Complete");

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
			f32 deltaTime = timer.GetDeltaTime();
			timer.Tick();

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
	// Create config folder if it doesn't exist
	std::filesystem::create_directories("Data/config");

	// Setup CVar Config
	{
		nlohmann::ordered_json fallback;
		if (!JsonUtils::LoadFromPathOrCreate(_cvarJson, fallback, "Data/config/CVar.json"))
		{
			return false;
		}

		JsonUtils::LoadJsonIntoCVars(_cvarJson);
		JsonUtils::LoadCVarsIntoJson(_cvarJson);
		JsonUtils::SaveToPath(_cvarJson, "Data/config/CVar.json");
	}

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
				DebugHandler::Print(message.data);
				break;
			}

			case MessageInbound::Type::Ping:
			{
				MessageOutbound pongMessage(MessageOutbound::Type::Pong);
				_messagesOutbound.enqueue(pongMessage);

				DebugHandler::Print("Main Thread -> Application Thread : Ping");
				break;
			}

			case MessageInbound::Type::DoString:
			{
				if (!Scripting::LuaUtil::DoString(message.data))
				{
					DebugHandler::PrintError("Failed to run Lua DoString");
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
		JsonUtils::LoadCVarsIntoJson(_cvarJson);
		JsonUtils::SaveToPath(_cvarJson, "Data/config/CVar.json");

		CVarSystem::Get()->ClearDirty();
	}

	return true;
}