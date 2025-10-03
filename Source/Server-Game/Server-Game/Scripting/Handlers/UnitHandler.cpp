#include "UnitHandler.h"
#include "Server-Game/Scripting/Game/Unit.h"

#include <Meta/Generated/Shared/UnitEnum.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void UnitHandler::Register(Zenith* zenith)
    {
        Game::Unit::Register(zenith);

        // PowerType Enum
        {
            zenith->CreateTable(Generated::PowerTypeEnumMeta::EnumName.data());

            for (const auto& pair : Generated::PowerTypeEnumMeta::EnumList)
            {
                zenith->AddTableField(pair.first.data(), pair.second);
            }

            zenith->Pop();
        }

        // StatType Enum
        {
            zenith->CreateTable(Generated::StatTypeEnumMeta::EnumName.data());

            for (const auto& pair : Generated::StatTypeEnumMeta::EnumList)
            {
                zenith->AddTableField(pair.first.data(), pair.second);
            }

            zenith->Pop();
        }
    }

    void UnitHandler::PostLoad(Zenith* zenith)
    {
    }
}
