#include "AuraEvents.h"
#include "Server-Game/Scripting/Game/Unit.h"
#include "Server-Game/Scripting/Game/Aura.h"
#include "Server-Game/Scripting/Game/AuraEffect.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Event::Aura
    {
        void OnAuraApply(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnApply& data)
        {
            zenith->CreateTable();

            entt::entity casterEntity = static_cast<entt::entity>(data.casterID);
            if (casterEntity != entt::null)
            {
                auto* caster = Game::Unit::Create(zenith, casterEntity);
                zenith->SetTableKey("caster");
            }

            auto* target = Game::Unit::Create(zenith, static_cast<entt::entity>(data.targetID));
            zenith->SetTableKey("target");

            auto* aura = Game::Aura::Create(zenith, static_cast<entt::entity>(data.auraEntity), data.spellID);
            zenith->SetTableKey("aura");
        }
        void OnAuraRemove(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnRemove& data)
        {
            zenith->CreateTable();

            entt::entity casterEntity = static_cast<entt::entity>(data.casterID);
            if (casterEntity != entt::null)
            {
                auto* caster = Game::Unit::Create(zenith, casterEntity);
                zenith->SetTableKey("caster");
            }

            auto* target = Game::Unit::Create(zenith, static_cast<entt::entity>(data.targetID));
            zenith->SetTableKey("target");

            auto* aura = Game::Aura::Create(zenith, static_cast<entt::entity>(data.auraEntity), data.spellID);
            zenith->SetTableKey("aura");
        }
        void OnAuraHandleEffect(Zenith* zenith, MetaGen::Server::Lua::AuraEventDataOnHandleEffect& data)
        {
            zenith->CreateTable();

            entt::entity casterEntity = static_cast<entt::entity>(data.casterID);
            if (casterEntity != entt::null)
            {
                auto* caster = Game::Unit::Create(zenith, casterEntity);
                zenith->SetTableKey("caster");
            }

            auto* target = Game::Unit::Create(zenith, static_cast<entt::entity>(data.targetID));
            zenith->SetTableKey("target");

            auto* aura = Game::Aura::Create(zenith, static_cast<entt::entity>(data.auraEntity), data.spellID);
            zenith->SetTableKey("aura");

            auto* spellEffect = Game::AuraEffect::Create(zenith, static_cast<entt::entity>(data.auraEntity), data.effectIndex, data.effectType);
            zenith->SetField("effect");

            if (data.procID != 0)
            {
                zenith->AddTableField("procID", data.procID);
            }
        }
    }
}