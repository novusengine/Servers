#pragma once
#include <Base/Types.h>

#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::CreatureAIEvents
    {
        void OnCreatureAIInit(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnInit& data);
        void OnCreatureAIDeinit(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnDeinit& data);
        void OnCreatureAIOnEnterCombat(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnEnterCombat& data);
        void OnCreatureAIOnLeaveCombat(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnLeaveCombat& data);
        void OnCreatureAIUpdate(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnUpdate& data);
        void OnCreatureAIOnResurrect(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnResurrect& data);
        void OnCreatureAIOnDied(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnDied& data);
    }
}