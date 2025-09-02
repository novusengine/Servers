#pragma once
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <Server-Common/Database/Definitions.h>

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Gameplay/GameDefine.h>
#include <Gameplay/Network/Define.h>

#include <Meta/Generated/Shared/UnitEnum.h>

#include <Network/Define.h>

#include <entt/fwd.hpp>

#include <functional>
#include <limits>
#include <memory>

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
        u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, u16 size = 0);
        bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos);
        bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, std::function<bool()>&& func = nullptr);
        
        namespace Unit
        {
            bool BuildUnitCreate(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid, const std::string& name);
        }

        namespace CombatLog
        {
            bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f32 damage);
            bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f32 healing);
            bool BuildResurrectedMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID);
        }

        namespace Cheat
        {
            bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, Generated::CheatCommandEnum command, u8 result, const std::string& response);

            bool BuildCheatCharacterAddResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
            bool BuildCheatCharacterRemoveResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
        }
    }
}
