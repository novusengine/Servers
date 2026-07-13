#pragma once
#include <Base/Types.h>

#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Aura
    {
        void OnAuraApply(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnApply& data);
        void OnAuraRemove(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnRemove& data);
        void OnAuraHandleEffect(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnHandleEffect& data);
    }
}