#include "CharacterLoginHandler.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/Util/UnitUtils.h"

#include <Base/Util/DebugHandler.h>

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
        Singletons::NetworkState& networkState = ctx.get<Singletons::NetworkState>();

        u32 approxRequests = static_cast<u32>(networkState.characterLoginRequest.size_approx());
        if (approxRequests == 0)
            return;

        Singletons::DatabaseState& databaseState = ctx.get<Singletons::DatabaseState>();

        CharacterLoginRequest request;
        while (networkState.characterLoginRequest.try_dequeue(request))
        {
            Network::SocketID socketID = request.socketID;
            u32 requestedCharacterNameHash = request.nameHash;

            bool notConnectedToACharacter = !networkState.socketIDToEntity.contains(socketID);
            bool characterDoesExist = databaseState.characterTables.charNameHashToCharID.contains(requestedCharacterNameHash);
            if (notConnectedToACharacter && characterDoesExist)
            {
                u64 requestedCharacterID = databaseState.characterTables.charNameHashToCharID[requestedCharacterNameHash];
                bool characterIsOffline = !networkState.characterIDToSocketID.contains(requestedCharacterID);

                if (characterIsOffline)
                {
                    entt::entity socketEntity = registry.create();

                    auto& netInfo = registry.emplace<Components::NetInfo>(socketEntity);
                    netInfo.socketID = socketID;

                    auto& objectInfo = registry.emplace<Components::ObjectInfo>(socketEntity);
                    objectInfo.guid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Player, requestedCharacterID);

                    networkState.socketIDToEntity[socketID] = socketEntity;
                    networkState.characterIDToEntity[requestedCharacterID] = socketEntity;
                    networkState.characterIDToSocketID[requestedCharacterID] = socketID;

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<16>();
                    if (!Util::MessageBuilder::Authentication::BuildConnectedMessage(buffer, Network::ConnectResult::Success))
                    {
                        NC_LOG_ERROR("Failed to build character login message for character: {0}", requestedCharacterID);

                        networkState.server->CloseSocketID(socketID);
                        continue;
                    }

                    networkState.server->SendPacket(socketID, buffer);
                    registry.emplace<Tags::CharacterNeedsInitialization>(socketEntity);
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