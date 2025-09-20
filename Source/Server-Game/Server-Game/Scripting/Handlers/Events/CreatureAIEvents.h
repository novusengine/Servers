#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Server/LuaEvent.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::CreatureAIEvents
    {
        void OnCreatureAIInit(Zenith* zenith, Generated::LuaCreatureAIEventDataOnInit& data);
        void OnCreatureAIDeinit(Zenith* zenith, Generated::LuaCreatureAIEventDataOnDeinit& data);
        void OnCreatureAIOnEnterCombat(Zenith* zenith, Generated::LuaCreatureAIEventDataOnEnterCombat& data);
        void OnCreatureAIOnLeaveCombat(Zenith* zenith, Generated::LuaCreatureAIEventDataOnLeaveCombat& data);
        void OnCreatureAIUpdate(Zenith* zenith, Generated::LuaCreatureAIEventDataOnUpdate& data);
        void OnCreatureAIOnResurrect(Zenith* zenith, Generated::LuaCreatureAIEventDataOnResurrect& data);
        void OnCreatureAIOnDied(Zenith* zenith, Generated::LuaCreatureAIEventDataOnDied& data);
    }
}