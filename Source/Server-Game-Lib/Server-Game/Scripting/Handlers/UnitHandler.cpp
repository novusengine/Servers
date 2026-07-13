#include "UnitHandler.h"
#include "Server-Game/Scripting/Game/Unit.h"

#include <MetaGen/Shared/Unit/Unit.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void UnitHandler::Register(Zenith* zenith)
    {
        Game::Unit::Register(zenith);

        // PowerType Enum
        {
            zenith->CreateTable(MetaGen::Shared::Unit::PowerTypeEnumMeta::ENUM_NAME.data());

            for (const auto& pair : MetaGen::Shared::Unit::PowerTypeEnumMeta::ENUM_FIELD_LIST)
            {
                zenith->AddTableField(pair.first.data(), pair.second);
            }

            zenith->Pop();
        }

        // StatType Enum
        {
            zenith->CreateTable(MetaGen::Shared::Unit::StatTypeEnumMeta::ENUM_NAME.data());

            for (const auto& pair : MetaGen::Shared::Unit::StatTypeEnumMeta::ENUM_FIELD_LIST)
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
