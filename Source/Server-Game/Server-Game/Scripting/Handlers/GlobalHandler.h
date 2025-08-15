#pragma once
#include "LuaHandlerBase.h"
#include "Server-Game/Scripting/LuaDefines.h"
#include "Server-Game/Scripting/LuaMethodTable.h"

#include <Base/Types.h>

namespace Scripting
{
    class GlobalHandler : public LuaHandlerBase
    {
    private:
        void Register(lua_State* state);
        void Clear() { }

    public: // Registered Functions
    };
}