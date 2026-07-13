#include "Itemhandler.h"
#include "Server-Game/Scripting/Game/Item.h"
#include "Server-Game/Scripting/Game/ItemTemplate.h"
#include "Server-Game/Scripting/Game/ItemWeaponTemplate.h"

#include <MetaGen/Shared/Unit/Unit.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void ItemHandler::Register(Zenith* zenith)
    {
        Game::Item::Register(zenith);
        Game::ItemTemplate::Register(zenith);
        Game::ItemWeaponTemplate::Register(zenith);

        {
            zenith->CreateTable(MetaGen::Shared::Unit::ItemEquipSlotEnumMeta::ENUM_NAME.data());

            for (const auto& pair : MetaGen::Shared::Unit::ItemEquipSlotEnumMeta::ENUM_FIELD_LIST)
            {
                zenith->AddTableField(pair.first.data(), pair.second);
            }

            zenith->Pop();
        }
    }

    void ItemHandler::PostLoad(Zenith* zenith)
    {
    }
}
