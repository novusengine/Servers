#include "LuaSystemBase.h"
#include "Server-Game/Scripting/LuaDefines.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>

#include <lualib.h>

#include <robinhood/robinhood.h>

namespace Scripting
{
    LuaSystemBase::LuaSystemBase() : _events()
    {
    }

    void LuaSystemBase::Init()
    {
    }

    void LuaSystemBase::Update(f32 deltaTime, lua_State* state)
    {
        LuaSystemEvent systemEvent;
        while (_events.try_dequeue(systemEvent))
        {
            switch (systemEvent)
            {
                case LuaSystemEvent::Reload:
                {
                    break;
                }

                default: break;
            }
        }
    }

    void LuaSystemBase::PushEvent(LuaSystemEvent systemEvent)
    {
        _events.enqueue(systemEvent);
    }
}