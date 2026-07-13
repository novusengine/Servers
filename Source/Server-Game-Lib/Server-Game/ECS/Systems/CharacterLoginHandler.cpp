#include "CharacterLoginHandler.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Shared/Packet/Packet.h>

#include <Network/Server.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace ECS::Systems
{
    namespace
    {
        constexpr u32 MaxDatabaseRequestsPerUpdate = 64;
    }

    void CharacterLoginHandler::Init(entt::registry& registry)
    {
    }

    void CharacterLoginHandler::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        u32 approxRequests = static_cast<u32>(networkState.characterLoginRequest.size_approx());
        if (approxRequests == 0)
            return;

        CharacterLoginRequest request;
        u32 processedRequests = 0;
        while (processedRequests++ < MaxDatabaseRequestsPerUpdate && networkState.characterLoginRequest.try_dequeue(request))
        {
            Network::SocketID socketID = request.socketID;
            bool notConnectedToACharacter = true;// !Util::Network::IsSocketLinkedToEntity(networkState, socketID);
            bool characterIsOffline = !Util::Cache::CharacterExistsByName(gameCache, request.name);

            if (notConnectedToACharacter && characterIsOffline)
            {
                auto characterResult = gameCache.database->GetCharacterByName(request.name);
                if (characterResult && characterResult.Value())
                {
                    const auto& character = *characterResult.Value();
                    u64 characterID = character.id;
                    u32 mapID = character.mapId;

                    if (Util::Cache::MapExistsByID(gameCache, mapID))
                    {
                        worldState.AddWorldTransferRequest(WorldTransferRequest{
                            .socketID = socketID,

                            .characterName = request.name,
                            .characterID = characterID,

                            .targetMapID = mapID
                        });

                        gameCache.characterTables.charNameToCharID[request.name] = characterID;

                        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerConnectResultPacket{
                            .result = static_cast<u8>(Network::ConnectResult::Success)
                        });

                        continue;
                    }
                }
            }

            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerConnectResultPacket{
                .result = static_cast<u8>(Network::ConnectResult::Failed)
            });

            networkState.server->CloseSocketID(socketID);
        }
    }
}
