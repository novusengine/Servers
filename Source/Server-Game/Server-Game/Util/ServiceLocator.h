#pragma once
#include <cassert>

namespace enki
{
    class TaskScheduler;
}

struct EnttRegistries;
class GameConsole;

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

private:
    ServiceLocator() { }
    static enki::TaskScheduler* _taskScheduler;
    static EnttRegistries* _enttRegistries;
};