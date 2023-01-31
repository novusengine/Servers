#include "ServiceLocator.h"

enki::TaskScheduler* ServiceLocator::_taskScheduler = nullptr;
EnttRegistries* ServiceLocator::_enttRegistries = nullptr;

void ServiceLocator::SetTaskScheduler(enki::TaskScheduler* taskScheduler)
{
    assert(_taskScheduler == nullptr);
    _taskScheduler = taskScheduler;
}

void ServiceLocator::SetEnttRegistries(EnttRegistries* enttRegistries)
{
    assert(_enttRegistries == nullptr);
    _enttRegistries = enttRegistries;
}