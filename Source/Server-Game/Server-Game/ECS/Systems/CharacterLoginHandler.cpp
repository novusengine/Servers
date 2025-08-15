#include "CharacterLoginHandler.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

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
                    entt::entity socketEntity = registry.create();

                    auto& netInfo = registry.emplace<Components::NetInfo>(socketEntity);
                    netInfo.socketID = socketID;

                    auto& objectInfo = registry.emplace<Components::ObjectInfo>(socketEntity);

                    u64 characterID = databaseResult[0][0].as<u64>();
                    objectInfo.guid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, characterID);

                    networkState.socketIDToEntity[socketID] = socketEntity;
                    networkState.characterIDToEntity[characterID] = socketEntity;
                    networkState.characterIDToSocketID[characterID] = socketID;

                    Util::Cache::CharacterCreate(gameCache, characterID, request.name, socketEntity);

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<16>();
                    if (Util::MessageBuilder::Authentication::BuildConnectedMessage(buffer, Network::ConnectResult::Success))
                    {
                        networkState.server->SendPacket(socketID, buffer);
                        registry.emplace<Tags::CharacterNeedsInitialization>(socketEntity);
                    }

                    continue;
                }
            }

            std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<16>();
            if (Util::MessageBuilder::Authentication::BuildConnectedMessage(buffer, Network::ConnectResult::Failed))
            {
                networkState.server->SendPacket(socketID, buffer);
            }

            networkState.server->CloseSocketID(socketID);
        }
    }
}