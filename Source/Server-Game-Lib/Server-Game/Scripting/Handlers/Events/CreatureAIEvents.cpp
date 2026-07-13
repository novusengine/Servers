#include "CreatureAIEvents.h"
#include "Server-Game/Scripting/Game/Unit.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Event::CreatureAIEvents
    {
        void OnCreatureAIInit(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnInit& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            zenith->AddTableField("scriptNameHash", data.scriptNameHash);
        }
        void OnCreatureAIDeinit(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnDeinit& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIOnEnterCombat(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnEnterCombat& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIOnLeaveCombat(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnLeaveCombat& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");
        }
        void OnCreatureAIUpdate(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnUpdate& data)
        {
            zenith->CreateTable();

            auto* unit = Game::Unit::Create(zenith, static_cast<entt::entity>(data.creatureEntity));
            zenith->SetTableKey("unit");

            zenith->AddTableField("deltaTime", data.deltaTime);
        }
        void OnCreatureAIOnResurrect(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnResurrect& data)
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
        void OnCreatureAIOnDied(Zenith* zenith, MetaGen::Server::Lua::CreatureAIEventDataOnDied& data)
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