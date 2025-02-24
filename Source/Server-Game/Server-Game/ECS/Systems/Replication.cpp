#include "Replication.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/ServiceLocator.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Base/Util/DebugHandler.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void Replication::Init(entt::registry& registry)
    {
        auto& worldState = registry.ctx().emplace<Singletons::WorldState>();

        worldState.AddWorld(0);

        // FAKE PLAYER BOTS FOR TESTING
        //for (u32 i = 0; i < 29; i++)
        //{
        //    GameDefine::ObjectGuid guid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, 500 + i);
        //    u64 characterID = guid.GetCounter();
        //
        //    entt::entity entity = registry.create();
        //    auto& objectInfo = registry.emplace<Components::ObjectInfo>(entity);
        //    objectInfo.guid = guid;
        //
        //    auto& netInfo = registry.emplace<Components::NetInfo>(entity);
        //    netInfo.socketID = Network::SOCKET_ID_INVALID;
        //
        //    Components::Transform& transform = registry.emplace<Components::Transform>(entity);
        //    u32 col = i % 5;
        //    u32 row = i / 5;
        //
        //    transform.position = vec3(5.0f * col, 0.0f, 5.0f * row);
        //    transform.rotation = quat(vec3(0.0f, 0.0f, 0.0f));
        //
        //    registry.emplace<Components::VisibilityInfo>(entity);
        //    auto& visibilityUpdateInfo = registry.emplace<Components::VisibilityUpdateInfo>(entity);
        //    visibilityUpdateInfo.updateInterval = 0.25f;
        //    visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;
        //
        //    Components::DisplayInfo& displayInfo = registry.emplace<Components::DisplayInfo>(entity);
        //    displayInfo.race = GameDefine::UnitRace::NightElf;
        //    displayInfo.gender = GameDefine::Gender::Male;
        //    displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(displayInfo.race, displayInfo.gender);
        //
        //    UnitUtils::AddStatsComponent(registry, entity);
        //
        //    World& world = worldState.GetWorld(0);
        //
        //    world.guidToEntity[objectInfo.guid] = entity;
        //    world.AddPlayer(objectInfo.guid, transform.position.x, transform.position.z);
        //}
    }

    void Replication::Update(entt::registry& registry, f32 deltaTime)
    {
        auto& worldState = registry.ctx().get<Singletons::WorldState>();
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();

        auto view = registry.view<Components::ObjectInfo, Components::VisibilityUpdateInfo>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            visibilityUpdateInfo.timeSinceLastUpdate += deltaTime;

            if (visibilityUpdateInfo.timeSinceLastUpdate >= visibilityUpdateInfo.updateInterval)
            {
                bool isPlayerQuery = objectInfo.guid.GetType() == GameDefine::ObjectGuid::Type::Player;
                auto& transform = registry.get<Components::Transform>(entity);
                auto& visibilityInfo = registry.get<Components::VisibilityInfo>(entity);

                u32 mapID = 0; // TODO : Get from transform
                World& world = worldState.GetWorld(mapID);

                robin_hood::unordered_set<GameDefine::ObjectGuid> prevVisiblePlayers = visibilityInfo.visiblePlayers;

                if (isPlayerQuery)
                {
                    auto& netInfo = registry.get<Components::NetInfo>(entity);
                    if (!networkState.activeSocketIDs.contains(netInfo.socketID))
                        return;

                    world.GetPlayersInRadius(transform.position.x, transform.position.z, World::DEFAULT_VISIBILITY_DISTANCE, [&](const GameDefine::ObjectGuid guid) -> bool
                    {
                        if (objectInfo.guid == guid)
                            return true;

                        if (prevVisiblePlayers.contains(guid))
                        {
                            prevVisiblePlayers.erase(guid);
                            return true;
                        }

                        visibilityInfo.visiblePlayers.insert(guid);

                        entt::entity otherEntity = world.GetEntity(guid);
                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                        auto& otherObjectInfo = registry.get<Components::ObjectInfo>(otherEntity);
                        if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, otherEntity, otherObjectInfo.guid))
                            return true;

                        networkState.server->SendPacket(netInfo.socketID, buffer);
                        return true;
                    });

                    for (GameDefine::ObjectGuid& prevVisibleGuid : prevVisiblePlayers)
                    {
                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                        if (!Util::MessageBuilder::Entity::BuildEntityDestroyMessage(buffer, prevVisibleGuid))
                            continue;

                        networkState.server->SendPacket(netInfo.socketID, buffer);

                        visibilityInfo.visiblePlayers.erase(prevVisibleGuid);
                    }
                }
                else
                {
                    robin_hood::unordered_set<GameDefine::ObjectGuid> playersEnteredInView;
                    playersEnteredInView.reserve(prevVisiblePlayers.size());
                    
                    world.GetPlayersInRadius(transform.position.x, transform.position.z, World::DEFAULT_VISIBILITY_DISTANCE, [&](const GameDefine::ObjectGuid guid) -> bool
                    {
                        if (prevVisiblePlayers.contains(guid))
                        {
                            prevVisiblePlayers.erase(guid);
                            return true;
                        }

                        visibilityInfo.visiblePlayers.insert(guid);
                        playersEnteredInView.insert(guid);
                        return true;
                    });

                    if (playersEnteredInView.size() > 0)
                    {
                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                        if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, entity, objectInfo.guid))
                            return;

                        for (GameDefine::ObjectGuid playerGuid : playersEnteredInView)
                        {
                            entt::entity playerEntity = world.GetEntity(playerGuid);
                            if (playerEntity == entt::null)
                                continue;

                            auto& playerNetInfo = registry.get<Components::NetInfo>(playerEntity);
                            networkState.server->SendPacket(playerNetInfo.socketID, buffer);
                        }
                    }

                    if (prevVisiblePlayers.size() > 0)
                    {
                        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                        if (!Util::MessageBuilder::Entity::BuildEntityDestroyMessage(buffer, objectInfo.guid))
                            return;

                        for (GameDefine::ObjectGuid playerGuid : prevVisiblePlayers)
                        {
                            entt::entity playerEntity = world.GetEntity(playerGuid);
                            if (playerEntity == entt::null)
                                continue;

                            auto& playerNetInfo = registry.get<Components::NetInfo>(playerEntity);
                            networkState.server->SendPacket(playerNetInfo.socketID, buffer);

                            visibilityInfo.visiblePlayers.erase(playerGuid);
                        }
                    }
                }

                visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
            }
        });
        /*
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

                    auto socketID = networkState.entityToSocketID[entity];
                    auto characterID = networkState.socketIDToCharacterID[socketID];

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<64>();
                    if (!Util::MessageBuilder::Entity::BuildEntityDestroyMessage(buffer, GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID)))
                        continue;

                    updates.push_back({ entity, {}, std::move(buffer) });
                }

                // Send Destroy Message to all players in the cell
                for (entt::entity entity : gridSingleton.cell.players.list)
                {
                    Network::SocketID socketID = networkState.entityToSocketID[entity];

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

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();

                    auto socketID = networkState.entityToSocketID[entity];
                    auto characterID = networkState.socketIDToCharacterID[socketID];

                    if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, entity, GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID)))
                        continue;

                    listUpdates.push_back({ entity, {}, std::move(buffer) });
                }
            }

            // Create Packets for all entering players
            for (entt::entity entity : gridSingleton.cell.players.entering)
            {
                if (!registry.valid(entity))
                    continue;

                if (!networkState.entityToSocketID.contains(entity))
                    continue;

                auto socketID = networkState.entityToSocketID[entity];
                auto characterID = networkState.socketIDToCharacterID[socketID];

                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();
                if (!Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, entity, GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID)))
                    continue;

                enterUpdates.push_back({ entity, {}, std::move(buffer) });

                gridSingleton.cell.players.list.insert(entity);
            }

            // Send Create Messages to all players in the cell
            for (entt::entity entity : gridSingleton.cell.players.list)
            {
                Network::SocketID socketID = networkState.entityToSocketID[entity];

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
                Singletons::GridUpdate update;
                while (gridSingleton.cell.updates.try_dequeue(update))
                {
                    for (entt::entity entity : gridSingleton.cell.players.list)
                    {
                        bool shouldSkip = update.owner == entity && !update.flags.SendToSelf;
                        if (shouldSkip)
                            continue;

                        Network::SocketID socketID = networkState.entityToSocketID[entity];

                        networkState.server->SendPacket(socketID, update.buffer);
                    }
                }
            }
            else
            {
                Singletons::GridUpdate update;
                while (gridSingleton.cell.updates.try_dequeue(update)) { }
            }
        }
        */
    }
}