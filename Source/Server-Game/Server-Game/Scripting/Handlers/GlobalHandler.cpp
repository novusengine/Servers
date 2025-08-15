#include "GlobalHandler.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/Scripting/LuaState.h"
#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Scripting/Systems/LuaSystemBase.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <entt/entt.hpp>
#include <lualib.h>

namespace Scripting
{
    /*static LuaMethod globalMethods[] =
    {
    };*/

    void GlobalHandler::Register(lua_State* state)
    {
        LuaManager* luaManager = ServiceLocator::GetLuaManager();
        LuaState ctx(state);

        ctx.CreateTableAndPopulate("Engine", [&]()
        {
            ctx.SetTable("Name", "NovusEngine");
            ctx.SetTable("Version", vec3(0.0f, 0.0f, 1.0f));
        });

        //LuaMethodTable::Set(state, globalMethods);
    }
}
