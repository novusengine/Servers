#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Server/LuaEvent.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Spell
    {
        void OnSpellPrepare(Zenith* zenith, Generated::LuaSpellEventDataOnPrepare& data);
        void OnSpellHandleEffect(Zenith* zenith, Generated::LuaSpellEventDataOnHandleEffect& data);
    }
}