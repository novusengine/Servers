#pragma once
#include <Base/Types.h>

#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Spell
    {
        void OnSpellPrepare(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnPrepare& data);
        void OnSpellHandleEffect(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnHandleEffect& data);
        void OnSpellFinish(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnFinish& data);
    }
}