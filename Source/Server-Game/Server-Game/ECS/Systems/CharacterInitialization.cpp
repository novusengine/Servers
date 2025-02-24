#include "CharacterInitialization.h"

#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

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
        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
        Singletons::WorldState& worldState = ctx.get<Singletons::WorldState>();
        
        auto view = registry.view<Components::ObjectInfo, Components::NetInfo, Tags::CharacterNeedsInitialization>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            Database::CharacterDefinition& characterDefinition = databaseState.characterTables.charIDToDefinition[characterID];

            Components::Transform& transform = registry.emplace<Components::Transform>(entity);
            transform.position = characterDefinition.position;
            transform.rotation = quat(vec3(0.0f, characterDefinition.orientation, 0.0f));

            registry.emplace<Components::VisibilityInfo>(entity);
            auto& visibilityUpdateInfo = registry.emplace<Components::VisibilityUpdateInfo>(entity);
            visibilityUpdateInfo.updateInterval = 0.25f;
            visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;

            Components::DisplayInfo& displayInfo = registry.emplace<Components::DisplayInfo>(entity);
            displayInfo.race = characterDefinition.GetRace();
            displayInfo.gender = characterDefinition.GetGender();
            displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(displayInfo.race, displayInfo.gender);

            UnitUtils::AddStatsComponent(registry, entity);

            Components::TargetInfo& targetInfo = registry.emplace<Components::TargetInfo>(entity);
            targetInfo.target = entt::null;

            registry.emplace<Components::PlayerContainers>(entity);

            std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<4096>();

            bool failed = false;
            failed |= !Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, entity, objectInfo.guid);
            failed |= !Util::MessageBuilder::Entity::BuildSetMoverMessage(buffer, objectInfo.guid);

            bool hasBaseContainer = databaseState.characterTables.charIDToBaseContainer.contains(characterID);
            if (hasBaseContainer)
            {
                const auto& baseContainer = databaseState.characterTables.charIDToBaseContainer[characterID];

                auto& playerContainers = registry.get<Components::PlayerContainers>(entity);
                playerContainers.equipment = baseContainer; // Copy Database Container

                u32 numEquipmentSlots = 19;

                for (u32 i = 0; i < numEquipmentSlots; i++)
                {
                    const auto& containerItem = playerContainers.equipment.GetItem(i);
                    if (containerItem.IsEmpty())
                        continue;

                    u64 itemInstanceID = containerItem.objectGuid.GetCounter();
                    const auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];

                    failed |= !Util::MessageBuilder::Item::BuildItemCreate(buffer, containerItem.objectGuid, itemInstance);
                }

                u32 firstBagIndex = 19;
                u32 numBags = 5;
                playerContainers.bags.resize(numBags);

                for (u32 i = 0; i < numBags; ++i)
                {
                    u32 bagSlotIndex = firstBagIndex + i;

                    const auto& containerItem = playerContainers.equipment.GetItem(bagSlotIndex);
                    if (containerItem.IsEmpty())
                        continue;

                    u64 containerInstanceID = containerItem.objectGuid.GetCounter();
                    if (!databaseState.itemTables.itemInstanceIDToContainer.contains(containerInstanceID))
                        continue;

                    const auto& container = databaseState.itemTables.itemInstanceIDToContainer[containerInstanceID];
                    auto& playerBagContainer = playerContainers.bags[i];
                    playerBagContainer = container; // Copy Database Container

                    u8 numSlots = playerBagContainer.GetTotalSlots();
                    u8 numFreeSlots = playerBagContainer.GetFreeSlots();
                    
                    if (numSlots != numFreeSlots)
                    {
                        for (u32 j = 0; j < numSlots; j++)
                        {
                            const auto& item = playerBagContainer.GetItem(j);
                            if (item.IsEmpty())
                                continue;

                            u64 itemInstanceID = item.objectGuid.GetCounter();
                            const auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];
                            failed |= !Util::MessageBuilder::Item::BuildItemCreate(buffer, item.objectGuid, itemInstance);
                        }
                    }

                    const auto& containerItemInstance = databaseState.itemTables.itemInstanceIDToDefinition[containerInstanceID];
                    failed |= !Util::MessageBuilder::Container::BuildContainerCreate(buffer, i + 1, containerItemInstance.itemID, containerItem.objectGuid, playerBagContainer);
                }

                failed |= !Util::MessageBuilder::Container::BuildContainerCreate(buffer, 0, 0, GameDefine::ObjectGuid::Empty, playerContainers.equipment);
            }

            if (failed)
            {
                NC_LOG_ERROR("Failed to build character initialization message for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }

            World& world = worldState.GetWorld(characterDefinition.mapID);

            world.guidToEntity[objectInfo.guid] = entity;
            world.AddPlayer(objectInfo.guid, characterDefinition.position.x, characterDefinition.position.z);

            //gridSingleton.cell.players.entering.insert(entity);
            networkState.server->SendPacket(netInfo.socketID, buffer);
        });

        registry.clear<Tags::CharacterNeedsInitialization>();
    }
}