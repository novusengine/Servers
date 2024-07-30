#include "SimpleReplication.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Singletons/GridSingleton.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void SimpleReplication::Init(entt::registry& registry)
    {
        Singletons::GridSingleton& gridSingleton = registry.ctx().emplace<Singletons::GridSingleton>();

        gridSingleton.cell.mutex.lock();
        gridSingleton.cell.players.list.reserve(256);
        gridSingleton.cell.players.entering.reserve(256);
        gridSingleton.cell.players.leaving.reserve(256);

        gridSingleton.cell.updates.reserve(256);
        gridSingleton.cell.mutex.unlock();
    }

    void SimpleReplication::Update(entt::registry& registry, f32 deltaTime)
    {
        Singletons::GridSingleton& gridSingleton = registry.ctx().get<Singletons::GridSingleton>();
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();

        gridSingleton.cell.mutex.lock();

        u32 numPlayersLeaving = static_cast<u32>(gridSingleton.cell.players.leaving.size());
        u32 numPlayersEntering = static_cast<u32>(gridSingleton.cell.players.entering.size());
        u32 numPlayersInCell = static_cast<u32>(gridSingleton.cell.players.list.size());

        // Handle Leaving Players
        if (numPlayersLeaving)
        {
            bool areAllPlayersLeaving = numPlayersLeaving == numPlayersInCell;
            if (!areAllPlayersLeaving)
            {
                std::vector<Singletons::GridUpdate> updates;
                updates.reserve(256);

                // Remove all leaving players from the player list
                for (entt::entity entity : gridSingleton.cell.players.leaving)
                {
                    gridSingleton.cell.players.list.erase(entity);

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                    if (!Util::MessageBuilder::Entity::BuildEntityDestroyMessage(buffer, entity))
                        continue;

                    updates.push_back({ entity, {}, std::move(buffer) });
                }

                // Send Destroy Message to all players in the cell
                for (entt::entity entity : gridSingleton.cell.players.list)
                {
                    Network::SocketID socketID = networkState.EntityToSocketID[entity];

                    for (Singletons::GridUpdate& update : updates)
                    {
                        // Send buffer to all players in the cell
                        networkState.server->SendPacket(socketID, update.buffer);
                    }
                }
            }
            else
            {
                gridSingleton.cell.players.list.clear();
            }

            gridSingleton.cell.players.leaving.clear();
            numPlayersInCell = static_cast<u32>(gridSingleton.cell.players.list.size());
        }

        // Handle Entering Players
        if (numPlayersEntering)
        {
            std::vector<Singletons::GridUpdate> listUpdates;
            std::vector<Singletons::GridUpdate> enterUpdates;
            listUpdates.reserve(256);
            enterUpdates.reserve(256);

            if (numPlayersInCell)
            {
                // Create Packets for all players in the list
                for (entt::entity entity : gridSingleton.cell.players.list)
                {
                    if (!registry.valid(entity))
                        continue;

                    auto& transform = registry.get<Components::Transform>(entity);
                    auto& displayInfo = registry.get<Components::DisplayInfo>(entity);

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                    if (!Util::MessageBuilder::Entity::BuildEntityCreateMessage(buffer, entity, transform, displayInfo.displayID))
                        continue;

                    listUpdates.push_back({ entity, {}, std::move(buffer) });
                }
            }

            // Create Packets for all entering players
            for (entt::entity entity : gridSingleton.cell.players.entering)
            {
                if (!registry.valid(entity))
                    continue;

                if (!networkState.EntityToSocketID.contains(entity))
                    continue;

                Components::Transform& transform = registry.get<Components::Transform>(entity);
                auto& displayInfo = registry.get<Components::DisplayInfo>(entity);

                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                if (!Util::MessageBuilder::Entity::BuildEntityCreateMessage(buffer, entity, transform, displayInfo.displayID))
                    continue;

                enterUpdates.push_back({ entity, {}, std::move(buffer) });

                gridSingleton.cell.players.list.insert(entity);
            }

            // Send Create Messages to all players in the cell
            for (entt::entity entity : gridSingleton.cell.players.list)
            {
                Network::SocketID socketID = networkState.EntityToSocketID[entity];

                bool justEntered = gridSingleton.cell.players.entering.contains(entity);
                if (justEntered)
                {
                    for (Singletons::GridUpdate& update : listUpdates)
                    {
                        // Send buffer to all players in the cell
                        networkState.server->SendPacket(socketID, update.buffer);
                    }

                    gridSingleton.cell.players.entering.erase(entity);
                }

                for (Singletons::GridUpdate& update : enterUpdates)
                {
                    if (update.owner == entity)
                        continue;

                    networkState.server->SendPacket(socketID, update.buffer);
                }
            }

            numPlayersInCell = static_cast<u32>(gridSingleton.cell.players.list.size());
            gridSingleton.cell.players.entering.clear();
        }

        // Handle Updates
        {
            if (numPlayersInCell)
            {
                for (auto& update : gridSingleton.cell.updates)
                {
                    for (entt::entity entity : gridSingleton.cell.players.list)
                    {
                        bool shouldSkip = update.owner == entity && !update.flags.SendToSelf;
                        if (shouldSkip)
                            continue;

                        Network::SocketID socketID = networkState.EntityToSocketID[entity];

                        networkState.server->SendPacket(socketID, update.buffer);
                    }
                }
            }

            gridSingleton.cell.updates.clear();
        }

        gridSingleton.cell.mutex.unlock();
    }
}