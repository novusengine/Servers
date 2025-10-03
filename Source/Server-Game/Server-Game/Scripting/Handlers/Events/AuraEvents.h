#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Server/LuaEvent.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Aura
    {
        void OnAuraApply(Zenith* zenith, Generated::LuaAuraEventDataOnApply& data);
        void OnAuraRemove(Zenith* zenith, Generated::LuaAuraEventDataOnRemove& data);
        void OnAuraHandleEffect(Zenith* zenith, Generated::LuaAuraEventDataOnHandleEffect& data);
    }
}