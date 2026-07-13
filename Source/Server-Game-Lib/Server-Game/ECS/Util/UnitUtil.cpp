#include "UnitUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
#include "Server-Game/ECS/Components/UnitSpellCooldownHistory.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Gameplay/ECS/Components/UnitFields.h>

#include <MetaGen/Shared/Packet/Packet.h>

#include <entt/entt.hpp>

using namespace ECS;

namespace ECS::Util::Unit
{
    u32 GetDisplayIDFromRaceGender(GameDefine::UnitRace race, GameDefine::UnitGender unitGender)
    {
        u32 displayID = 0;

        switch (race)
        {
            case GameDefine::UnitRace::Human:
            {
                displayID = 49;
                break;
            }
            case GameDefine::UnitRace::Orc:
            {
                displayID = 51;
                break;
            }
            case GameDefine::UnitRace::Dwarf:
            {
                displayID = 53;
                break;
            }
            case GameDefine::UnitRace::NightElf:
            {
                displayID = 55;
                break;
            }
            case GameDefine::UnitRace::Undead:
            {
                displayID = 57;
                break;
            }
            case GameDefine::UnitRace::Tauren:
            {
                displayID = 59;
                break;
            }
            case GameDefine::UnitRace::Gnome:
            {
                displayID = 1563;
                break;
            }
            case GameDefine::UnitRace::Troll:
            {
                displayID = 1478;
                break;
            }

            default: break;
        }

        displayID += 1 * (unitGender != GameDefine::UnitGender::Male);

        return displayID;
    }
    void UpdateDisplayID(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, u32 displayID, bool forceDirty)
    {
        // If the displayID is the same, we don't need to update it
        u32 currentDisplayID = unitFields.fields.GetField<u32>(MetaGen::Shared::NetField::UnitNetFieldEnum::DisplayID);
        if (currentDisplayID == displayID)
            return;
        
        unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::DisplayID, displayID);
        registry.emplace_or_replace<Events::ObjectNeedsNetFieldUpdate>(entity);
    }
    void UpdateDisplayRaceGender(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitRace race, GameDefine::UnitGender gender, bool forceDirty)
    {
        constexpr u8 GenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitOffset;
        constexpr u8 RaceBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitSize;
        constexpr u8 GenderBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitSize;
        constexpr u8 RaceGenderBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceByteOffset;
        constexpr u8 RaceGenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitOffset;
        constexpr u8 RaceGenderBitSize = RaceBitSize + GenderBitSize;

        auto currentRaceGenderPacked = unitFields.fields.GetField<u16>(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, RaceGenderBytesOffset, RaceGenderBitOffset, RaceGenderBitSize);
        u16 newRaceGenderPacked = static_cast<u16>(race) | static_cast<u16>(gender) << GenderBitOffset;

        if (currentRaceGenderPacked == newRaceGenderPacked)
            return;

        unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, newRaceGenderPacked, RaceGenderBytesOffset, RaceGenderBitOffset, RaceGenderBitSize);
        registry.emplace_or_replace<Events::ObjectNeedsNetFieldUpdate>(entity);

        u32 displayID = GetDisplayIDFromRaceGender(race, gender);
        UpdateDisplayID(registry, entity, unitFields, displayID, forceDirty);
    }
    void UpdateDisplayRace(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitRace race, bool forceDirty)
    {
        constexpr u8 RaceByteOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceByteOffset;
        constexpr u8 RaceBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitOffset;
        constexpr u8 RaceBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitSize;
        constexpr u8 GenderBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderByteOffset;
        constexpr u8 GenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitOffset;
        constexpr u8 GenderBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitSize;
        constexpr u8 GenderBitMask = (1 << GenderBitSize) - 1;
        constexpr u8 RaceGenderBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceByteOffset;
        constexpr u8 RaceGenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitOffset;
        constexpr u8 RaceGenderBitSize = RaceBitSize + GenderBitSize;

        auto packedRaceGender = unitFields.fields.GetField<u16>(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, RaceGenderBytesOffset, RaceGenderBitOffset, RaceGenderBitSize);
        auto currentRace = static_cast<GameDefine::UnitRace>(packedRaceGender & RaceBitSize);

        if (currentRace == race)
            return;

        auto currentGender = static_cast<GameDefine::UnitGender>((packedRaceGender >> GenderBitOffset) & GenderBitMask);
        unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, race, RaceByteOffset, RaceBitOffset, RaceBitSize);
        registry.emplace_or_replace<Events::ObjectNeedsNetFieldUpdate>(entity);

        u32 displayID = GetDisplayIDFromRaceGender(race, currentGender);
        UpdateDisplayID(registry, entity, unitFields, displayID, forceDirty);
    }
    void UpdateDisplayGender(entt::registry& registry, entt::entity entity, Components::UnitFields& unitFields, GameDefine::UnitGender gender, bool forceDirty)
    {
        constexpr u8 RaceBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitSize;
        constexpr u8 GenderBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderByteOffset;
        constexpr u8 GenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitOffset;
        constexpr u8 GenderBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::GenderBitSize;
        constexpr u8 GenderBitMask = (1 << GenderBitSize) - 1;
        constexpr u8 RaceGenderBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceByteOffset;
        constexpr u8 RaceGenderBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::RaceBitOffset;
        constexpr u8 RaceGenderBitSize = RaceBitSize + GenderBitSize;

        auto packedRaceGender = unitFields.fields.GetField<u16>(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, RaceGenderBytesOffset, RaceGenderBitOffset, RaceGenderBitSize);
        auto currentGender = static_cast<GameDefine::UnitGender>((packedRaceGender >> GenderBitOffset) & GenderBitMask);

        if (currentGender == gender)
            return;

        auto currentRace = static_cast<GameDefine::UnitRace>(packedRaceGender & RaceBitSize);

        unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, gender, GenderBytesOffset, GenderBitOffset, GenderBitSize);
        registry.emplace_or_replace<Events::ObjectNeedsNetFieldUpdate>(entity);

        u32 displayID = GetDisplayIDFromRaceGender(currentRace, currentGender);
        UpdateDisplayID(registry, entity, unitFields, displayID, forceDirty);
    }

    ECS::Components::UnitPowersComponent& AddPowersComponent(World& world, entt::entity entity, GameDefine::UnitClass unitClass)
    {
        auto& unitPowersComponent = world.Emplace<Components::UnitPowersComponent>(entity);

        AddPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health, 100.0, 100.0, 100.0);

        switch (unitClass)
        {
            case GameDefine::UnitClass::Warrior:
            {
                AddPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Rage, 100.0, 0.0, 100.0);
                break;
            }
            case GameDefine::UnitClass::Paladin:
            case GameDefine::UnitClass::Priest:
            case GameDefine::UnitClass::Shaman:
            case GameDefine::UnitClass::Mage:
            case GameDefine::UnitClass::Warlock:
            case GameDefine::UnitClass::Druid:
            {
                AddPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Mana, 100.0, 100.0, 100.0);
                break;
            }

            case GameDefine::UnitClass::Hunter:
            {
                AddPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Focus, 100.0, 0.0, 100.0);
                break;
            }

            case GameDefine::UnitClass::Rogue:
            {
                AddPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Energy, 100.0, 0.0, 100.0);
                break;
            }
        }

        return unitPowersComponent;
    }

    ECS::Components::UnitResistancesComponent& AddResistancesComponent(World& world, entt::entity entity)
    {
        auto& unitResistancesComponent = world.Emplace<Components::UnitResistancesComponent>(entity);
        return unitResistancesComponent;
    }

    Components::UnitStatsComponent& AddStatsComponent(World& world, entt::entity entity)
    {
        auto& unitStatsComponent = world.Emplace<Components::UnitStatsComponent>(entity);

        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Health, 100.0, 100.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Stamina, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Strength, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Agility, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Intellect, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Spirit, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Armor, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::AttackPower, 0.0, 0.0);
        AddStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::SpellPower, 0.0, 0.0);

        return unitStatsComponent;
    }
    f32 HandleRageRegen(f32 current, f32 rateModifier, f32 deltaTime)
    {
        static constexpr f32 RageGainPerSecond = -10.0f;

        f32 result = glm::clamp(current + (RageGainPerSecond * rateModifier) * deltaTime, 0.0f, 100.0f);
        return result;
    }
    f32 HandleEnergyRegen(f32 current, f32 max, f32 rateModifier, f32 deltaTime)
    {
        static constexpr f32 EnergyGainPerSecond = 10.0f;

        f32 result = glm::clamp(current + ((EnergyGainPerSecond * rateModifier) * deltaTime), 0.0f, max);
        return result;
    }

    void TeleportToXYZ(World& world, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, const vec3& position, f32 orientation)
    {
        transform.position = position;
        transform.pitchYaw.y = orientation;
        
        world.playerVisData.Update(objectInfo.guid, transform.position.x, transform.position.z);

        ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitTeleportPacket{
            .guid = objectInfo.guid,
            .position = transform.position,
            .orientation = transform.pitchYaw.y
        });
    }

    bool TeleportToLocation(Singletons::WorldState& worldState, World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, entt::entity entity, Components::ObjectInfo& objectInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, u32 mapID, const vec3& position, f32 orientation)
    {
        if (transform.mapID == mapID)
        {
            TeleportToXYZ(world, networkState, entity, objectInfo, transform, visibilityInfo, position, orientation);
            return true;
        }

        // Transfer to new map
        if (!Util::Cache::MapExistsByID(gameCache, mapID))
            return false;

        world.EmplaceOrReplace<Events::CharacterNeedsDeinitialization>(entity);
        auto& characterTransferRequest = world.EmplaceOrReplace<Events::CharacterWorldTransfer>(entity);
        characterTransferRequest.targetMapID = mapID;
        characterTransferRequest.targetPosition = position;
        characterTransferRequest.targetOrientation = orientation;

        return true;
    }

    void SendChatMessage(World& world, Singletons::NetworkState& networkState, ::Network::SocketID socketID, const std::string& message)
    {
        ECS::Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerSendChatMessagePacket{
            .guid = ObjectGUID::Empty,
            .message = message
        });
    }
    bool HasSpellCooldown(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID)
    {
        auto itr = unitSpellCooldownHistory.spellIDToCooldown.find(spellID);
        if (itr == unitSpellCooldownHistory.spellIDToCooldown.end())
            return false;

        bool onCooldown = itr->second > 0.0f;
        return onCooldown;
    }
    f32 GetSpellCooldownRemaining(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID)
    {
        auto itr = unitSpellCooldownHistory.spellIDToCooldown.find(spellID);
        if (itr == unitSpellCooldownHistory.spellIDToCooldown.end())
            return 0.0f;

        return itr->second;
    }
    void SetSpellCooldown(Components::UnitSpellCooldownHistory& unitSpellCooldownHistory, u32 spellID, f32 cooldown)
    {
        unitSpellCooldownHistory.spellIDToCooldown[spellID] = cooldown;
    }

    bool HasPower(const Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType)
    {
        bool hasPowerType = unitPowersComponent.powerTypeToValue.contains(powerType);
        return hasPowerType;
    }
    UnitPower& GetPower(Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType)
    {
        return unitPowersComponent.powerTypeToValue.at(powerType);
    }
    UnitPower* TryGetPower(Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType)
    {
        if (!HasPower(unitPowersComponent, powerType))
            return nullptr;

        return &unitPowersComponent.powerTypeToValue[powerType];
    }
    bool AddPower(World& world, entt::entity entity, Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType, f64 base, f64 current, f64 max)
    {
        if (HasPower(unitPowersComponent, powerType))
            return false;

        unitPowersComponent.dirtyPowerTypes.insert(powerType);
        UnitPower& power = unitPowersComponent.powerTypeToValue[powerType];
        power.base = base;
        power.current = current;
        power.max = max;

        if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Health && power.current < power.max)
            world.EmplaceOrReplace<Tags::IsMissingHealth>(entity);

        world.EmplaceOrReplace<Events::UnitNeedsPowerUpdate>(entity);
        return true;
    }
    bool SetPower(World& world, entt::entity entity, Components::UnitPowersComponent& unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum powerType, f64 base, f64 current, f64 max)
    {
        if (!HasPower(unitPowersComponent, powerType))
            return false;

        UnitPower& power = unitPowersComponent.powerTypeToValue[powerType];

        bool changed = power.base != base || power.current != current || power.max != max;
        if (changed)
        {
            unitPowersComponent.dirtyPowerTypes.insert(powerType);
            power.base = base;
            power.current = current;
            power.max = max;

            if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Health)
            {
                if (power.current == 0)
                {
                    unitPowersComponent.timeToNextUpdate = 0.0f;
                }
                else
                {
                    if (power.current < power.max)
                    {
                        world.EmplaceOrReplace<Tags::IsMissingHealth>(entity);
                    }
                    else
                    {
                        unitPowersComponent.timeToNextUpdate = 0.0f;
                        world.Remove<Tags::IsMissingHealth>(entity);
                    }
                }
            }

            world.EmplaceOrReplace<Events::UnitNeedsPowerUpdate>(entity);
        }

        return true;
    }

    bool HasResistance(const Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType)
    {
        bool hasResistanceType = unitResistancesComponent.resistanceTypeToValue.contains(resistanceType);
        return hasResistanceType;
    }
    UnitResistance& GetResistance(Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType)
    {
        return unitResistancesComponent.resistanceTypeToValue.at(resistanceType);
    }
    bool AddResistance(Components::UnitResistancesComponent& unitResistancesComponent, MetaGen::Shared::Unit::ResistanceTypeEnum resistanceType, f64 base, f64 current, f64 max)
    {
        if (HasResistance(unitResistancesComponent, resistanceType))
            return false;

        unitResistancesComponent.dirtyResistanceTypes.insert(resistanceType);
        unitResistancesComponent.resistanceTypeToValue[resistanceType] = UnitResistance{
            .base = base,
            .current = current,
            .max = max
        };

        return true;
    }

    bool HasStat(const Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType)
    {
        bool hasStatType = unitStatsComponent.statTypeToValue.contains(statType);
        return hasStatType;
    }
    UnitStat& GetStat(Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType)
    {
        return unitStatsComponent.statTypeToValue.at(statType);
    }
    bool AddStat(Components::UnitStatsComponent& unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum statType, f64 base, f64 current)
    {
        if (HasStat(unitStatsComponent, statType))
            return false;

        unitStatsComponent.dirtyStatTypes.insert(statType);
        unitStatsComponent.statTypeToValue[statType] = UnitStat{
            .base = base,
            .current = current
        };

        return true;
    }

    bool AddAura(World& world, Singletons::GameCache& gameCache, entt::entity caster, entt::entity target, Components::UnitAuraInfo& unitAuraInfo, u32 spellID, u16 stackCount, entt::entity& outAuraEntity)
    {
        entt::entity auraEntity = GetAura(unitAuraInfo, spellID);

        // Create new aura if it doesn't exist
        if (auraEntity == entt::null)
        {
            GameDefine::Database::Spell* spell = nullptr;
            if (!Cache::GetSpellByID(gameCache, spellID, spell))
                return false;

            auraEntity = world.CreateEntity();

            auto* casterObjectInfo = world.TryGet<Components::ObjectInfo>(caster);
            auto& targetObjectInfo = world.Get<Components::ObjectInfo>(target);

            world.Emplace<Components::AuraEffectInfo>(auraEntity);
            auto& auraInfo = world.Emplace<Components::AuraInfo>(auraEntity);
            auraInfo.caster = casterObjectInfo ? casterObjectInfo->guid : ObjectGUID::Empty;
            auraInfo.target = targetObjectInfo.guid;

            auraInfo.flags = { 0 };
            auraInfo.flags.isBuff = true;
            auraInfo.stacks = stackCount;

            auraInfo.spellID = spellID;
            auraInfo.duration = spell->duration;
            auraInfo.timeRemaining = spell->duration;

            world.Emplace<Tags::IsUnpreparedAura>(auraEntity);
            unitAuraInfo.spellIDToAuraEntity[spellID] = auraEntity;
        }
        else
        {
            auto& auraInfo = world.Get<Components::AuraInfo>(auraEntity);

            // Check if stacks would overflow
            if (std::numeric_limits<u16>::max() - auraInfo.stacks < stackCount)
                return false;

            auraInfo.stacks += stackCount;

            // TODO : Check if duration is allowed to be refreshed
            auraInfo.timeRemaining = auraInfo.duration;

            if (!world.AllOf<Tags::IsUnpreparedAura>(auraEntity))
                world.EmplaceOrReplace<Events::AuraRefreshed>(auraEntity);
        }

        outAuraEntity = auraEntity;
        return true;
    }
    bool RemoveAura(World& world, Components::UnitAuraInfo& unitAuraInfo, u32 spellID, u16 stacksToRemove)
    {
        entt::entity auraEntity = GetAura(unitAuraInfo, spellID);
        if (auraEntity == entt::null)
            return false;

        auto& auraInfo = world.Get<Components::AuraInfo>(auraEntity);
        if (auraInfo.stacks > stacksToRemove)
        {
            auraInfo.stacks -= stacksToRemove;

            if (!world.AllOf<Tags::IsUnpreparedAura>(auraEntity))
                world.EmplaceOrReplace<Events::AuraRefreshed>(auraEntity);
        }
        else
        {
            auraInfo.stacks = 0;
            unitAuraInfo.spellIDToAuraEntity.erase(spellID);

            world.Emplace<Events::AuraExpired>(auraEntity);
            world.Remove<Tags::IsActiveAura>(auraEntity);
        }

        return true;
    }
    bool HasAura(Components::UnitAuraInfo& unitAuraInfo, u32 spellID)
    {
        bool hasAura = unitAuraInfo.spellIDToAuraEntity.contains(spellID);
        return hasAura;
    }
    entt::entity GetAura(Components::UnitAuraInfo& unitAuraInfo, u32 spellID)
    {
        auto itr = unitAuraInfo.spellIDToAuraEntity.find(spellID);
        if (itr == unitAuraInfo.spellIDToAuraEntity.end())
            return entt::null;

        return itr->second;
    }
}