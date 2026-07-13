#include "SpellEvents.h"
#include "Server-Game/Scripting/Game/Unit.h"
#include "Server-Game/Scripting/Game/Spell.h"
#include "Server-Game/Scripting/Game/SpellEffect.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    namespace Event::Spell
    {
        void OnSpellPrepare(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnPrepare& data)
        {
            zenith->CreateTable();

            auto* caster = Game::Unit::Create(zenith, static_cast<entt::entity>(data.casterID));
            zenith->SetTableKey("caster");

            auto* spell = Game::Spell::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.spellID);
            zenith->SetTableKey("spell");
        }
        void OnSpellHandleEffect(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnHandleEffect& data)
        {
            zenith->CreateTable();

            auto* caster = Game::Unit::Create(zenith, static_cast<entt::entity>(data.casterID));
            zenith->SetField("caster");

            auto* spell = Game::Spell::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.spellID);
            zenith->SetField("spell");

            auto* spellEffect = Game::SpellEffect::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.effectIndex, data.effectType);
            zenith->SetField("effect");

            if (data.procID != 0)
            {
                zenith->AddTableField("procID", data.procID);
            }
        }
        void OnSpellFinish(Zenith* zenith, MetaGen::Server::Lua::SpellEventDataOnFinish& data)
        {
            zenith->CreateTable();

            auto* caster = Game::Unit::Create(zenith, static_cast<entt::entity>(data.casterID));
            zenith->SetTableKey("caster");

            auto* spell = Game::Spell::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.spellID);
            zenith->SetTableKey("spell");
        }
    }
}