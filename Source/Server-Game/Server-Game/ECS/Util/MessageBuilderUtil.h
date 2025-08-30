#pragma once
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <Server-Common/Database/Definitions.h>

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include <Gameplay/GameDefine.h>
#include <Gameplay/Network/Define.h>
#include <Gameplay/Network/Opcode.h>

#include <Meta/Generated/Game/ProximityTriggerEnum.h>

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
        u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, u16 size = 0);
        bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos);
        bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, std::function<bool()>&& func = nullptr);

        namespace Authentication
        {
            bool BuildConnectedMessage(std::shared_ptr<Bytebuffer>& buffer, Network::ConnectResult result);
        }

        namespace Heartbeat
        {
            bool BuildUpdateStatsMessage(std::shared_ptr<Bytebuffer>& buffer, u8 serverDiff);
        }

        namespace Entity
        {
            bool BuildSetMoverMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid);
            bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid objectGuid, const Components::Transform& transform);
            bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid);
            bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const vec3& position, const quat& rotation, const Components::MovementFlags movementFlags, f32 verticalVelocity);
            bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, Components::PowerType powerType, f32 base, f32 current, f32 max);
            bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, GameDefine::ObjectGuid targetGuid);
            bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, f32 castTime, f32 castDuration);
            bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const Components::DisplayInfo& displayInfo);
        }

        namespace Unit
        {
            bool BuildUnitCreate(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, GameDefine::ObjectGuid guid);
        }

        namespace Item
        {
            bool BuildItemCreate(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const Database::ItemInstance& itemInstance);
        }

        namespace Container
        {
            bool BuildContainerCreate(std::shared_ptr<Bytebuffer>& buffer, u16 containerIndex, u32 itemID, GameDefine::ObjectGuid guid, const Database::Container& container);
            bool BuildAddToSlot(std::shared_ptr<Bytebuffer>& buffer, u16 containerIndex, u16 slot, GameDefine::ObjectGuid itemGuid);
            bool BuildRemoveFromSlot(std::shared_ptr<Bytebuffer>& buffer, u16 containerIndex, u16 slot);
            bool BuildSwapSlots(std::shared_ptr<Bytebuffer>& buffer, u16 srcContainerIndex, u16 destContainerIndex, u16 srcSlot, u16 destSlot);
        }

        namespace Spell
        {
            bool BuildSpellCastResultMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, f32 castTime, f32 castDuration);
        }

        namespace CombatLog
        {
            bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid, f32 damage);
            bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid, f32 healing);
            bool BuildRessurectedMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid);
        }

        namespace ProximityTrigger
        {
            bool BuildProximityTriggerCreate(std::shared_ptr<Bytebuffer>& buffer, u32 triggerID, const std::string& name, Generated::ProximityTriggerFlagEnum flags, u16 mapID, const vec3& position, const vec3& extents);
            bool BuildProximityTriggerDelete(std::shared_ptr<Bytebuffer>& buffer, u32 triggerID);
        }

        namespace Cheat
        {
            bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, const std::string& response);

            bool BuildCheatCreateCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
            bool BuildCheatDeleteCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
        }
    }
}
