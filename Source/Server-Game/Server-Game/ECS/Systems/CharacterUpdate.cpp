#include "CharacterUpdate.h"

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
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void CharacterUpdate::Init(entt::registry& registry)
    {
    }

    void HandleContainerUpdate(entt::registry& registry, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<4096>();

        auto view = registry.view<Components::ObjectInfo, Components::NetInfo, Components::PlayerContainers, Tags::CharacterNeedsContainerUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::PlayerContainers& playerContainers)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            buffer->Reset();

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
                    if (!Util::Cache::GetItemInstanceByID(gameCache, equipmentContainerItem.objectGuid.GetCounter(), containerItemInstance))
                        continue;

                    failed |= !Util::MessageBuilder::Container::BuildContainerCreate(buffer, containerIndex, containerItemInstance->itemID, equipmentContainerItem.objectGuid, bag);
                }
                else
                {
                    for (u16 slotIndex : bag.GetDirtySlots())
                    {
                        const Database::ContainerItem& containerItem = bag.GetItem(slotIndex);
                        if (containerItem.IsEmpty())
                        {
                            failed |= !Util::MessageBuilder::Container::BuildRemoveFromSlot(buffer, containerIndex, slotIndex);
                        }
                        else
                        {
                            failed |= !Util::MessageBuilder::Container::BuildAddToSlot(buffer, containerIndex, slotIndex, containerItem.objectGuid);
                        }
                    }
                }

                bag.ClearDirtySlots();
            }

            // Base Equipment Container
            {
                if (playerContainers.equipment.IsUninitialized())
                {
                    failed |= !Util::MessageBuilder::Container::BuildContainerCreate(buffer, PLAYER_BASE_CONTAINER_ID, 0, GameDefine::ObjectGuid::Empty, playerContainers.equipment);

                }
                else
                {
                    for (u16 slotIndex : playerContainers.equipment.GetDirtySlots())
                    {
                        const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(slotIndex);
                        if (containerItem.IsEmpty())
                        {
                            failed |= !Util::MessageBuilder::Container::BuildRemoveFromSlot(buffer, PLAYER_BASE_CONTAINER_ID, slotIndex);
                        }
                        else
                        {
                            failed |= !Util::MessageBuilder::Container::BuildAddToSlot(buffer, PLAYER_BASE_CONTAINER_ID, slotIndex, containerItem.objectGuid);
                        }
                    }
                }

                playerContainers.equipment.ClearDirtySlots();
            }

            if (failed)
            {
                NC_LOG_ERROR("Failed to build character container update message for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }

            networkState.server->SendPacket(netInfo.socketID, buffer);
        });

        registry.clear<Tags::CharacterNeedsContainerUpdate>();
    }

    void HandleDisplayUpdate(entt::registry& registry)
    {
        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<256>();

        auto view = registry.view<Components::ObjectInfo, Components::NetInfo, Components::DisplayInfo, Tags::CharacterNeedsDisplayUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::DisplayInfo& displayInfo)
        {
            buffer->Reset();

            if (!Util::MessageBuilder::Entity::BuildEntityDisplayInfoUpdateMessage(buffer, objectInfo.guid, displayInfo))
                return;

            ECS::Util::Grid::SendToNearby(registry, entity, netInfo, buffer, true);
        });

        registry.clear<Tags::CharacterNeedsDisplayUpdate>();
    }

    void CharacterUpdate::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();

        HandleContainerUpdate(registry, gameCache, networkState);
        HandleDisplayUpdate(registry);
    }
}