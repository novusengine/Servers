#pragma once
#include "Server-Game/ECS/Components/Transform.h"

#include <Base/Types.h>
#include <Base/Memory/Bytebuffer.h>

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
        bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, std::function<bool()> func);

        namespace Authentication
        {
            bool BuildConnectedMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, entt::entity entity, const std::string* resultString = nullptr);
        }

        namespace Entity
        {
            bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::Transform& transform, u32 displayID);
            bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity);
            bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const vec3& position, const quat& rotation, const Components::MovementFlags& movementFlags, f32 verticalVelocity);
            bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::UnitStatsComponent& unitStatsComponent);
            bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, entt::entity target);
            bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, f32 castTime, f32 castDuration);
            bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u32 displayID);
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
            bool BuildCharacterCreateResultMessage(std::shared_ptr<Bytebuffer>& buffer, const std::string& name, u8 result);
            bool BuildCharacterDeleteResultMessage(std::shared_ptr<Bytebuffer>& buffer, const std::string& name, u8 result);
        }
    }
}
