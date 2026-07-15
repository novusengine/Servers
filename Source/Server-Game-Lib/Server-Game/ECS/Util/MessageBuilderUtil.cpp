#include "MessageBuilderUtil.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/UnitFaction.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/UnitVisualItems.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/Gameplay/Faction/FactionRuntimeData.h"

#include <Gameplay/ECS/Components/ObjectFields.h>
#include <Gameplay/ECS/Components/UnitFields.h>

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Shared/CombatLog/CombatLog.h>
#include <MetaGen/Shared/Packet/Packet.h>

#include <entt/entt.hpp>

namespace ECS::Util::MessageBuilder
{
    u32 AddHeader(Bytebuffer& buffer, ::Network::OpcodeType opcode, u16 size)
    {
        ::Network::MessageHeader header = {
            .opcode = opcode,
            .size = size
        };

        if (buffer.GetSpace() < sizeof(::Network::MessageHeader))
            return std::numeric_limits<u32>().max();

        u32 headerPos = static_cast<u32>(buffer.writtenData);
        buffer.Put(header);

        return headerPos;
    }

    bool ValidatePacket(Bytebuffer& buffer, u32 headerPos)
    {
        if (buffer.writtenData < headerPos + sizeof(::Network::MessageHeader))
            return false;

        auto* header = reinterpret_cast<::Network::MessageHeader*>(buffer.GetDataPointer() + headerPos);

        u32 headerSize = static_cast<u32>(buffer.writtenData - headerPos) - sizeof(::Network::MessageHeader);
        if (headerSize > std::numeric_limits<u16>().max())
            return false;

        header->size = headerSize;
        return true;
    }

    bool CreatePacket(Bytebuffer& buffer, ::Network::OpcodeType opcode, std::function<bool()>&& func)
    {
        const u32 headerPos = AddHeader(buffer, opcode);
        if (headerPos == std::numeric_limits<u32>().max())
            return false;

        if (func && !func())
            return false;

        if (!ValidatePacket(buffer, headerPos))
            return false;

        return true;
    }

    u32 AddHeader(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, u16 size)
    {
        return buffer != nullptr ? AddHeader(*buffer, opcode, size) : std::numeric_limits<u32>().max();
    }

    bool ValidatePacket(const std::shared_ptr<Bytebuffer>& buffer, u32 headerPos)
    {
        return buffer != nullptr && ValidatePacket(*buffer, headerPos);
    }

    bool CreatePacket(std::shared_ptr<Bytebuffer>& buffer, ::Network::OpcodeType opcode, std::function<bool()>&& func)
    {
        return buffer != nullptr && CreatePacket(*buffer, opcode, std::move(func));
    }

    namespace Unit
    {
        bool BuildUnitAdd(Bytebuffer& buffer, ObjectGUID guid, const std::string& name, GameDefine::UnitClass unitClass, const vec3& position, const vec3& scale, const vec2& pitchYaw)
        {
            bool result = Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitAddPacket{
                .guid = guid,
                .name = name,
                .unitClass = static_cast<u8>(unitClass),
                .position = position,
                .scale = scale,
                .pitchYaw = pitchYaw
            });

            return result;
        }
        bool BuildUnitBaseInfo(Bytebuffer& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid, const Gameplay::Faction::FactionRuntimeData& factionRuntime)
        {
            auto& unitAuraInfo = registry.get<Components::UnitAuraInfo>(entity);
            auto& unitPowersComponent = registry.get<Components::UnitPowersComponent>(entity);
            auto& displayInfo = registry.get<Components::DisplayInfo>(entity);
            auto& visualItems = registry.get<Components::UnitVisualItems>(entity);
            auto& objectFields = registry.get<Components::ObjectFields>(entity);
            auto& unitFields = registry.get<Components::UnitFields>(entity);
            auto& unitFaction = registry.get<Components::UnitFaction>(entity);

            bool failed = false;

            failed |= !Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitFactionUpdatePacket{
                .guid = guid,
                .factionID = factionRuntime.GetFactionID(unitFaction.effectiveFaction),
                .playerReactionBounds = unitFaction.effectivePlayerReactionBounds
            });

            u32 numEquippedItems = static_cast<u32>(MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd) + 1;
            for (u32 i = 0; i < numEquippedItems; i++)
            {
                if (visualItems.equippedItemIDs[i] == 0)
                    continue;

                failed |= !Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitVisualItemUpdatePacket{
                    .guid = guid,
                    .slot = static_cast<ItemEquipSlot_t>(i),
                    .itemID = visualItems.equippedItemIDs[i]
                });
            }

            for (auto& pair : unitPowersComponent.powerTypeToValue)
            {
                failed |= !Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitPowerUpdatePacket{
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

                failed |= !Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitAddAuraPacket{
                    .guid = guid,
                    .auraInstanceID = entt::to_integral(entity),
                    .spellID = pair.first,
                    .duration = auraInfo.duration,
                    .stacks = auraInfo.stacks
                });
            }

            auto objectGUID = objectFields.fields.GetField<ObjectGUID>(MetaGen::Shared::NetField::ObjectNetFieldEnum::ObjectGUIDLow);

            failed |= !Util::MessageBuilder::CreatePacket(buffer, (::Network::OpcodeType)MetaGen::PacketListEnum::ServerObjectNetFieldUpdatePacket, [&buffer, &objectFields, objectGUID]() -> bool
            {
                return buffer.Serialize(objectGUID) && objectFields.fields.SerializeSetFields(&buffer);
            });

            failed |= !Util::MessageBuilder::CreatePacket(buffer, (::Network::OpcodeType)MetaGen::PacketListEnum::ServerUnitNetFieldUpdatePacket, [&buffer, &unitFields, objectGUID]() -> bool
            {
                return buffer.Serialize(objectGUID) && unitFields.fields.SerializeSetFields(&buffer);
            });

            return !failed;
        }

        bool BuildUnitAdd(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID guid, const std::string& name, GameDefine::UnitClass unitClass, const vec3& position, const vec3& scale, const vec2& pitchYaw)
        {
            return buffer != nullptr && BuildUnitAdd(*buffer, guid, name, unitClass, position, scale, pitchYaw);
        }

        bool BuildUnitBaseInfo(std::shared_ptr<Bytebuffer>& buffer, entt::registry& registry, entt::entity entity, ObjectGUID guid, const Gameplay::Faction::FactionRuntimeData& factionRuntime)
        {
            return buffer != nullptr && BuildUnitBaseInfo(*buffer, registry, entity, guid, factionRuntime);
        }
    }

    namespace CombatLog
    {
        bool BuildDamageDealtMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 damage, f64 overKillDamage)
        {
            bool result = CreatePacket(buffer, MetaGen::Shared::Packet::ServerSendCombatEventPacket::PACKET_ID, [&, sourceGUID, targetGUID, damage, overKillDamage]() -> bool
            {
                return buffer.Put(MetaGen::Shared::CombatLog::CombatLogEventEnum::DamageDealt) &&
                       buffer.Serialize(sourceGUID) &&
                       buffer.Serialize(targetGUID) &&
                       buffer.PutF64(damage) &&
                       buffer.PutF64(overKillDamage);
            });

            return result;
        }
        bool BuildHealingDoneMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 healing, f64 overHealing)
        {
            bool result = CreatePacket(buffer, MetaGen::Shared::Packet::ServerSendCombatEventPacket::PACKET_ID, [&, sourceGUID, targetGUID, healing, overHealing]() -> bool
            {
                return buffer.Put(MetaGen::Shared::CombatLog::CombatLogEventEnum::HealingDone) &&
                       buffer.Serialize(sourceGUID) &&
                       buffer.Serialize(targetGUID) &&
                       buffer.PutF64(healing) &&
                       buffer.PutF64(overHealing);
            });

            return result;
        }
        bool BuildResurrectedMessage(Bytebuffer& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 restoredHealth)
        {
            bool result = CreatePacket(buffer, MetaGen::Shared::Packet::ServerSendCombatEventPacket::PACKET_ID, [&buffer, sourceGUID, targetGUID, restoredHealth]() -> bool
            {
                return buffer.Put(MetaGen::Shared::CombatLog::CombatLogEventEnum::Resurrected) &&
                       buffer.Serialize(sourceGUID) &&
                       buffer.Serialize(targetGUID) &&
                       buffer.PutF64(restoredHealth);
            });

            return result;
        }

        bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 damage, f64 overKillDamage)
        {
            return buffer != nullptr && BuildDamageDealtMessage(*buffer, sourceGUID, targetGUID, damage, overKillDamage);
        }

        bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 healing, f64 overHealing)
        {
            return buffer != nullptr && BuildHealingDoneMessage(*buffer, sourceGUID, targetGUID, healing, overHealing);
        }

        bool BuildResurrectedMessage(std::shared_ptr<Bytebuffer>& buffer, ObjectGUID sourceGUID, ObjectGUID targetGUID, f64 restoredHealth)
        {
            return buffer != nullptr && BuildResurrectedMessage(*buffer, sourceGUID, targetGUID, restoredHealth);
        }
    }

    namespace Cheat
    {
        bool BuildCheatCommandResultMessage(Bytebuffer& buffer, MetaGen::Shared::Cheat::CheatCommandEnum command, u8 result, const std::string& response)
        {
            bool createdPacket = CreatePacket(buffer, MetaGen::Shared::Packet::ServerCheatCommandResultPacket::PACKET_ID, [&buffer, &response, command, result]() -> bool
            {
                return buffer.Put(command) && buffer.PutU8(result) && buffer.PutString(response);
            });

            return createdPacket;
        }

        bool BuildCheatCharacterAddResponse(Bytebuffer& buffer, u8 result)
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

                default:
                    break;
            }

            return BuildCheatCommandResultMessage(buffer, MetaGen::Shared::Cheat::CheatCommandEnum::CharacterAdd, result, *resultString);
        }
        bool BuildCheatCharacterRemoveResponse(Bytebuffer& buffer, u8 result)
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

                default:
                    break;
            }

            return BuildCheatCommandResultMessage(buffer, MetaGen::Shared::Cheat::CheatCommandEnum::CharacterRemove, result, *resultString);
        }

        bool BuildCheatCommandResultMessage(std::shared_ptr<Bytebuffer>& buffer, MetaGen::Shared::Cheat::CheatCommandEnum command, u8 result, const std::string& response)
        {
            return buffer != nullptr && BuildCheatCommandResultMessage(*buffer, command, result, response);
        }

        bool BuildCheatCharacterAddResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
        {
            return buffer != nullptr && BuildCheatCharacterAddResponse(*buffer, result);
        }

        bool BuildCheatCharacterRemoveResponse(std::shared_ptr<Bytebuffer>& buffer, u8 result)
        {
            return buffer != nullptr && BuildCheatCharacterRemoveResponse(*buffer, result);
        }
    }
}
