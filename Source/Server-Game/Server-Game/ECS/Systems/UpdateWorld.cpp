#include "UpdateWorld.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/CreatureInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/UnitVisualItems.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/ProximityTriggers.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/ProximityTriggerUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/ECS/Util/Persistence/CharacterUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/CreatureUtils.h>
#include <Server-Common/Database/Util/ProximityTriggerUtils.h>

#include <Base/Util/DebugHandler.h>

#include <Meta/Generated/Shared/NetworkPacket.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void UpdateWorld::Init(entt::registry& registry)
    {
        auto& worldState = registry.ctx().emplace<Singletons::WorldState>();
    }

    void UpdateWorld::HandleMapInitialization(World& world, Singletons::GameCache& gameCache)
    {
        if (!world.ContainsSingleton<Events::MapNeedsInitialization>())
            return;

        // Handle Map Initialization
        world.EmplaceSingleton<Singletons::ProximityTriggers>();

        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        pqxx::result databaseResult;
        if (Database::Util::Creature::CreatureGetInfoByMap(databaseConn, world.mapID, databaseResult))
        {
            databaseResult.for_each([&world, &gameCache](u64 id, u32 templateID, u32 displayID, u16 mapID, f32 posX, f32 posY, f32 posZ, f32 posO)
            {
                entt::entity creaureEntity = world.CreateEntity();

                Events::CreatureNeedsInitialization event =
                {
                    .guid = ObjectGUID::CreateCreature(id),
                    .templateID = templateID,
                    .displayID = displayID,
                    .mapID = mapID,
                    .position = vec3(posX, posY, posZ),
                    .orientation = posO
                };

                world.Emplace<Events::CreatureNeedsInitialization>(creaureEntity, event);
            });
        }

        if (Database::Util::ProximityTrigger::ProximityTriggerGetInfoByMap(databaseConn, world.mapID, databaseResult))
        {
            databaseResult.for_each([&world, &gameCache](u32 id, const std::string& name, u16 flags, u16 mapID, f32 posX, f32 posY, f32 posZ, f32 extentX, f32 extentY, f32 extentZ)
            {
                entt::entity triggerEntity = world.CreateEntity();

                Events::ProximityTriggerNeedsInitialization event =
                {
                    .triggerID = id,

                    .name = name,
                    .flags = static_cast<Generated::ProximityTriggerFlagEnum>(flags),

                    .mapID = mapID,
                    .position = vec3(posX, posY, posZ),
                    .extents = vec3(extentX, extentY, extentZ)
                };

                world.Emplace<Events::ProximityTriggerNeedsInitialization>(triggerEntity, event);
            });
        }

        // Stress Test - Spawn a bunch of creatures
        //for (u32 i = 0; i < 999; i++)
        //{
        //    auto guid = ObjectGUID(ObjectGUID::Type::Creature, 500 + i);
        //    u64 characterID = guid.GetCounter();
        //
        //    entt::entity entity = world.CreateEntity();
        //
        //    auto& objectInfo = world.Emplace<Components::ObjectInfo>(entity);
        //    objectInfo.guid = guid;
        //
        //    auto& creatureInfo = world.Emplace<Components::CreatureInfo>(entity);
        //    creatureInfo.name = "Test";
        //    creatureInfo.templateID = 2;
        //
        //    Components::Transform& transform = world.Emplace<Components::Transform>(entity);
        //    u32 col = i % 50;
        //    u32 row = i / 50;
        //
        //    transform.position = vec3(-1200.0f + 1.5f * col, -56.6f + 0.1f * row, 1165.0f + 1.5f * row);
        //    transform.pitchYaw = vec2(0.0f);
        //
        //    world.Emplace<Components::VisibilityInfo>(entity);
        //    auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
        //    visibilityUpdateInfo.updateInterval = 0.25f;
        //    visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;
        //
        //    Components::DisplayInfo& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
        //    Util::Unit::UpdateDisplayRaceGender(*world.registry, entity, displayInfo, GameDefine::UnitRace::NightElf, GameDefine::UnitGender::Male);
        //    Util::Unit::AddStatsComponent(*world.registry, entity);
        //
        //    world.AddEntity(objectInfo.guid, entity, vec2(transform.position.x, transform.position.z));
        //}
        
        world.EraseSingleton<Events::MapNeedsInitialization>();
    }

    void UpdateWorld::HandleCreatureDeinitialization(World& world, Singletons::GameCache& gameCache)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        auto view = world.View<Events::CreatureNeedsDeinitialization>();
        view.each([&](entt::entity entity, Events::CreatureNeedsDeinitialization& event)
        {
            entt::entity creaureEntity = world.GetEntity(event.guid);
            if (creaureEntity == entt::null)
                return;

            world.RemoveEntity(event.guid);
            world.DestroyEntity(creaureEntity);

            auto transaction = databaseConn->NewTransaction();
            if (!Database::Util::Creature::CreatureDelete(transaction, event.guid.GetCounter()))
                return;

            transaction.commit();
        });
        world.Clear<Events::CreatureNeedsDeinitialization>();
    }
    void UpdateWorld::HandleCreatureInitialization(World& world, Singletons::GameCache& gameCache)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        auto createView = world.View<Events::CreatureCreate>();
        createView.each([&](entt::entity entity, Events::CreatureCreate& event)
        {
            GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
                return;

            auto transaction = databaseConn->NewTransaction();

            u64 creatureID = 0;
            if (!Database::Util::Creature::CreatureCreate(transaction, event.templateID, event.displayID, event.mapID, event.position, event.orientation, creatureID))
                return;

            transaction.commit();

            Events::CreatureNeedsInitialization initEvent =
            {
                .guid = ObjectGUID::CreateCreature(creatureID),
                .templateID = event.templateID,
                .displayID = 0,
                .mapID = event.mapID,
                .position = event.position,
                .orientation = event.orientation
            };

            world.Emplace<Events::CreatureNeedsInitialization>(entity, initEvent);
        });
        world.Clear<Events::CreatureCreate>();

        auto initView = world.View<Events::CreatureNeedsInitialization>();
        initView.each([&](entt::entity entity, Events::CreatureNeedsInitialization& event)
        {
            GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
                return;

            auto& objectInfo = world.Emplace<Components::ObjectInfo>(entity);
            objectInfo.guid = event.guid;

            auto& creatureInfo = world.Emplace<Components::CreatureInfo>(entity);
            creatureInfo.templateID = event.templateID;
            creatureInfo.name = creatureTemplate->name;

            auto& creaureTransform = world.Emplace<Components::Transform>(entity);
            creaureTransform.mapID = event.mapID;
            creaureTransform.position = event.position;
            creaureTransform.pitchYaw = vec2(0.0f, event.orientation);
            creaureTransform.scale = vec3(1.0f);

            auto& visualItems = world.Emplace<Components::UnitVisualItems>(entity);
            auto& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
            displayInfo.displayID = creatureTemplate->displayID;

            Components::UnitStatsComponent& unitStats = Util::Unit::AddStatsComponent(*world.registry, entity);
            unitStats.baseHealth *= creatureTemplate->healthMod;
            unitStats.currentHealth *= creatureTemplate->healthMod;

            auto& visibilityInfo = world.Emplace<Components::VisibilityInfo>(entity);
            auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
            visibilityUpdateInfo.updateInterval = 0.5f;

            world.AddEntity(objectInfo.guid, entity, vec2(creaureTransform.position.x, creaureTransform.position.z));
        });
        world.Clear<Events::CreatureNeedsInitialization>();
    }

    void UpdateWorld::HandleCharacterDeinitialization(Singletons::WorldState& worldState, World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto view = world.View<Components::ObjectInfo, Components::NetInfo, Events::CharacterNeedsDeinitialization>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo)
        {
            u64 characterID = objectInfo.guid.GetCounter();
        
            if (auto* playerContainers = world.TryGet<Components::PlayerContainers>(entity))
            {
                for (const auto& [bagIndex, bagContainer] : playerContainers->bags)
                {
                    u16 numContainerSlots = bagContainer.GetTotalSlots();
                    for (u16 containerSlot = 0; containerSlot < numContainerSlots; containerSlot++)
                    {
                        const Database::ContainerItem& item = bagContainer.GetItem(containerSlot);
                        if (item.IsEmpty())
                            continue;
        
                        u64 itemInstanceID = item.objectGUID.GetCounter();
                        Util::Cache::ItemInstanceDelete(gameCache, itemInstanceID);
                    }
                }
        
                u16 numEquipmentSlots = playerContainers->equipment.GetTotalSlots();
                for (u16 equipmentSlot = 0; equipmentSlot < numEquipmentSlots; equipmentSlot++)
                {
                    const Database::ContainerItem& item = playerContainers->equipment.GetItem(equipmentSlot);
                    if (item.IsEmpty())
                        continue;
        
                    u64 itemInstanceID = item.objectGUID.GetCounter();
                    Util::Cache::ItemInstanceDelete(gameCache, itemInstanceID);
                }
            }
        
            // Network Cleanup
            {
                Util::Network::UnlinkSocketFromEntity(networkState, netInfo.socketID);
                Util::Network::UnlinkCharacterFromSocket(networkState, characterID);
                Util::Network::UnlinkCharacterFromEntity(networkState, characterID);
            }
        
            // Grid Cleanup
            if (auto* transform = world.TryGet<Components::Transform>(entity))
            {
                f32 orientation = transform->pitchYaw.y;
                Util::Persistence::Character::CharacterSetMapIDPositionOrientation(gameCache, characterID, transform->mapID, transform->position, orientation);
        
                world.RemoveEntity(objectInfo.guid);
            }
        
            if (auto* characterInfo = world.TryGet<Components::CharacterInfo>(entity))
            {
                if (auto* worldTransfter = world.TryGet<Events::CharacterWorldTransfer>(entity))
                {
                    worldState.AddWorldTransferRequest(WorldTransferRequest{
                        .socketID = netInfo.socketID,

                        .characterName = characterInfo->name,
                        .characterID = characterID,

                        .targetMapID = worldTransfter->targetMapID,
                        .targetPosition = worldTransfter->targetPosition,
                        .targetOrientation = worldTransfter->targetOrientation,
                        .useTargetPosition = true
                    });
                }

                Util::Cache::CharacterDelete(gameCache, characterID, *characterInfo);
            }
        
            world.DestroyEntity(entity);
        });
        
        world.Clear<Events::CharacterNeedsDeinitialization>();
    }
    void UpdateWorld::HandleCharacterInitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<8192>();

        auto view = world.View<Components::ObjectInfo, Components::NetInfo, Events::CharacterNeedsInitialization>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            pqxx::result databaseResult;
        
            if (!Database::Util::Character::CharacterGetInfoByID(databaseConn, characterID, databaseResult))
            {
                Util::Network::RequestSocketToClose(networkState, netInfo.socketID);
                return;
            }

            auto& characterInfo = world.Emplace<Components::CharacterInfo>(entity);
            characterInfo.name = databaseResult[0][2].as<std::string>();
            characterInfo.accountID = databaseResult[0][1].as<u32>();
            characterInfo.totalTime = databaseResult[0][3].as<u32>();
            characterInfo.levelTime = databaseResult[0][4].as<u32>();
            characterInfo.logoutTime = databaseResult[0][5].as<u32>();
            characterInfo.flags = databaseResult[0][6].as<u32>();
            characterInfo.level = databaseResult[0][8].as<u16>();
            characterInfo.experiencePoints = databaseResult[0][9].as<u64>();
        
            auto& transform = world.Emplace<Components::Transform>(entity);
            transform.mapID = databaseResult[0][10].as<u16>();
            transform.position = vec3(databaseResult[0][11].as<f32>(), databaseResult[0][12].as<f32>(), databaseResult[0][13].as<f32>());
        
            f32 orientation = databaseResult[0][14].as<f32>();
            transform.pitchYaw = vec2(0.0f, orientation);

            auto& aabb = world.Emplace<Components::AABB>(entity);
            aabb.extents = vec3(3.0f, 5.0f, 2.0f); // TODO: Create proper data for this
        
            world.Emplace<Components::VisibilityInfo>(entity);
            auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
            visibilityUpdateInfo.updateInterval = 0.25f;
            visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;
        
            auto& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
            auto& visualItems = world.Emplace<Components::UnitVisualItems>(entity);
        
            u16 raceGenderClass = databaseResult[0][7].as<u16>();
            auto unitRace = static_cast<GameDefine::UnitRace>(raceGenderClass & 0x7F);
            auto unitGender = static_cast<GameDefine::UnitGender>((raceGenderClass >> 7) & 0x3);
            auto unitClass = static_cast<GameDefine::UnitClass>((raceGenderClass >> 9) & 0x7F);
        
            characterInfo.unitClass = unitClass;
            Util::Unit::UpdateDisplayRaceGender(*world.registry, entity, displayInfo, unitRace, unitGender);
        
            Util::Unit::AddStatsComponent(*world.registry, entity);
        
            Components::TargetInfo& targetInfo = world.Emplace<Components::TargetInfo>(entity);
            targetInfo.target = entt::null;
        
            auto& playerContainers = world.Emplace<Components::PlayerContainers>(entity);
        
            buffer->Reset();
            bool failed = false;

            failed |= !Util::MessageBuilder::Unit::BuildUnitCreate(buffer, *world.registry, entity, objectInfo.guid, characterInfo.name);
            failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::UnitSetMoverPacket{
                .guid = objectInfo.guid
            });
        
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
                    databaseResult.for_each([&](u64 characterID, u64 containerID, u16 slotIndex, u64 itemInstanceID)
                    {
                        Database::ItemInstance* itemInstance = nullptr;
                        if (!Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
                            return;
        
                        ObjectGUID itemGUID = ObjectGUID::CreateItem(itemInstanceID);
                        playerContainers.equipment.AddItemToSlot(itemGUID, slotIndex);
        
                        if (slotIndex >= PLAYER_BAG_INDEX_START)
                            return;
        
                        failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::ItemAddPacket{
                            .guid = itemGUID,
                            .itemID = itemInstance->itemID,
                            .count = itemInstance->count,
                            .durability = itemInstance->durability
                        });
                    });
        
                    for (ItemEquipSlot_t bagIndex = PLAYER_BAG_INDEX_START; bagIndex <= PLAYER_BAG_INDEX_END; bagIndex++)
                    {
                        if (playerContainers.equipment.IsSlotEmpty(bagIndex))
                            continue;
        
                        ObjectGUID containerGUID = playerContainers.equipment.GetItem(bagIndex).objectGUID;
                        u64 containerID = containerGUID.GetCounter();
        
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
        
                                ObjectGUID itemGUID = ObjectGUID::CreateItem(itemInstanceID);
                                container.AddItemToSlot(itemGUID, slotIndex);

                                failed |= !Util::Network::AppendPacketToBuffer(buffer, Generated::ItemAddPacket{
                                    .guid = itemGUID,
                                    .itemID = itemInstance->itemID,
                                    .count = itemInstance->count,
                                    .durability = itemInstance->durability
                                });
                            });
                        }
                    }
                }

                bool hasDirtyVisualItems = false;
                for (ItemEquipSlot_t i = static_cast<ItemEquipSlot_t>(Generated::ItemEquipSlotEnum::EquipmentStart); i <= static_cast<ItemEquipSlot_t>(Generated::ItemEquipSlotEnum::EquipmentEnd); i++)
                {
                    const auto& item = playerContainers.equipment.GetItem(static_cast<ItemEquipSlot_t>(i));
                    if (item.IsEmpty())
                        continue;

                    ObjectGUID itemGUID = item.objectGUID;

                    Database::ItemInstance* itemInstance = nullptr;
                    if (!Util::Cache::GetItemInstanceByID(gameCache, itemGUID.GetCounter(), itemInstance))
                        continue;

                    visualItems.equippedItemIDs[i] = itemInstance->itemID;
                    visualItems.dirtyItemIDs.insert(i);

                    hasDirtyVisualItems = true;
                }

                if (hasDirtyVisualItems)
                    world.EmplaceOrReplace<Events::CharacterNeedsVisualItemUpdate>(entity);
            }
        
            world.EmplaceOrReplace<Events::CharacterNeedsContainerUpdate>(entity);

            // Send Proximity Triggers
            {
                auto view = world.View<Components::ProximityTrigger, Components::Transform, Components::AABB, Tags::ProximityTriggerIsClientSide>();
                view.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Components::Transform& transform, Components::AABB& aabb)
                {
                    failed |= Util::Network::AppendPacketToBuffer(buffer, Generated::ServerTriggerAddPacket{
                        .triggerID = proximityTrigger.triggerID,

                        .name = proximityTrigger.name,
                        .flags = static_cast<u8>(proximityTrigger.flags),

                        .mapID = transform.mapID,
                        .position = transform.position,
                        .extents = aabb.extents
                    });
                });
            }

            if (failed)
            {
                NC_LOG_ERROR("Failed to build character initialization message for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }
        
            world.AddEntity(objectInfo.guid, entity, vec2(transform.position.x, transform.position.z));
        
            Util::Network::SendPacket(networkState, netInfo.socketID, buffer);
        });
        
        world.Clear<Events::CharacterNeedsInitialization>();
    }

    void UpdateWorld::HandleProximityTriggerDeinitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        auto view = world.View<Components::ProximityTrigger, Events::ProximityTriggerNeedsDeinitialization>();
        view.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Events::ProximityTriggerNeedsDeinitialization& event)
        {
            auto transaction = databaseConn->NewTransaction();
            if (!Database::Util::ProximityTrigger::ProximityTriggerDelete(transaction, event.triggerID))
                return;

            transaction.commit();

            Generated::ServerTriggerRemovePacket triggerRemovePacket =
            {
                .triggerID = event.triggerID
            };

            std::shared_ptr<Bytebuffer> triggerRemoveBuffer = Bytebuffer::BorrowRuntime(sizeof(::Network::MessageHeader) + triggerRemovePacket.GetSerializedSize());
            Util::Network::AppendPacketToBuffer(triggerRemoveBuffer, triggerRemovePacket);

            bool isServerSideOnly = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) == Generated::ProximityTriggerFlagEnum::IsServerSideOnly;

            for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
            {
                if (isServerSideOnly && Util::ProximityTrigger::IsPlayerInTrigger(proximityTrigger, playerEntity))
                {
                    Util::ProximityTrigger::OnExit(proximityTrigger, playerEntity);
                }

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                    continue;

                Util::Network::SendPacket(networkState, socketID, triggerRemoveBuffer);
            }

            world.DestroyEntity(entity);
            Util::ProximityTrigger::RemoveTriggerFromWorld(proximityTriggers, event.triggerID, isServerSideOnly);
        });
        world.Clear<Events::CreatureNeedsDeinitialization>();
    }
    void UpdateWorld::HandleProximityTriggerInitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        {
            auto transaction = databaseConn->NewTransaction();

            auto createView = world.View<Events::ProximityTriggerCreate>();
            createView.each([&](entt::entity entity, Events::ProximityTriggerCreate& event)
            {
                u32 triggerID;
                if (!Database::Util::ProximityTrigger::ProximityTriggerCreate(transaction, event.name, static_cast<u16>(event.flags), event.mapID, event.position, event.extents, triggerID))
                    return;

                Events::ProximityTriggerNeedsInitialization initEvent =
                {
                    .triggerID = triggerID,

                    .name = event.name,
                    .flags = event.flags,

                    .mapID = event.mapID,
                    .position = event.position,
                    .extents = event.extents
                };

                world.Emplace<Events::ProximityTriggerNeedsInitialization>(entity, initEvent);
            });

            transaction.commit();
            world.Clear<Events::ProximityTriggerCreate>();
        }

        auto view = world.View<Events::ProximityTriggerNeedsInitialization>();
        view.each([&](entt::entity entity, Events::ProximityTriggerNeedsInitialization& event)
        {
            auto& proximityTrigger = world.Emplace<Components::ProximityTrigger>(entity);
            proximityTrigger.triggerID = event.triggerID;
            proximityTrigger.name = event.name;
            proximityTrigger.flags = event.flags;

            auto& transform = world.Emplace<Components::Transform>(entity);
            transform.mapID = event.mapID;
            transform.position = event.position;

            auto& aabb = world.Emplace<Components::AABB>(entity);
            aabb.centerPos = vec3(0, 0, 0);
            aabb.extents = event.extents;

            auto& worldAABB = world.Emplace<Components::WorldAABB>(entity);
            worldAABB.min = transform.position + aabb.centerPos - aabb.extents;
            worldAABB.max = transform.position + aabb.centerPos + aabb.extents;

            bool isServerSideOnly = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) != Generated::ProximityTriggerFlagEnum::None;
            if (isServerSideOnly)
            {
                world.Emplace<Tags::ProximityTriggerIsServerSideOnly>(entity);
            }
            else
            {
                world.Emplace<Tags::ProximityTriggerIsClientSide>(entity);

                Generated::ServerTriggerAddPacket triggerAddPacket =
                {
                    .triggerID = event.triggerID,

                    .name = event.name,
                    .flags = static_cast<u8>(event.flags),

                    .mapID = event.mapID,
                    .position = event.position,
                    .extents = event.extents
                };

                std::shared_ptr<Bytebuffer> triggerAddBuffer = Bytebuffer::BorrowRuntime(sizeof(::Network::MessageHeader) + triggerAddPacket.GetSerializedSize());
                Util::Network::AppendPacketToBuffer(triggerAddBuffer, triggerAddPacket);

                for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
                {
                    Network::SocketID socketID;
                    if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                        continue;

                    Util::Network::SendPacket(networkState, socketID, triggerAddBuffer);
                }
            }

            Util::ProximityTrigger::AddTriggerToWorld(proximityTriggers, entity, event.triggerID, worldAABB, isServerSideOnly);
        });
        world.Clear<Events::ProximityTriggerNeedsInitialization>();
    }
    void UpdateWorld::HandleProximityTriggerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        HandleProximityTriggerDeinitialization(world, gameCache, networkState);
        HandleProximityTriggerInitialization(world, gameCache, networkState);

        robin_hood::unordered_set<entt::entity> playersToRemove;
        playersToRemove.reserve(1024);

        auto serverSideAuthoritativeUpdateView = world.View<Components::ProximityTrigger, Components::Transform, Components::WorldAABB, Tags::ProximityTriggerHasEnteredPlayers>();
        serverSideAuthoritativeUpdateView.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Components::Transform& transform, Components::WorldAABB& worldAABB)
        {
            for (auto playerEntity : proximityTrigger.playersEntered)
            {
                if (!world.ValidEntity(playerEntity))
                    continue;

                Util::ProximityTrigger::OnEnter(proximityTrigger, playerEntity);
            }

            proximityTrigger.playersEntered.clear();
        });
        world.Clear<Tags::ProximityTriggerHasEnteredPlayers>();

        auto serverSideOnlyUpdateView = world.View<Components::ProximityTrigger, Components::Transform, Components::WorldAABB, Tags::ProximityTriggerIsServerSideOnly>();
        serverSideOnlyUpdateView.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Components::Transform& transform, Components::WorldAABB& worldAABB)
        {
            playersToRemove = proximityTrigger.playersInside;

            vec4 minMaxRect = vec4(
                worldAABB.min.x,
                worldAABB.min.z,
                worldAABB.max.x,
                worldAABB.max.z
            );

            world.GetPlayersInRect(minMaxRect, [&](const ObjectGUID& guid) -> bool
            {
                entt::entity playerEntity = world.GetEntity(guid);

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, guid.GetCounter(), socketID))
                    return true;

                if (Util::ProximityTrigger::IsPlayerInTrigger(proximityTrigger, playerEntity))
                {
                    Util::ProximityTrigger::OnStay(proximityTrigger, playerEntity);
                    playersToRemove.erase(playerEntity);
                }
                else
                {
                    Util::ProximityTrigger::AddPlayerToTrigger(proximityTrigger, playerEntity);
                    Util::ProximityTrigger::OnEnter(proximityTrigger, playerEntity);
                }

                return true;
            });

            // Loop over players that have exited the trigger and call OnExit
            for (auto playerEntity : playersToRemove)
            {
                if (!world.ValidEntity(playerEntity))
                    continue;

                Util::ProximityTrigger::RemovePlayerFromTrigger(proximityTrigger, playerEntity);
                Util::ProximityTrigger::OnExit(proximityTrigger, playerEntity);
            }
        });
    }

    void UpdateWorld::HandleReplication(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        robin_hood::unordered_set<ObjectGUID> cachedList;

        auto playerView = world.View<Components::ObjectInfo, Components::NetInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        playerView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            if (!Util::Network::IsSocketActive(networkState, netInfo.socketID))
                return;

            visibilityUpdateInfo.timeSinceLastUpdate += deltaTime;
            if (visibilityUpdateInfo.timeSinceLastUpdate < visibilityUpdateInfo.updateInterval)
                return;

            // Query nearby players
            {
                robin_hood::unordered_set<ObjectGUID>& visiblePlayers = visibilityInfo.visiblePlayers;
                cachedList = visiblePlayers;

                world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                {
                    if (objectInfo.guid == guid)
                        return true;

                    if (cachedList.contains(guid))
                    {
                        cachedList.erase(guid);
                        return true;
                    }

                    visiblePlayers.insert(guid);

                    entt::entity otherEntity = world.GetEntity(guid);
                    auto& otherObjectInfo = world.Get<Components::ObjectInfo>(otherEntity);
                    auto& otherCharacterInfo = world.Get<Components::CharacterInfo>(otherEntity);

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                    if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, *world.registry, otherEntity, otherObjectInfo.guid, otherCharacterInfo.name))
                        return true;

                    networkState.server->SendPacket(netInfo.socketID, buffer);
                    return true;
                });

                for (ObjectGUID& prevVisibleGUID : cachedList)
                {
                    visiblePlayers.erase(prevVisibleGUID);

                    Util::Network::SendPacket(networkState, netInfo.socketID, Generated::UnitRemovePacket{
                        .guid = prevVisibleGUID
                    });
                }
            }

            // Query nearby units
            {
                robin_hood::unordered_set<ObjectGUID>& visibleCreatures = visibilityInfo.visibleCreatures;
                cachedList = visibleCreatures;

                world.GetCreaturesInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                {
                    if (cachedList.contains(guid))
                    {
                        cachedList.erase(guid);
                        return true;
                    }

                    return true;
                });

                if (cachedList.size() > 0)
                {
                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<8192>();

                    for (ObjectGUID creatureGUID : cachedList)
                    {
                        visibleCreatures.erase(creatureGUID);

                        if (!Util::Network::AppendPacketToBuffer(buffer, Generated::UnitRemovePacket{
                            .guid = creatureGUID
                        }))
                        {
                            continue;
                        }
                    }

                    if (buffer->writtenData > 0)
                        networkState.server->SendPacket(netInfo.socketID, buffer);
                }
            }

            visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
        });

        robin_hood::unordered_set<ObjectGUID> playersEnteredInView;
        playersEnteredInView.reserve(cachedList.size());

        auto creatureView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        creatureView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            robin_hood::unordered_set<ObjectGUID>& visiblePlayers = visibilityInfo.visiblePlayers;
            cachedList = visiblePlayers;

            playersEnteredInView.clear();

            world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
            {
                if (cachedList.contains(guid))
                {
                    cachedList.erase(guid);
                    return true;
                }

                visiblePlayers.insert(guid);
                playersEnteredInView.insert(guid);
                return true;
            });

            if (playersEnteredInView.size() > 0)
            {
                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, *world.registry, entity, objectInfo.guid, creatureInfo.name))
                    return;

                for (ObjectGUID playerGUID : playersEnteredInView)
                {
                    entt::entity playerEntity = world.GetEntity(playerGUID);
                    if (playerEntity == entt::null)
                        continue;

                    if (auto* playerVisibilityInfo = world.TryGet<Components::VisibilityInfo>(playerEntity))
                    {
                        playerVisibilityInfo->visibleCreatures.insert(objectInfo.guid);
                    }

                    if (auto* playerNetInfo = world.TryGet<Components::NetInfo>(playerEntity))
                        networkState.server->SendPacket(playerNetInfo->socketID, buffer);
                }
            }

            if (cachedList.size() > 0)
            {
                for (ObjectGUID playerGUID : cachedList)
                {
                    visiblePlayers.erase(playerGUID);
                }
            }

            visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
        });
    }
    void UpdateWorld::HandleContainerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        std::shared_ptr<Bytebuffer> containerUpdateBuffer = Bytebuffer::Borrow<4096>();
        std::shared_ptr<Bytebuffer> equippedItemsBuffer = Bytebuffer::Borrow<512>();
    
        auto view = world.View<Components::ObjectInfo, Components::NetInfo, Components::VisibilityInfo, Components::PlayerContainers, Events::CharacterNeedsContainerUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::VisibilityInfo& visibilityInfo, Components::PlayerContainers& playerContainers)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            containerUpdateBuffer->Reset();
            equippedItemsBuffer->Reset();
    
            bool failed = false;
    
            // Bags
            for (ItemEquipSlot_t bagIndex = PLAYER_BAG_INDEX_START; bagIndex <= PLAYER_BAG_INDEX_END; bagIndex++)
            {
                if (playerContainers.equipment.IsSlotEmpty(bagIndex))
                    continue;
    
                const Database::ContainerItem& equipmentContainerItem = playerContainers.equipment.GetItem(bagIndex);
                Database::Container& bag = playerContainers.bags[bagIndex];
                u16 containerIndex = (bagIndex - PLAYER_BAG_INDEX_START) + 1;
    
                if (bag.IsUninitialized())
                {
                    Database::ItemInstance* containerItemInstance = nullptr;
                    if (!Util::Cache::GetItemInstanceByID(gameCache, equipmentContainerItem.objectGUID.GetCounter(), containerItemInstance))
                        continue;
    
                    u16 numSlots = bag.GetTotalSlots();
                    u16 numFreeSlots = bag.GetFreeSlots();

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddPacket{
                        .index = containerIndex,
                        .itemID = containerItemInstance->itemID,
                        .guid = equipmentContainerItem.objectGUID,
                        .numSlots = numSlots,
                        .numFreeSlots = numFreeSlots
                    });

                    if (numSlots != numFreeSlots)
                    {
                        for (u32 i = 0; i < numSlots; i++)
                        {
                            const Database::ContainerItem& containerItem = bag.GetItem(i);
                            if (containerItem.IsEmpty())
                                continue;

                            bag.SetSlotAsDirty(i);
                        }
                    }
                }

                for (u16 slotIndex : bag.GetDirtySlots())
                {
                    const Database::ContainerItem& containerItem = bag.GetItem(slotIndex);

                    if (containerItem.IsEmpty())
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerRemoveFromSlotPacket{
                            .index = containerIndex,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddToSlotPacket{
                            .index = containerIndex,
                            .slot = slotIndex,
                            .guid = containerItem.objectGUID
                        });
                    }
                }
    
                bag.ClearDirtySlots();
            }
    
            // Base Equipment Container
            {
                if (playerContainers.equipment.IsUninitialized())
                {
                    u16 numSlots = playerContainers.equipment.GetTotalSlots();
                    u16 numFreeSlots = playerContainers.equipment.GetFreeSlots();

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddPacket{
                        .index = PLAYER_BASE_CONTAINER_ID,
                        .itemID = 0,
                        .guid = ObjectGUID::Empty,
                        .numSlots = numSlots,
                        .numFreeSlots = numFreeSlots
                    });

                    if (numSlots != numFreeSlots)
                    {
                        for (u32 i = 0; i < numSlots; i++)
                        {
                            const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(i);
                            if (containerItem.IsEmpty())
                                continue;

                            playerContainers.equipment.SetSlotAsDirty(i);
                        }
                    }
                }

                for (u16 slotIndex : playerContainers.equipment.GetDirtySlots())
                {
                    const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(slotIndex);
                    bool isContainerItemEmpty = containerItem.IsEmpty();

                    if (isContainerItemEmpty)
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerRemoveFromSlotPacket{
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddToSlotPacket{
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex,
                            .guid = containerItem.objectGUID
                        });
                    }

                    if (slotIndex >= PLAYER_EQUIPMENT_INDEX_START && slotIndex <= PLAYER_EQUIPMENT_INDEX_END)
                    {
                        u32 itemID = 0;

                        if (isContainerItemEmpty)
                        {
                            Database::ItemInstance* itemInstance = nullptr;
                            if (!Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                                continue;

                            itemID = itemInstance->itemID;
                        }

                        failed |= !Util::Network::AppendPacketToBuffer(equippedItemsBuffer, Generated::ServerUnitEquippedItemUpdatePacket{
                            .guid = objectInfo.guid,
                            .slot = static_cast<u8>(slotIndex),
                            .itemID = itemID
                        });
                    }
                }
    
                playerContainers.equipment.ClearDirtySlots();

                if (equippedItemsBuffer->writtenData > 0)
                {
                    Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, equippedItemsBuffer);
                }
            }
    
            if (failed)
            {
                NC_LOG_ERROR("Failed to build character container update message for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }
    
            networkState.server->SendPacket(netInfo.socketID, containerUpdateBuffer);
        });
    
        world.Clear<Events::CharacterNeedsContainerUpdate>();
    }
    void UpdateWorld::HandleDisplayUpdate(World& world, Singletons::NetworkState& networkState)
    {
        auto view = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::DisplayInfo, Events::CharacterNeedsDisplayUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo, Components::DisplayInfo& displayInfo)
        {
            ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitDisplayInfoUpdatePacket{
                .guid = objectInfo.guid,
                .displayID = displayInfo.displayID,
                .race = static_cast<u8>(displayInfo.unitRace),
                .gender = static_cast<u8>(displayInfo.unitGender)
            });
        });
    
        world.Clear<Events::CharacterNeedsDisplayUpdate>();

        auto visualItemView = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::UnitVisualItems, Events::CharacterNeedsVisualItemUpdate>();
        visualItemView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo, Components::UnitVisualItems& visualItems)
        {
            for (ItemEquipSlot_t slot : visualItems.dirtyItemIDs)
            {
                ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::ServerUnitVisualItemUpdatePacket{
                    .guid = objectInfo.guid,
                    .slot = slot,
                    .itemID = visualItems.equippedItemIDs[slot]
                });
            }

            visualItems.dirtyItemIDs.clear();
        });

        world.Clear<Events::CharacterNeedsVisualItemUpdate>();
    }
    void UpdateWorld::HandlePowerUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        static f32 timeSinceLastUpdate = 0.0f;
        static constexpr f32 UPDATE_INTERVAL = 1.0f / 10.0f;

        auto view = world.View<Components::UnitStatsComponent>();
        view.each([&](entt::entity entity, Components::UnitStatsComponent& unitStatsComponent)
        {
            static constexpr f32 healthGainRate = 5.0f;
            static constexpr f32 powerGainRate = 25.0f;

            // Handle Health Regen
            {
                f32 currentHealth = unitStatsComponent.currentHealth;

                if (unitStatsComponent.currentHealth > 0.0f && currentHealth < unitStatsComponent.maxHealth)
                    unitStatsComponent.currentHealth = glm::clamp(currentHealth + (healthGainRate * deltaTime), 0.0f, unitStatsComponent.maxHealth);

                bool isHealthDirty = (currentHealth != unitStatsComponent.currentHealth || unitStatsComponent.healthIsDirty);
                unitStatsComponent.healthIsDirty |= isHealthDirty;
            }

            for (i32 i = 0; i < (u32)Generated::PowerTypeEnum::Count; i++)
            {
                f32 prevPowerValue = unitStatsComponent.currentPower[i];
                f32& powerValue = unitStatsComponent.currentPower[i];
                f32 basePower = unitStatsComponent.basePower[i];
                f32 maxPower = unitStatsComponent.maxPower[i];

                auto powerType = (Generated::PowerTypeEnum)i;
                if (powerType == Generated::PowerTypeEnum::Rage)
                {
                    if (powerValue == 0)
                        continue;
                }
                else
                {
                    if (powerValue == maxPower)
                        continue;
                }

                switch (powerType)
                {
                    case Generated::PowerTypeEnum::Rage:
                    {
                        powerValue = Util::Unit::HandleRageRegen(powerValue, 1.0f, deltaTime);
                        break;
                    }

                    case Generated::PowerTypeEnum::Focus:
                    case Generated::PowerTypeEnum::Energy:
                    {
                        powerValue = Util::Unit::HandleEnergyRegen(powerValue, maxPower, 1.0f, deltaTime);
                        break;
                    }

                    default:
                    {
                        continue;
                    }
                }

                bool isStatDirty = powerValue != prevPowerValue;
                unitStatsComponent.powerIsDirty[i] |= isStatDirty;
            }
        });

        timeSinceLastUpdate += deltaTime;
        if (timeSinceLastUpdate >= UPDATE_INTERVAL)
        {
            auto view = world.View<const Components::ObjectInfo, const Components::VisibilityInfo, Components::UnitStatsComponent>();
            view.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::VisibilityInfo& visibilityInfo, Components::UnitStatsComponent& unitStatsComponent)
            {
                if (unitStatsComponent.healthIsDirty)
                {
                    ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitResourcesUpdatePacket{
                        .guid = objectInfo.guid,
                        .powerType = static_cast<i8>(Generated::PowerTypeEnum::Health),
                        .base = unitStatsComponent.baseHealth,
                        .current = unitStatsComponent.currentHealth,
                        .max = unitStatsComponent.maxHealth
                    });

                    unitStatsComponent.healthIsDirty = false;
                }

                for (i32 i = 0; i < (u32)Generated::PowerTypeEnum::Count; i++)
                {
                    bool isDirty = unitStatsComponent.powerIsDirty[i];
                    if (!isDirty)
                        continue;

                    auto powerType = (Generated::PowerTypeEnum)i;

                    ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitResourcesUpdatePacket{
                        .guid = objectInfo.guid,
                        .powerType = static_cast<i8>(powerType),
                        .base = unitStatsComponent.baseHealth,
                        .current = unitStatsComponent.currentHealth,
                        .max = unitStatsComponent.maxHealth
                    });
                    unitStatsComponent.powerIsDirty[i] = false;
                }
            });

            timeSinceLastUpdate -= UPDATE_INTERVAL;
        }
    }
    void UpdateWorld::HandleSpellUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        struct CastEvent
        {
            ObjectGUID caster;
            ObjectGUID target;
        };
        
        std::vector<CastEvent> castEvents;
        
        auto view = world.View<Components::CastInfo>();
        view.each([&](entt::entity entity, Components::CastInfo& castInfo)
        {
            castInfo.duration += deltaTime;
        
            if (castInfo.duration >= castInfo.castTime)
            {
                castEvents.push_back({ castInfo.caster, castInfo.target });
            }
        });

        for (const CastEvent& castEvent : castEvents)
        {
            ObjectGUID casterGUID = castEvent.caster;
            ObjectGUID targetGUID = castEvent.target;

            entt::entity casterEntity = world.GetEntity(casterGUID);
            entt::entity targetEntity = world.GetEntity(targetGUID);

            if (!world.ValidEntity(casterEntity) || !world.ValidEntity(targetEntity))
                continue;
        
            auto& targetStats = world.Get<Components::UnitStatsComponent>(targetEntity);
            targetStats.currentHealth = glm::max(0.0f, targetStats.currentHealth - 35.0f);
            targetStats.healthIsDirty = true;
        
            // Send Grid Message
            {
                std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, casterGUID, targetGUID, 35.0f))
                    continue;
        
                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(casterEntity);
                Util::Network::SendToNearby(networkState, world, casterEntity, visibilityInfo, true, damageDealtMessage);
            }
        
            world.Remove<Components::CastInfo>(casterEntity);
        }
    }

    void UpdateWorld::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        // Handle World Transfer Requests
        {
            auto& worldTransferRequests = worldState.GetWorldTransferRequests();
            
            WorldTransferRequest request;
            while (worldTransferRequests.try_dequeue(request))
            {
                if (!Util::Network::IsSocketActive(networkState, request.socketID))
                    continue;

                if (!worldState.HasWorld(request.targetMapID))
                    worldState.AddWorld(request.targetMapID);

                World& world = worldState.GetWorld(request.targetMapID);
                entt::entity socketEntity = world.CreateEntity();

                auto& netInfo = world.Emplace<Components::NetInfo>(socketEntity);
                netInfo.socketID = request.socketID;

                auto& objectInfo = world.Emplace<Components::ObjectInfo>(socketEntity);
                objectInfo.guid = ObjectGUID(ObjectGUID::Type::Player, request.characterID);

                worldState.SetMapIDForSocket(request.socketID, request.targetMapID);
                networkState.socketIDToEntity[request.socketID] = socketEntity;
                networkState.characterIDToEntity[request.characterID] = socketEntity;
                networkState.characterIDToSocketID[request.characterID] = request.socketID;

                Util::Cache::CharacterCreate(gameCache, request.characterID, request.characterName, socketEntity);

                Util::Network::SendPacket(networkState, request.socketID, Generated::ServerWorldTransferPacket{
                });

                Util::Network::SendPacket(networkState, request.socketID, Generated::ServerLoadMapPacket{
                    .mapID = request.targetMapID
                });

                if (request.useTargetPosition)
                    Util::Persistence::Character::CharacterSetMapIDPositionOrientation(gameCache, request.characterID, request.targetMapID, request.targetPosition, request.targetOrientation);

                world.Emplace<Events::CharacterNeedsInitialization>(socketEntity);
            }
        }

        for (auto& itr : worldState)
        {
            u32 mapID = itr.first;
            World& world = itr.second;
            
            HandleMapInitialization(world, gameCache);
        
            HandleCharacterDeinitialization(worldState, world, gameCache, networkState);
            HandleCharacterInitialization(world, gameCache, networkState);

            HandleCreatureDeinitialization(world, gameCache);
            HandleCreatureInitialization(world, gameCache);

            HandleProximityTriggerUpdate(world, gameCache, networkState);

            HandleReplication(world, networkState, deltaTime);
            HandleContainerUpdate(world, gameCache, networkState);
            HandleDisplayUpdate(world, networkState);
            HandlePowerUpdate(world, networkState, deltaTime);
            HandleSpellUpdate(world, networkState, deltaTime);
        }
    }
}