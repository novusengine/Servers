#include "Itemhandler.h"
#include "Server-Game/Scripting/Game/Item.h"
#include "Server-Game/Scripting/Game/ItemTemplate.h"
#include "Server-Game/Scripting/Game/ItemWeaponTemplate.h"

#include <Meta/Generated/Shared/UnitEnum.h>

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
            zenith->CreateTable(Generated::ItemEquipSlotEnumMeta::EnumName.data());

            for (const auto& pair : Generated::ItemEquipSlotEnumMeta::EnumList)
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
