#include "CreatureAIEvents.h"
#include "Server-Game/Scripting/Game/Unit.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Event::CreatureAIEvents
    {
        void OnCreatureAIInit(Zenith* zenith, Generated::LuaCreatureAIEventDataOnInit& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            zenith->AddTableField("scriptNameHash", data.scriptNameHash);
        }
        void OnCreatureAIDeinit(Zenith* zenith, Generated::LuaCreatureAIEventDataOnDeinit& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIOnEnterCombat(Zenith* zenith, Generated::LuaCreatureAIEventDataOnEnterCombat& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIOnLeaveCombat(Zenith* zenith, Generated::LuaCreatureAIEventDataOnLeaveCombat& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIUpdate(Zenith* zenith, Generated::LuaCreatureAIEventDataOnUpdate& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            zenith->AddTableField("deltaTime", data.deltaTime);
        }
        void OnCreatureAIOnResurrect(Zenith* zenith, Generated::LuaCreatureAIEventDataOnResurrect& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            entt::entity resurrectorEntity = static_cast<entt::entity>(data.resurrectorEntity);
            if (resurrectorEntity != entt::null)
            {
                auto* resurrector = Game::Unit::Create(zenith, resurrectorEntity);
                zenith->SetTableKey("resurrector");
            }
        }
        void OnCreatureAIOnDied(Zenith* zenith, Generated::LuaCreatureAIEventDataOnDied& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            entt::entity killerEntity = static_cast<entt::entity>(data.killerEntity);
            if (killerEntity != entt::null)
            {
                auto* killer = Game::Unit::Create(zenith, killerEntity);
                zenith->SetTableKey("killer");
            }
        }
    }
}