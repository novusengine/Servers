#include "SpellHandler.h"
#include "Server-Game/Scripting/Game/Spell.h"
#include "Server-Game/Scripting/Game/SpellEffect.h"

#include <Meta/Generated/Shared/SpellEnum.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void SpellHandler::Register(Zenith* zenith)
    {
        Game::Spell::Register(zenith);
        Game::SpellEffect::Register(zenith);

        {
            zenith->CreateTable(Generated::SpellEffectTypeEnumMeta::EnumName.data());

            for (const auto& pair : Generated::SpellEffectTypeEnumMeta::EnumList)
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
