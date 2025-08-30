#include "CharacterInitialization.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void CharacterInitialization::Init(entt::registry& registry)
    {
    }

    void CharacterInitialization::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
        Singletons::WorldState& worldState = ctx.get<Singletons::WorldState>();

        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        // Handle Deinitizialization of Characters
        {
            auto view = registry.view<Components::ObjectInfo, Components::NetInfo, Tags::CharacterNeedsDeinitialization>();
            view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo)
            {
                u64 characterID = objectInfo.guid.GetCounter();

                if (auto* playerContainers = registry.try_get<Components::PlayerContainers>(entity))
                {
                    for (const auto& [bagIndex, bagContainer] : playerContainers->bags)
                    {
                        u16 numContainerSlots = bagContainer.GetTotalSlots();
                        for (u16 containerSlot = 0; containerSlot < numContainerSlots; containerSlot++)
                        {
                            const Database::ContainerItem& item = bagContainer.GetItem(containerSlot);
                            if (item.IsEmpty())
                                continue;

                            u64 itemInstanceID = item.objectGuid.GetCounter();
                            Util::Cache::ItemInstanceDelete(gameCache, itemInstanceID);
                        }
                    }

                    u16 numEquipmentSlots = playerContainers->equipment.GetTotalSlots();
                    for (u16 equipmentSlot = 0; equipmentSlot < numEquipmentSlots; equipmentSlot++)
                    {
                        const Database::ContainerItem& item = playerContainers->equipment.GetItem(equipmentSlot);
                        if (item.IsEmpty())
                            continue;

                        u64 itemInstanceID = item.objectGuid.GetCounter();
                        Util::Cache::ItemInstanceDelete(gameCache, itemInstanceID);
                    }
                }

                if (auto* characterInfo = registry.try_get<Components::CharacterInfo>(entity))
                {
                    Util::Cache::CharacterDelete(registry, gameCache, characterID, *characterInfo);
                }

                // Network Cleanup
                {
                    Util::Network::UnlinkSocketFromEntity(networkState, netInfo.socketID);
                    Util::Network::UnlinkCharacterFromSocket(networkState, characterID);
                    Util::Network::UnlinkCharacterFromEntity(networkState, characterID);
                }

                // Grid Cleanup
                if (auto* transform = registry.try_get<Components::Transform>(entity))
                {
                    World& world = worldState.GetWorld(transform->mapID);
                    world.guidToEntity.erase(objectInfo.guid);
                    world.RemovePlayer(objectInfo.guid);

                    registry.destroy(entity);
                }
            });

            registry.clear<Tags::CharacterNeedsDeinitialization>();
        }

        // Handle Initialization of Characters
        {
            auto view = registry.view<Components::ObjectInfo, Components::NetInfo, Tags::CharacterNeedsInitialization>();
            view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo)
            {
                u64 characterID = objectInfo.guid.GetCounter();
                pqxx::result databaseResult;

                if (!Database::Util::Character::CharacterGetInfoByID(databaseConn, characterID, databaseResult))
                {
                    Util::Network::RequestSocketToClose(networkState, netInfo.socketID);
                    return;
                }

                auto& characterInfo = registry.emplace<Components::CharacterInfo>(entity);
                characterInfo.name = databaseResult[0][2].as<std::string>();
                characterInfo.accountID = databaseResult[0][1].as<u32>();
                characterInfo.totalTime = databaseResult[0][3].as<u32>();
                characterInfo.levelTime = databaseResult[0][4].as<u32>();
                characterInfo.logoutTime = databaseResult[0][5].as<u32>();
                characterInfo.flags = databaseResult[0][6].as<u32>();
                characterInfo.level = databaseResult[0][8].as<u16>();
                characterInfo.experiencePoints = databaseResult[0][9].as<u64>();

                auto& transform = registry.emplace<Components::Transform>(entity);
                transform.mapID = databaseResult[0][10].as<u16>();
                transform.position = vec3(databaseResult[0][11].as<f32>(), databaseResult[0][12].as<f32>(), databaseResult[0][13].as<f32>());

                auto& aabb = registry.emplace<Components::AABB>(entity);
                aabb.extents = vec3(3.0f, 5.0f, 2.0f); // TODO: Create proper data for this

                f32 orientation = databaseResult[0][14].as<f32>();
                transform.rotation = quat(vec3(0.0f, orientation, 0.0f));

                registry.emplace<Components::VisibilityInfo>(entity);
                auto& visibilityUpdateInfo = registry.emplace<Components::VisibilityUpdateInfo>(entity);
                visibilityUpdateInfo.updateInterval = 0.25f;
                visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;

                Components::DisplayInfo& displayInfo = registry.emplace<Components::DisplayInfo>(entity);

                u16 raceGenderClass = databaseResult[0][7].as<u16>();
                GameDefine::UnitRace unitRace = static_cast<GameDefine::UnitRace>(raceGenderClass & 0x7F);
                GameDefine::UnitGender unitGender = static_cast<GameDefine::UnitGender>((raceGenderClass >> 7) & 0x3);
                GameDefine::UnitClass unitClass = static_cast<GameDefine::UnitClass>((raceGenderClass >> 9) & 0x7F);

                characterInfo.unitClass = unitClass;
                Util::Unit::UpdateDisplayRaceGender(registry, entity, displayInfo, unitRace, unitGender);

                Util::Unit::AddStatsComponent(registry, entity);

                Components::TargetInfo& targetInfo = registry.emplace<Components::TargetInfo>(entity);
                targetInfo.target = entt::null;

                registry.emplace<Components::PlayerContainers>(entity);

                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<4096>();

                bool failed = false;
                failed |= !Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, entity, objectInfo.guid);
                failed |= !Util::MessageBuilder::Entity::BuildSetMoverMessage(buffer, objectInfo.guid);

                if (Database::Util::Character::CharacterGetItems(databaseConn, characterID, databaseResult))
                {
                    databaseResult.for_each([&](u64 itemInstanceID, u32 itemID, u64 ownerID, u16 count, u16 durability)
                    {
                        Database::ItemInstance itemInstance =
                        {
                            .id = itemInstanceID,
                            .ownerID = ownerID,
                            .itemID = itemID,
                            .count = count,
                            .durability = durability
                        };

                        Util::Cache::ItemInstanceCreate(gameCache, itemInstanceID, itemInstance);
                    });

                    if (Database::Util::Character::CharacterGetItemsInContainer(databaseConn, characterID, PLAYER_BASE_CONTAINER_ID, databaseResult))
                    {
                        auto& playerContainers = registry.get<Components::PlayerContainers>(entity);

                        databaseResult.for_each([&](u64 characterID, u64 containerID, u16 slotIndex, u64 itemInstanceID)
                        {
                            Database::ItemInstance* itemInstance = nullptr;
                            if (!Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
                                return;

                            GameDefine::ObjectGuid itemGuid = GameDefine::ObjectGuid::CreateItem(itemInstanceID);
                            playerContainers.equipment.AddItemToSlot(itemGuid, slotIndex);

                            if (slotIndex >= PLAYER_BAG_INDEX_START)
                                return;

                            failed |= !Util::MessageBuilder::Item::BuildItemCreate(buffer, itemGuid, *itemInstance);
                        });

                        for (ItemEquipSlot_t bagIndex = PLAYER_BAG_INDEX_START; bagIndex <= PLAYER_BAG_INDEX_END; bagIndex++)
                        {
                            if (playerContainers.equipment.IsSlotEmpty(bagIndex))
                                continue;

                            GameDefine::ObjectGuid containerGuid = playerContainers.equipment.GetItem(bagIndex).objectGuid;
                            u64 containerID = containerGuid.GetCounter();

                            Database::ItemInstance* containerItemInstance = nullptr;
                            if (!Util::Cache::GetItemInstanceByID(gameCache, containerID, containerItemInstance))
                                continue;

                            playerContainers.bags.emplace(bagIndex, Database::Container(containerItemInstance->durability));
                            Database::Container& container = playerContainers.bags[bagIndex];

                            if (Database::Util::Character::CharacterGetItemsInContainer(databaseConn, characterID, containerID, databaseResult))
                            {
                                databaseResult.for_each([&](u64 characterID, u64 containerID, u16 slotIndex, u64 itemInstanceID)
                                {
                                    Database::ItemInstance* itemInstance = nullptr;
                                    if (!Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
                                        return;

                                    GameDefine::ObjectGuid itemGuid = GameDefine::ObjectGuid::CreateItem(itemInstanceID);
                                    container.AddItemToSlot(itemGuid, slotIndex);

                                    failed |= !Util::MessageBuilder::Item::BuildItemCreate(buffer, itemGuid, *itemInstance);
                                });
                            }
                        }
                    }
                }
                registry.emplace_or_replace<Tags::CharacterNeedsContainerUpdate>(entity);

                for (auto& trigger : gameCache.proximityTriggerTables.triggers)
                {
                    if (!gameCache.proximityTriggerTables.triggerIDToEntity.contains(trigger.id))
                        continue;

                    entt::entity triggerEntity = gameCache.proximityTriggerTables.triggerIDToEntity[trigger.id];
                    if (registry.all_of<Tags::ProximityTriggerIsServerSideOnly>(triggerEntity))
                        continue;

                    auto& triggerTransform = registry.get<Components::Transform>(triggerEntity);
                    auto& triggerAABB = registry.get<Components::AABB>(triggerEntity);
                    auto& proximityTrigger = registry.get<Components::ProximityTrigger>(triggerEntity);

                    if (triggerTransform.mapID != transform.mapID)
                        continue;

                    if (!Util::MessageBuilder::ProximityTrigger::BuildProximityTriggerCreate(buffer, trigger.id, trigger.name, trigger.flags, triggerTransform.mapID, triggerTransform.position, triggerAABB.extents))
                        continue;
                }

                if (failed)
                {
                    NC_LOG_ERROR("Failed to build character initialization message for character: {0}", characterID);
                    networkState.server->CloseSocketID(netInfo.socketID);
                    return;
                }

                World& world = worldState.GetWorld(transform.mapID);
                world.InitPlayer(objectInfo.guid, entity, transform.position.x, transform.position.z);

                networkState.server->SendPacket(netInfo.socketID, buffer);
            });

            registry.clear<Tags::CharacterNeedsInitialization>();
        }
    }
}