#include "CharacterEvents.h"
#include "Server-Game/Scripting/Game/Unit.h"
#include "Server-Game/Scripting/Game/Spell.h"
#include "Server-Game/Scripting/Game/SpellEffect.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    namespace Event::Spell
    {
        void OnSpellPrepare(Zenith* zenith, Generated::LuaSpellEventDataOnPrepare& data)
        {
            zenith->CreateTable();

            auto* caster = Game::Unit::Create(zenith, static_cast<entt::entity>(data.casterID));
            zenith->SetTableKey("caster");

            auto* spell = Game::Spell::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.spellID);
            zenith->SetTableKey("spell");
        }
        void OnSpellHandleEffect(Zenith* zenith, Generated::LuaSpellEventDataOnHandleEffect& data)
        {
            zenith->CreateTable();

            auto* caster = Game::Unit::Create(zenith, static_cast<entt::entity>(data.casterID));
            zenith->SetField("caster");

            auto* spell = Game::Spell::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.spellID);
            zenith->SetField("spell");

            auto* spellEffect = Game::SpellEffect::Create(zenith, static_cast<entt::entity>(data.spellEntity), data.effectIndex, data.effectType);
            zenith->SetField("effect");
        }
    }
}