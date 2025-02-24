#include "MessageBuilderUtil.h"
#include "Server-Game/ECS/Components/CombatLog.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <entt/entt.hpp>

namespace ECS::Util::MessageBuilder
{
    u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, Network::GameOpcode opcode, u16 size)
    {
        Network::MessageHeader header =
        {
            .opcode = static_cast<Network::OpcodeType>(opcode),
            .size = size
        };

        if (buffer->GetSpace() < sizeof(Network::MessageHeader))
            return std::numeric_limits<u32>().max();

        u32 headerPos = static_cast<u32>(buffer->writtenData);
        buffer->Put(header);

        return headerPos;
    }

    bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos)
    {
        if (buffer->writtenData < headerPos + sizeof(Network::MessageHeader))
            return false;

        Network::MessageHeader* header = reinterpret_cast<Network::MessageHeader*>(buffer->GetDataPointer() + headerPos);

        u32 headerSize = static_cast<u32>(buffer->writtenData - headerPos) - sizeof(Network::MessageHeader);
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

    namespace Heartbeat
    {
        bool BuildUpdateStatsMessage(std::shared_ptr<Bytebuffer>& buffer, u8 serverDiff)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_UpdateStats, [&buffer, serverDiff]()
            {
                buffer->PutU8(serverDiff);
            });

            return result;
        }
    }

    namespace Entity
    {
        bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const vec3& position, const quat& rotation, const Components::MovementFlags movementFlags, f32 verticalVelocity)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Shared_EntityMove, [&buffer, &position, &rotation, &guid, movementFlags, verticalVelocity]()
            {
                buffer->Serialize(guid);
                buffer->Put(position);
                buffer->Put(rotation);
                buffer->Put(movementFlags);
                buffer->PutF32(verticalVelocity);
            });

            return result;
        }
        bool BuildSetMoverMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SetMover, [&buffer, &guid]()
            {
                buffer->Serialize(guid);
            });

            return result;
        }
        bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid objectGuid, const Components::Transform& transform)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityCreate, [&buffer, &transform, &objectGuid]()
            {
                buffer->Serialize(objectGuid);
                buffer->Put(transform);
            });

            return result;
        }
        bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityDestroy, [&buffer, guid]()
            {
                buffer->Serialize(guid);
            });

            return result;
        }
        bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, Components::PowerType powerType, f32 base, f32 current, f32 max)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityResourcesUpdate, [&buffer, &guid, powerType, base, current, max]()
            {
                buffer->Serialize(guid);
                buffer->Put(powerType);
                buffer->PutF32(base);
                buffer->PutF32(current);
                buffer->PutF32(max);
            });

            return result;
        }
        bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, GameDefine::ObjectGuid targetGuid)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Shared_EntityTargetUpdate, [&buffer, &guid, &targetGuid]()
            {
                buffer->Serialize(guid);
                buffer->Serialize(targetGuid);
            });

            return result;
        }
        bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, f32 castTime, f32 castDuration)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityCastSpell, [&buffer, &guid, castTime, castDuration]()
            {
                buffer->Serialize(guid);
                buffer->PutF32(castTime);
                buffer->PutF32(castDuration);
            });

            return result;
        }
        bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const Components::DisplayInfo& displayInfo)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_EntityDisplayInfoUpdate, [&buffer, &guid, displayInfo]()
            {
                buffer->Serialize(guid);
                buffer->Put(displayInfo);
            });

            return result;
        }
    }

    namespace Unit
    {
        bool BuildUnitCreate(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, GameDefine::ObjectGuid guid)
        {
            Components::Transform& transform = registry.get<Components::Transform>(entity);
            Components::UnitStatsComponent& unitStatsComponent = registry.get<Components::UnitStatsComponent>(entity);
            Components::DisplayInfo& displayInfo = registry.get<Components::DisplayInfo>(entity);

            bool failed = false;
            failed |= !Entity::BuildEntityCreateMessage(buffer, guid, transform);
            failed |= !Entity::BuildEntityDisplayInfoUpdateMessage(buffer, guid, displayInfo);

            failed |= !Entity::BuildUnitStatsMessage(buffer, guid, Components::PowerType::Health, unitStatsComponent.baseHealth, unitStatsComponent.currentHealth, unitStatsComponent.maxHealth);

            for (i32 i = 0; i < (u32)Components::PowerType::Count; i++)
            {
                failed |= !Entity::BuildUnitStatsMessage(buffer, guid, (Components::PowerType)i, unitStatsComponent.basePower[i], unitStatsComponent.currentPower[i], unitStatsComponent.maxPower[i]);
            }

            return !failed;
        }
    }

    namespace Item
    {
        bool BuildItemCreate(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid guid, const Database::ItemInstance& itemInstance)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_ItemCreate, [&buffer, &itemInstance, guid]()
            {
                buffer->Serialize(guid);

                buffer->PutU32(itemInstance.itemID);
                buffer->PutU16(itemInstance.count);
                buffer->PutU16(itemInstance.durability);
            });

            return result;
        }
    }

    namespace Container
    {
        bool BuildContainerCreate(std::shared_ptr<Bytebuffer>& buffer, u8 containerIndex, u32 itemID, GameDefine::ObjectGuid guid, const Database::Container& container)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_ContainerCreate, [&buffer, &container, containerIndex, itemID, &guid]()
            {
                buffer->PutU8(containerIndex);
                buffer->PutU32(itemID);
                buffer->Serialize(guid);

                const auto& items = container.GetItems();
                u8 numSlots = container.GetTotalSlots();
                u8 numSlotsFree = container.GetFreeSlots();

                buffer->PutU8(numSlots);
                buffer->PutU8(numSlotsFree);

                for (u32 i = 0; i < numSlots; i++)
                {
                    const auto& item = items[i];
                    if (item.IsEmpty())
                        continue;

                    buffer->PutU8(i);
                    buffer->Serialize(item.objectGuid);
                }
            });

            return result;
        }

        bool BuildAddToSlot(std::shared_ptr<Bytebuffer>& buffer, u8 containerIndex, u8 slot, GameDefine::ObjectGuid itemGuid)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_ContainerAddToSlot, [&buffer, containerIndex, slot, itemGuid]()
            {
                buffer->PutU8(containerIndex);
                buffer->PutU8(slot);
                buffer->Serialize(itemGuid);
            });

            return result;
        }

        bool BuildRemoveFromSlot(std::shared_ptr<Bytebuffer>& buffer, u8 containerIndex, u8 slot)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_ContainerRemoveFromSlot, [&buffer, containerIndex, slot]()
            {
                buffer->PutU8(containerIndex);
                buffer->PutU8(slot);
            });

            return result;
        }

        bool BuildSwapSlots(std::shared_ptr<Bytebuffer>& buffer, u8 srcContainerIndex, u8 destContainerIndex, u8 srcSlot, u8 destSlot)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_ContainerSwapSlots, [&buffer, srcContainerIndex, destContainerIndex, srcSlot, destSlot]()
            {
                buffer->PutU8(srcContainerIndex);
                buffer->PutU8(destContainerIndex);
                buffer->PutU8(srcSlot);
                buffer->PutU8(destSlot);
            });

            return result;
        }
    }

    namespace Spell
    {
        bool BuildSpellCastResultMessage(std::shared_ptr<Bytebuffer>& buffer, u8 result, f32 castTime, f32 castDuration)
        {
            bool createPacketResult = false;

            if (result == 0)
            {
                createPacketResult = CreatePacket(buffer, Network::GameOpcode::Server_SendSpellCastResult, [&buffer, result, castTime, castDuration]()
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
        bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid, f32 damage)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&]()
            {
                buffer->Put(Components::CombatLogEvents::DamageDealt);
                buffer->Serialize(sourceGuid);
                buffer->Serialize(targetGuid);
                buffer->PutF32(damage);
            });

            return result;
        }
        bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid, f32 healing)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&buffer, &sourceGuid, &targetGuid, healing]()
            {
                buffer->Put(Components::CombatLogEvents::HealingDone);
                buffer->Serialize(sourceGuid);
                buffer->Serialize(targetGuid);
                buffer->PutF32(healing);
            });

            return result;
        }
        bool BuildRessurectedMessage(std::shared_ptr<Bytebuffer>& buffer, GameDefine::ObjectGuid sourceGuid, GameDefine::ObjectGuid targetGuid)
        {
            bool result = CreatePacket(buffer, Network::GameOpcode::Server_SendCombatEvent, [&buffer, &sourceGuid, &targetGuid]()
            {
                buffer->Put(Components::CombatLogEvents::Ressurected);
                buffer->Serialize(sourceGuid);
                buffer->Serialize(targetGuid);
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
