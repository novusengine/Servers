#pragma once
#include <cassert>

namespace enki
{
    class TaskScheduler;
}

struct EnttRegistries;

namespace Scripting
{
    class LuaManager;
}

class ServiceLocator
{
public:
    static enki::TaskScheduler* GetTaskScheduler()
    {
        assert(_taskScheduler != nullptr);
        return _taskScheduler;
    }
    static void SetTaskScheduler(enki::TaskScheduler* taskScheduler);

    static EnttRegistries* GetEnttRegistries()
    {
        assert(_enttRegistries != nullptr);
        return _enttRegistries;
    }
    static void SetEnttRegistries(EnttRegistries* enttRegistries);

    static Scripting::LuaManager* GetLuaManager()
    {
        assert(_luaManager != nullptr);
        return _luaManager;
    }
    static void SetLuaManager(Scripting::LuaManager* luaManager);

private:
    ServiceLocator() { }
    static Scripting::LuaManager* _luaManager;
    static enki::TaskScheduler* _taskScheduler;
    static EnttRegistries* _enttRegistries;
};