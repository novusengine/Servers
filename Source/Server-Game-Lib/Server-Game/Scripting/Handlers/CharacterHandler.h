#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    class CharacterHandler : public LuaHandlerBase
    {
    private:
        void Register(Zenith* zenith);
        void Clear(Zenith* zenith) {}

        void PostLoad(Zenith* zenith);
        void Update(Zenith* zenith, f32 deltaTime) {}

    public: // Registered Functions

    };
}