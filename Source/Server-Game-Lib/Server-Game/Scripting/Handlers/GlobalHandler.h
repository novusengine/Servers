#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

namespace Scripting
{
    class GlobalHandler : public LuaHandlerBase
    {
    private:
        void Register(Zenith* zenith);
        void Clear(Zenith* zenith) {}

        void PostLoad(Zenith* zenith);
        void Update(Zenith* zenith, f32 deltaTime) {}

    public: // Registered Functions
        static i32 GetStateMapID(Zenith* zenith);
        static i32 GetStateInstanceID(Zenith* zenith);
        static i32 GetStateVariantID(Zenith* zenith);
        static i32 GetStateIDs(Zenith* zenith);
        static i32 IsStateGlobal(Zenith* zenith);
        static i32 RegisterCreatureAIScript(Zenith* zenith);
    };

    static LuaRegister<> GlobalHandlerGlobalMethods[] =
    {
        { "GetStateMapID", GlobalHandler::GetStateMapID },
        { "GetStateInstanceID", GlobalHandler::GetStateInstanceID },
        { "GetStateVariantID", GlobalHandler::GetStateVariantID },
        { "GetStateIDs", GlobalHandler::GetStateIDs },
        { "IsStateGlobal", GlobalHandler::IsStateGlobal },

        { "RegisterCreatureAIScript", GlobalHandler::RegisterCreatureAIScript }
    };
}