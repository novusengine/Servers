#include "UnitHandler.h"
#include "Server-Game/Scripting/Game/Unit.h"
#include "Server-Game/Gameplay/Faction/CreatureFactionPolicyDefines.h"

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

        {
            using Gameplay::Faction::CreatureAggressionPolicy;
            zenith->CreateTable("CreatureAggressionPolicy");
            zenith->AddTableField("Passive", static_cast<u8>(CreatureAggressionPolicy::Passive));
            zenith->AddTableField("Defensive", static_cast<u8>(CreatureAggressionPolicy::Defensive));
            zenith->AddTableField("Aggressive", static_cast<u8>(CreatureAggressionPolicy::Aggressive));
            zenith->Pop();
        }

        {
            using Gameplay::Faction::CreatureAssistancePolicy;
            zenith->CreateTable("CreatureAssistancePolicy");
            zenith->AddTableField("None", static_cast<u8>(CreatureAssistancePolicy::None));
            zenith->AddTableField("SameFaction", static_cast<u8>(CreatureAssistancePolicy::SameFaction));
            zenith->AddTableField("Friendly", static_cast<u8>(CreatureAssistancePolicy::Friendly));
            zenith->Pop();
        }
    }

    void UnitHandler::PostLoad(Zenith* zenith)
    {
    }
}
