#include "MessageBuilderUtil.h"
#include "Server-Game/ECS/Components/CombatLog.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <entt/entt.hpp>

namespace ECS::Util::MessageBuilder
{
    u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, u16 size)
    {
        Network::PacketHeader header =
        {
            .opcode = static_cast<Network::OpcodeType>(opcode),
            .size = size
        };

        if (buffer->GetSpace() < sizeof(Network::PacketHeader))
            return std::numeric_limits<u32>().max();

        u32 headerPos = static_cast<u32>(buffer->writtenData);
        buffer->Put(header);

        return headerPos;
    }

    bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos)
    {
        if (buffer->writtenData < headerPos + sizeof(Network::PacketHeader))
            return false;

        Network::PacketHeader* header = reinterpret_cast<Network::PacketHeader*>(buffer->GetDataPointer() + headerPos);

        u32 headerSize = static_cast<u32>(buffer->writtenData - headerPos) - sizeof(Network::PacketHeader);
        if (headerSize > std::numeric_limits<u16>().max())
            return false;

        header->size = headerSize;
        return true;
    }

    bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, std::function<void()>&& func)
    {
        if (!buffer)
            return false;

        u32 headerPos = AddHeader(buffer, opcode);

        func();

        if (!ValidatePacket(buffer, headerPos))
            return false;

        return true;
    }

    namespace Authentication
    {
        bool BuildConnectedMessage(std::shared_ptr<Bytebuffer>& buffer, Network::ConnectResult result)
        {
            bool createPacketResult = CreatePacket(buffer, Network::GameOpcode::Server_Connected, [&buffer, result]()
            {
                buffer->Put(result);
            });

            return createPacketResult;
        }
    }

    namespace Entity
    {
        bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const vec3& position, const quat& rotation, const Components::MovementFlags movementFlags, f32 verticalVelocity)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Shared_EntityMove, [&buffer, &position, &rotation, entity, movementFlags, verticalVelocity]()
            {
                buffer->Put(entity);
                buffer->Put(position);
                buffer->Put(rotation);
                buffer->Put(movementFlags);
                buffer->PutF32(verticalVelocity);
            });

            return result;
        }
        bool BuildSetMoverMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SetMover, [&buffer, entity]()
            {
                buffer->Put(entity);
            });

            return result;
        }
        bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::Transform& transform)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityCreate, [&buffer, &transform, entity]()
            {
                buffer->Put(entity);
                buffer->Put(transform);
            });

            return result;
        }
        bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityDestroy, [&buffer, entity]()
            {
                buffer->Put(entity);
            });

            return result;
        }
        bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::UnitStatsComponent& unitStatsComponent)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityResourcesUpdate, [&buffer, &unitStatsComponent, entity]()
            {
                buffer->Put(entity);
                buffer->Put(Components::PowerType::Health);
                buffer->PutF32(unitStatsComponent.baseHealth);
                buffer->PutF32(unitStatsComponent.currentHealth);
                buffer->PutF32(unitStatsComponent.maxHealth);
            });

            if (!result)
                return false;

            for (i32 i = 0; i < (u32)Components::PowerType::Count; i++)
            {
                Components::PowerType powerType = (Components::PowerType)i;

                f32 powerValue = unitStatsComponent.currentPower[i];
                f32 basePower = unitStatsComponent.basePower[i];
                f32 maxPower = unitStatsComponent.maxPower[i];

                u32 headerPos = AddHeader(buffer, Network::GameOpcode::Server_EntityResourcesUpdate);

                buffer->Put(entity);
                buffer->Put(powerType);
                buffer->PutF32(basePower);
                buffer->PutF32(powerValue);
                buffer->PutF32(maxPower);

                if (!ValidatePacket(buffer, headerPos))
                    return false;
            }

            return true;
        }
        bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, entt::entity target)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Shared_EntityTargetUpdate, [&buffer, entity, target]()
            {
                buffer->Put(entity);
                buffer->Put(target);
            });

            return result;
        }
        bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, f32 castTime, f32 castDuration)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityCastSpell, [&buffer, entity, castTime, castDuration]()
            {
                buffer->Put(entity);
                buffer->PutF32(castTime);
                buffer->PutF32(castDuration);
            });

            return result;
        }
        bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u32 displayID)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityDisplayInfoUpdate, [&buffer, entity, displayID]()
            {
                buffer->Put(entity);
                buffer->PutU32(displayID);
            });

            return result;
        }
    }

    namespace Unit
    {
        bool BuildUnitCreate(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity)
        {
            Components::Transform& transform = registry.get<Components::Transform>(entity);
            Components::UnitStatsComponent& unitStatsComponent = registry.get<Components::UnitStatsComponent>(entity);
            Components::DisplayInfo& displayInfo = registry.get<Components::DisplayInfo>(entity);

            bool failed = false;
            failed |= !Entity::BuildEntityCreateMessage(buffer, entity, transform);
            failed |= !Entity::BuildEntityDisplayInfoUpdateMessage(buffer, entity, displayInfo.displayID);
            failed |= !Entity::BuildUnitStatsMessage(buffer, entity, unitStatsComponent);

            return !failed;
        }
    }

    namespace Spell
    {
        bool BuildSpellCastResultMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u8 result, f32 castTime, f32 castDuration)
        {
            bool createPacketResult = false;

            if (result == 0)
            {
                result = CreatePacket(buffer, Network::GameOpcode::Server_SendSpellCastResult, [&buffer, entity, result, castTime, castDuration]()
                {
                    buffer->PutU8(result);
                    buffer->PutF32(castTime);
                    buffer->PutF32(castDuration);
                });
            }
            else
            {
                static const std::string EmptyStr = "";
                static const std::string AlreadyCasting = "A spell is already being cast";
                static const std::string NotTargetAvailable = "You have no target to cast on";
                const std::string* errorString = &EmptyStr;

                switch (result)
                {
                    case 0x1:
                    {
                        errorString = &AlreadyCasting;
                        break;
                    }

                    case 0x2:
                    {
                        errorString = &NotTargetAvailable;
                        break;
                    }

                    default: break;
                }

                createPacketResult = CreatePacket(buffer, Network::GameOpcode::Server_SendSpellCastResult, [&buffer, &errorString, result]()
                {
                    buffer->PutU8(result);
                    buffer->PutString(*errorString);
                });
            }

            return createPacketResult;
        }
    }

    namespace CombatLog
    {
        bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 damage)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&]()
            {
                buffer->Put(Components::CombatLogEvents::DamageDealt);
                buffer->Put(source);
                buffer->Put(target);
                buffer->PutF32(damage);
            });

            return result;
        }
        bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 healing)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&buffer, source, target, healing]()
            {
                buffer->Put(Components::CombatLogEvents::HealingDone);
                buffer->Put(source);
                buffer->Put(target);
                buffer->PutF32(healing);
            });

            return result;
        }
        bool BuildRessurectedMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&buffer, source, target]()
            {
                buffer->Put(Components::CombatLogEvents::Ressurected);
                buffer->Put(source);
                buffer->Put(target);
            });

            return result;
        }
    }

    namespace Cheat
    {
        bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, const std::string& response)
        {
            bool createdPacket = CreatePacket(buffer, Network::GameOpcode::Server_SendCheatCommandResult, [&buffer, &response, result]()
            {
                buffer->PutU8(result);
                buffer->PutString(response);
            });

            return createdPacket;
        }

        bool BuildCheatCreateCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
        {
            static const std::string EmptyStr = "";
            static const std::string InvalidNameSupplied = "Character creation failed due to invalid name";
            static const std::string CharacterWasCreated = "Character was created";
            static const std::string CharacterAlreadyExists = "A character with the given name already exists";
            static const std::string DatabaseTransactionFailed = "A database error occured during character creation";

            const std::string* resultString = &EmptyStr;
            switch (result)
            {
                case 0x0:
                {
                    resultString = &CharacterWasCreated;
                    break;
                }

                case 0x1:
                {
                    resultString = &InvalidNameSupplied;
                    break;
                }

                case 0x2:
                {
                    resultString = &CharacterAlreadyExists;
                    break;
                }

                case 0x4:
                {
                    resultString = &DatabaseTransactionFailed;
                    break;
                }

                default: break;
            }

            return BuildCheatCommandResultMessage(buffer, result, *resultString);
        }
        bool BuildCheatDeleteCharacterResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
        {
            static const std::string EmptyStr = "";
            static const std::string CharacterWasDeleted = "Character was deleted";
            static const std::string CharacterDoesNotExists = "No character with the given name exists";
            static const std::string DatabaseTransactionFailed = "A database error occured during character deletion";
            static const std::string InsufficientPermission = "Could not delete character, insufficient permissions";

            const std::string* resultString = &EmptyStr;
            switch (result)
            {
                case 0x0:
                {
                    resultString = &CharacterWasDeleted;
                    break;
                }

                case 0x1:
                {
                    resultString = &CharacterDoesNotExists;
                    break;
                }

                case 0x2:
                {
                    resultString = &DatabaseTransactionFailed;
                    break;
                }

                case 0x4:
                {
                    resultString = &InsufficientPermission;
                    break;
                }

                default: break;
            }

            return BuildCheatCommandResultMessage(buffer, result, *resultString);
        }
    }
}