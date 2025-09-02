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

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Meta/Generated/Shared/NetworkPacket.h>

#include <Network/Server.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace ECS::Systems
{
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

        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        CharacterLoginRequest request;
        while (networkState.characterLoginRequest.try_dequeue(request))
        {
            Network::SocketID socketID = request.socketID;
            u32 requestedCharacterNameHash = StringUtils::fnv1a_32(request.name.c_str(), request.name.length());

            bool notConnectedToACharacter = !Util::Network::IsSocketLinkedToEntity(networkState, socketID);
            bool characterIsOffline = !Util::Cache::CharacterExistsByNameHash(gameCache, requestedCharacterNameHash);

            if (notConnectedToACharacter && characterIsOffline)
            {
                pqxx::result databaseResult;
                if (Database::Util::Character::CharacterGetInfoByName(databaseConn, request.name, databaseResult))
                {
                    u64 characterID = databaseResult[0][0].as<u64>();
                    u16 mapID = databaseResult[0][10].as<u16>();

                    if (Util::Cache::MapExistsByID(gameCache, mapID))
                    {
                        worldState.AddWorldTransferRequest(WorldTransferRequest{
                            .socketID = socketID,

                            .characterName = request.name,
                            .characterID = characterID,

                            .targetMapID = mapID
                        });

                        u32 charNameHash = StringUtils::fnv1a_32(request.name.c_str(), request.name.length());
                        gameCache.characterTables.charNameHashToCharID[charNameHash] = characterID;

                        Util::Network::SendPacket(networkState, socketID, Generated::ConnectResultPacket{
                            .result = static_cast<u8>(Network::ConnectResult::Success)
                        });

                        continue;
                    }
                }
            }

            Util::Network::SendPacket(networkState, socketID, Generated::ConnectResultPacket{
                .result = static_cast<u8>(Network::ConnectResult::Failed)
            });

            networkState.server->CloseSocketID(socketID);
        }
    }
}