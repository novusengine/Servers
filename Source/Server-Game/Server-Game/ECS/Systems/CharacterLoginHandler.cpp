#include "CharacterLoginHandler.h"

#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GridSingleton.h"
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
        Singletons::GridSingleton& gridSingleton = ctx.get<Singletons::GridSingleton>();

        CharacterLoginRequest request;
        while (networkState.characterLoginRequest.try_dequeue(request))
        {
            Network::SocketID socketID = request.socketID;
            u32 characterNameHash = request.nameHash;
            u64 characterID = std::numeric_limits<u64>::max();

            bool characterDoesNotExist = !databaseState.characterTables.nameHashToID.contains(characterNameHash);
            bool alreadyConnectedToACharacter = networkState.socketIDToCharacterID.contains(socketID);
            bool characterInUse = false;

            if (!characterDoesNotExist && !alreadyConnectedToACharacter)
            {
                characterID = databaseState.characterTables.nameHashToID[characterNameHash];
                characterInUse = networkState.characterIDToSocketID.contains(characterID);
            }

            if (characterDoesNotExist || alreadyConnectedToACharacter || characterInUse)
            {
                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<16>();
                if (Util::MessageBuilder::Authentication::BuildConnectedMessage(buffer, Network::ConnectResult::Failed))
                {
                    networkState.server->SendPacket(socketID, buffer);
                }

                networkState.server->CloseSocketID(socketID);
                continue;
            }

            entt::entity socketEntity = registry.create();
            
            networkState.socketIDToCharacterID[socketID] = characterID;
            networkState.socketIDToEntity[socketID] = socketEntity;
            networkState.characterIDToSocketID[characterID] = socketID;
            networkState.entityToSocketID[socketEntity] = socketID;

            Database::CharacterDefinition& characterDefinition = databaseState.characterTables.idToDefinition[characterID];

            Components::Transform& transform = registry.emplace<Components::Transform>(socketEntity);
            transform.position = characterDefinition.position;
            transform.rotation = quat(vec3(0.0f, characterDefinition.orientation, 0.0f));

            Components::DisplayInfo& displayInfo = registry.emplace<Components::DisplayInfo>(socketEntity);
            displayInfo.race = characterDefinition.GetRace();
            displayInfo.gender = characterDefinition.GetGender();
            displayInfo.displayID = UnitUtils::GetDisplayIDFromRaceGender(displayInfo.race, displayInfo.gender);

            UnitUtils::AddStatsComponent(registry, socketEntity);

            Components::TargetInfo& targetInfo = registry.emplace<Components::TargetInfo>(socketEntity);
            targetInfo.target = entt::null;

           registry.emplace<Components::NetInfo>(socketEntity);

            std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();

            bool failed = false;
            failed |= !Util::MessageBuilder::Authentication::BuildConnectedMessage(buffer, Network::ConnectResult::Success);
            failed |= !Util::MessageBuilder::Unit::BuildUnitCreate(buffer, registry, socketEntity);
            failed |= !Util::MessageBuilder::Entity::BuildSetMoverMessage(buffer, socketEntity);

            if (failed)
            {
                NC_LOG_ERROR("Failed to build character login message for characterID: {0}", characterID);

                networkState.server->CloseSocketID(socketID);
                continue;
            }

            gridSingleton.cell.players.entering.insert(socketEntity);

            networkState.server->SendPacket(socketID, buffer);
        }
    }
}