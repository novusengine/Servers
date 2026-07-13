#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

namespace Scripting
{
    class CreatureHandler : public LuaHandlerBase
    {
    private:
        void Register(Zenith* zenith);
        void Clear(Zenith* zenith);

        void PostLoad(Zenith* zenith);
        void Update(Zenith* zenith, f32 deltaTime) {}

    public: // Registered Functions
        static i32 RegisterCreatureAIScript(Zenith* zenith);
    };

    static LuaRegister<> CreatureHandlersGlobalMethods[] =
    {
        { "RegisterCreatureAIScript", CreatureHandler::RegisterCreatureAIScript }
    };
}