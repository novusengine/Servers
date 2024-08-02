#pragma once
#include "Server-Game/ECS/Components/Transform.h"

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

#include "Gameplay/Network/Define.h"
#include "Gameplay/Network/Opcode.h"

#include <entt/fwd.hpp>

#include <functional>
#include <limits>
#include <memory>

namespace ECS
{
    namespace Components
    {
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

        namespace Entity
        {
            bool BuildSetMoverMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity);
            bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::Transform& transform);
            bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity);
            bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const vec3& position, const quat& rotation, const Components::MovementFlags movementFlags, f32 verticalVelocity);
            bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::UnitStatsComponent& unitStatsComponent);
            bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, entt::entity target);
            bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, f32 castTime, f32 castDuration);
            bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u32 displayID);
        }

        namespace Unit
        {
            bool BuildUnitCreate(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity);
        }

        namespace Spell
        {
            bool BuildSpellCastResultMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u8 result, f32 castTime, f32 castDuration);
        }

        namespace CombatLog
        {
            bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 damage);
            bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 healing);
            bool BuildRessurectedMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target);
        }

        namespace Cheat
        {
            bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, const std::string& response);

            bool BuildCheatCreateCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
            bool BuildCheatDeleteCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result);
        }
    }
}
