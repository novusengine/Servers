#include "GenericSystem.h"
#include "Server-Game/Scripting/LuaDefines.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>

namespace Scripting
{
    GenericSystem::GenericSystem() : LuaSystemBase() { }

    void GenericSystem::Prepare(f32 deltaTime, lua_State* state)
    {
    }

    void GenericSystem::Run(f32 deltaTime, lua_State* state)
    {
        LuaState ctx(state);

        // TODO :: Do Stuff
    }
}