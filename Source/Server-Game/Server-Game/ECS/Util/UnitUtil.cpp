#include "UnitUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"

#include <Meta/Generated/Shared/NetworkPacket.h>

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
    void UpdateDisplayID(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, u32 displayID, bool forceDirty)
    {
        // If the displayID is the same, we don't need to update it
        forceDirty = forceDirty && displayInfo.displayID != displayID;
        displayInfo.displayID = displayID;

        if (forceDirty)
            registry.emplace_or_replace<Events::CharacterNeedsDisplayUpdate>(entity);
    }
    void UpdateDisplayRaceGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, GameDefine::UnitGender gender, bool forceDirty)
    {
        displayInfo.unitRace = race;
        displayInfo.unitGender = gender;

        u32 displayID = GetDisplayIDFromRaceGender(race, gender);
        UpdateDisplayID(registry, entity, displayInfo, displayID, forceDirty);
    }
    void UpdateDisplayRace(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitRace race, bool forceDirty)
    {
        UpdateDisplayRaceGender(registry, entity, displayInfo, race, displayInfo.unitGender, forceDirty);
    }
    void UpdateDisplayGender(entt::registry& registry, entt::entity entity, Components::DisplayInfo& displayInfo, GameDefine::UnitGender gender, bool forceDirty)
    {
        UpdateDisplayRaceGender(registry, entity, displayInfo, displayInfo.unitRace, gender, forceDirty);
    }

    Components::UnitStatsComponent& AddStatsComponent(entt::registry& registry, entt::entity entity)
    {
        auto& unitStatsComponent = registry.emplace_or_replace<Components::UnitStatsComponent>(entity);

        {
            unitStatsComponent.baseHealth = 100.0f;
            unitStatsComponent.currentHealth = 50.0f;
            unitStatsComponent.maxHealth = 100.0f;
        }

        for (u32 i = 0; i < (u32)Generated::PowerTypeEnum::Count; ++i)
        {
            unitStatsComponent.basePower[i] = 100.0f;

            if (i == (u32)Generated::PowerTypeEnum::Mana || i == (u32)Generated::PowerTypeEnum::Rage || i == (u32)Generated::PowerTypeEnum::Happiness)
                unitStatsComponent.currentPower[i] = 100.0f;
            else
                unitStatsComponent.currentPower[i] = 0.0f;

            unitStatsComponent.maxPower[i] = 100.0f;
        }

        for (u32 i = 0; i < (u32)Generated::StatTypeEnum::Count; ++i)
        {
            unitStatsComponent.baseStat[i] = 10;
            unitStatsComponent.currentStat[i] = 10;
        }

        for (u32 i = 0; i < (u32)Generated::ResistanceTypeEnum::Count; ++i)
        {
            unitStatsComponent.baseResistance[i] = 5;
            unitStatsComponent.currentResistance[i] = 5;
        }

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

        ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::ServerUnitTeleportPacket{
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
        ECS::Util::Network::SendPacket(networkState, socketID, Generated::ServerSendChatMessagePacket{
            .guid = ObjectGUID::Empty,
            .message = message
        });
    }
}