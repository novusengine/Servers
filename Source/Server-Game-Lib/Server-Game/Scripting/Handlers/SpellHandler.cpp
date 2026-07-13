#include "SpellHandler.h"
#include "Server-Game/Scripting/Game/Spell.h"
#include "Server-Game/Scripting/Game/SpellEffect.h"
#include "Server-Game/Scripting/Game/Aura.h"
#include "Server-Game/Scripting/Game/AuraEffect.h"

#include <MetaGen/Shared/Spell/Spell.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void SpellHandler::Register(Zenith* zenith)
    {
        Game::Spell::Register(zenith);
        Game::SpellEffect::Register(zenith);

        Game::Aura::Register(zenith);
        Game::AuraEffect::Register(zenith);

        {
            zenith->CreateTable(MetaGen::Shared::Spell::SpellEffectTypeEnumMeta::ENUM_NAME.data());

            for (const auto& pair : MetaGen::Shared::Spell::SpellEffectTypeEnumMeta::ENUM_FIELD_LIST)
            {
                zenith->AddTableField(pair.first.data(), pair.second);
            }

            zenith->Pop();
        }
    }

    void SpellHandler::PostLoad(Zenith* zenith)
    {
    }
}
