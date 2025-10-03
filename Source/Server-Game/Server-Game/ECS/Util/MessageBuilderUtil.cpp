#include "MessageBuilderUtil.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/UnitVisualItems.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include <Meta/Generated/Shared/CombatLogEnum.h>
#include <Meta/Generated/Shared/NetworkPacket.h>

#include <entt/entt.hpp>

namespace ECS::Util::MessageBuilder
{
    u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, u16 size)
    {
        ::Network::MessageHeader header =
        {
            .opcode = opcode,
            .size = size
        };

        if (buffer->GetSpace() < sizeof(::Network::MessageHeader))
            return std::numeric_limits<u32>().max();

        u32 headerPos = static_cast<u32>(buffer->writtenData);
        buffer->Put(header);

        return headerPos;
    }

    bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos)
    {
        if (buffer->writtenData < headerPos + sizeof(::Network::MessageHeader))
            return false;

        auto* header = reinterpret_cast<::Network::MessageHeader*>(buffer->GetDataPointer() + headerPos);

        u32 headerSize = static_cast<u32>(buffer->writtenData - headerPos) - sizeof(::Network::MessageHeader);
        if (headerSize > std::numeric_limits<u16>().max())
            return false;

        header->size = headerSize;
        return true;
    }

    bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, std::function<void()>&& func)
    {
        if (!buffer)
            return false;

        u32 headerPos = AddHeader(buffer, opcode);

        func();

        if (!ValidatePacket(buffer, headerPos))
            return false;

        return true;
    }

    namespace Unit
    {
        bool BuildUnitAdd(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID guid, const std::string& name, GameDefine::UnitClass unitClass, const vec3& position, const vec3& scale, const vec2& pitchYaw)
        {
            bool result = Util::Network::AppendPacketToBuffer(buffer, Generated::UnitAddPacket{
                .guid = guid,
                .name = name,
                .unitClass = static_cast<u8>(unitClass),
                .position = position,
                .scale = scale,
                .pitchYaw = pitchYaw
            });

            return result;
        }
        bool BuildUnitBaseInfo(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid)
        {
            auto& unitAuraInfo = registry.get<Components::UnitAuraInfo>(entity);
            auto& unitPowersComponent = registry.get<Components::UnitPowersComponent>(entity);
            auto& displayInfo = registry.get<Components::DisplayInfo>(entity);
            auto& visualItems = registry.get<Components::UnitVisualItems>(entity);

            bool failed = false;

            failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::UnitDisplayInfoUpdatePacket{
                .guid = guid,
                .displayID = displayInfo.displayID,
                .race = static_cast<u8>(displayInfo.unitRace),
                .gender = static_cast<u8>(displayInfo.unitGender)
            });

            u32 numEquippedItems = static_cast<u32>(Generated::ItemEquipSlotEnum::EquipmentEnd) + 1;
            for (u32 i = 0; i < numEquippedItems; i++)
            {
                if (visualItems.equippedItemIDs[i] == 0)
                    continue;

                failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::ServerUnitVisualItemUpdatePacket{
                    .guid = guid,
                    .slot = static_cast<ItemEquipSlot_t>(i),
                    .itemID = visualItems.equippedItemIDs[i]
                });
            }

            for (auto& pair : unitPowersComponent.powerTypeToValue)
            {
                failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::UnitPowerUpdatePacket{
                    .guid = guid,
                    .kind = static_cast<u8>(pair.first),
                    .base = pair.second.base,
                    .current = pair.second.current,
                    .max = pair.second.max
                });
            }

            for (auto& pair : unitAuraInfo.spellIDToAuraEntity)
            {
                entt::entity entity = pair.second;

                auto& auraInfo = registry.get<Components::AuraInfo>(entity);

                failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::ServerUnitAddAuraPacket{
                    .guid = guid,
                    .auraInstanceID = entt::to_integral(entity),
                    .spellID = pair.first,
                    .duration = auraInfo.duration,
                    .stacks = auraInfo.stacks
                });
            }

            return !failed;
        }
    }

    namespace CombatLog
    {
        bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 damage, f64 overKillDamage)
        {
            bool result = CreatePacket(buffer, Generated::SendCombatEventPacket::PACKET_ID, [&, sourceGUID, targetGUID, damage, overKillDamage]()
            {
                buffer->Put(Generated::CombatLogEventsEnum::DamageDealt);
                buffer->Serialize(sourceGUID);
                buffer->Serialize(targetGUID);
                buffer->PutF64(damage);
                buffer->PutF64(overKillDamage);
            });

            return result;
        }
        bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 healing, f64 overHealing)
        {
            bool result = CreatePacket(buffer, Generated::SendCombatEventPacket::PACKET_ID, [&, sourceGUID, targetGUID, healing, overHealing]()
            {
                buffer->Put(Generated::CombatLogEventsEnum::HealingDone);
                buffer->Serialize(sourceGUID);
                buffer->Serialize(targetGUID);
                buffer->PutF64(healing);
                buffer->PutF64(overHealing);
            });

            return result;
        }
        bool BuildResurrectedMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 restoredHealth)
        {
            bool result = CreatePacket(buffer, Generated::SendCombatEventPacket::PACKET_ID, [&buffer, sourceGUID, targetGUID, restoredHealth]()
            {
                buffer->Put(Generated::CombatLogEventsEnum::Resurrected);
                buffer->Serialize(sourceGUID);
                buffer->Serialize(targetGUID);
                buffer->PutF64(restoredHealth);
            });

            return result;
        }
    }

    namespace Cheat
    {
        bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, Generated::CheatCommandEnum command, u8 result, const std::string& response)
        {
            bool createdPacket = CreatePacket(buffer, Generated::CheatCommandResultPacket::PACKET_ID, [&buffer, &response, command, result]()
            {
                buffer->Put(command);
                buffer->PutU8(result);
                buffer->PutString(response);
            });

            return createdPacket;
        }

        bool BuildCheatCharacterAddResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
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

            return BuildCheatCommandResultMessage(buffer, Generated::CheatCommandEnum::CharacterAdd, result, *resultString);
        }
        bool BuildCheatCharacterRemoveResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
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

            return BuildCheatCommandResultMessage(buffer, Generated::CheatCommandEnum::CharacterRemove, result, *resultString);
        }
    }
}