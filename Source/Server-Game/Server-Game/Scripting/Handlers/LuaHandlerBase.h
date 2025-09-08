#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    class LuaHandlerBase
    {
    public:
        virtual void Register(Zenith* zenith) = 0;
        virtual void Clear(Zenith* zenith) = 0;

        virtual void PostLoad(Zenith* zenith) = 0;
        virtual void Update(Zenith* zenith, f32 deltaTime) = 0;
    };
}