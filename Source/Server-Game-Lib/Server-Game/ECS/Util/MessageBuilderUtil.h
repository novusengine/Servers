#pragma once
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <Server-Common/Database/Definitions.h>

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Gameplay/GameDefine.h>
#include <Gameplay/Network/Define.h>

#include <MetaGen/Shared/Cheat/Cheat.h>

#include <Network/Define.h>

#include <entt/fwd.hpp>

#include <functional>
#include <limits>
#include <memory>

namespace Gameplay::Faction
{
    struct FactionRuntimeData;
}

namespace ECS
{
    namespace Components
    {
        struct DisplayInfo;
        struct UnitStatsComponent;
        struct Transform;
    }

    namespace Util::MessageBuilder
    {
        u32 AddHeader(Bytebuffer& buffer, ::Network::OpcodeType opcode, u16 size = 0);
        bool ValidatePacket(Bytebuffer& buffer, u32 headerPos);
        bool CreatePacket(Bytebuffer& buffer, ::Network::OpcodeType opcode, std::function<bool()>&& func = nullptr);
        u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, u16 size = 0);
        bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos);
        bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, std::function<bool()>&& func = nullptr);

        namespace Unit
        {
            bool BuildUnitAdd(Bytebuffer& buffer, ObjectGUID guid, const std::string& name, GameDefine::UnitClass unitClass, const vec3& position, const vec3& scale, const vec2& pitchYaw);
            bool BuildUnitBaseInfo(Bytebuffer& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid, const ::Gameplay::Faction::FactionRuntimeData& factionRuntime);
            bool BuildUnitAdd(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID guid, const std::string& name, GameDefine::UnitClass unitClass, const vec3& position, const vec3& scale, const vec2& pitchYaw);
            bool BuildUnitBaseInfo(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid, const ::Gameplay::Faction::FactionRuntimeData& factionRuntime);
        }

        namespace CombatLog
        {
            bool BuildDamageDealtMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 damage, f64 overKillDamage);
            bool BuildHealingDoneMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 healing, f64 overHealing);
            bool BuildResurrectedMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 restoredHealth);
            bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 damage, f64 overKillDamage);
            bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 healing, f64 overHealing);
            bool BuildResurrectedMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 restoredHealth);
        }

        namespace Cheat
        {
            bool BuildCheatCommandResultMessage(Bytebuffer& buffer, MetaGen::Shared::Cheat::CheatCommandEnum command, u8 result, const std::string& response);
            bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, MetaGen::Shared::Cheat::CheatCommandEnum command, u8 result, const std::string& response);

            bool BuildCheatCharacterAddResponse(Bytebuffer& buffer, u8 result);
            bool BuildCheatCharacterRemoveResponse(Bytebuffer& buffer, u8 result);
            bool BuildCheatCharacterAddResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
            bool BuildCheatCharacterRemoveResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
        }
    }
}
