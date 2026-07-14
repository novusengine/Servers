#include "AccountLoginHandler.h"

#include "Server-Game/ECS/Components/AccountInfo.h"
#include "Server-Game/ECS/Components/AuthenticationInfo.h"
#include "Server-Game/ECS/Components/CharacterListInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PermissionInfo.h"
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
#include "Server-Game/ECS/Util/PermissionUtil.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/PacketList.h>
#include <MetaGen/Shared/Packet/Packet.h>

#include <Network/Server.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <spake2-ee/crypto_spake.h>

namespace ECS::Systems
{
    namespace
    {
        constexpr u32 MaxDatabaseRequestsPerQueuePerUpdate = 64;
    }

    void AccountLoginHandler::Init(entt::registry& registry)
    {
    }

    void AccountLoginHandler::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        AccountLoginRequest request;
        u32 processedRequests = 0;
        while (processedRequests++ < MaxDatabaseRequestsPerQueuePerUpdate && networkState.accountLoginRequest.try_dequeue(request))
        {
            Network::SocketID socketID = request.socketID;

            auto accountResult = gameCache.database->GetAccountByName(request.name);
            if (accountResult && accountResult.Value())
            {
                auto& account = *accountResult.Value();
                auto accountID = account.id;

                Network::SocketID linkedSocketID;
                bool isAccountLoggedIn = Util::Network::GetSocketIDFromAccountID(networkState, accountID, linkedSocketID);

                // TODO : Implement disconnecting previous session if already logged in
                if (!isAccountLoggedIn)
                {
                    auto permissionsResult = gameCache.database->GetAccountPermissions(accountID);
                    if (!permissionsResult)
                    {
                        networkState.server->CloseSocketID(socketID);
                        continue;
                    }

                    if (!account.blob || account.blob->writtenData != crypto_spake_STOREDBYTES)
                    {
                        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerConnectResultPacket{
                            .result = static_cast<u8>(Network::ConnectResult::Failed)
                        });

                        networkState.server->CloseSocketID(socketID);
                        continue;
                    }

                    entt::entity accountEntity = registry.create();
                    Util::Network::LinkSocketToAccount(networkState, socketID, accountID, accountEntity);

                    auto& accountInfo = registry.emplace<Components::AccountInfo>(accountEntity);
                    accountInfo.id = accountID;
                    accountInfo.name = request.name;
                    accountInfo.email = std::move(account.email);
                    accountInfo.flags = account.flags;
                    accountInfo.creationTimestamp = account.registrationTimestamp;
                    accountInfo.lastLoginTimestamp = account.lastLoginTimestamp;

                    auto& permissionInfo = registry.emplace<Components::AccountPermissionInfo>(accountEntity);
                    Util::Permission::Merge(permissionInfo.permissions, gameCache.permissionTables, permissionsResult.Value());

                    auto& authInfo = registry.emplace<Components::AuthenticationInfo>(accountEntity);
                    authInfo.state = AuthenticationState::Step1;
                    memcpy(authInfo.blob, account.blob->GetDataPointer(), crypto_spake_STOREDBYTES);

                    unsigned char public_data[crypto_spake_PUBLICDATABYTES];
                    i32 result = crypto_spake_step0(&authInfo.serverState, public_data, authInfo.blob);
                    if (result != 0)
                    {
                        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerConnectResultPacket{
                            .result = static_cast<u8>(Network::ConnectResult::Failed)
                        });

                        networkState.server->CloseSocketID(socketID);
                        continue;
                    }

                    // Send Public Data Packet
                    {
                        MetaGen::Shared::Packet::ServerAuthChallengePacket challenge;
                        memcpy(challenge.challenge.data(), public_data, crypto_spake_PUBLICDATABYTES);

                        Util::Network::SendPacket(networkState, socketID, challenge);
                    }

                    continue;
                }
            }

            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerConnectResultPacket{
                .result = static_cast<u8>(Network::ConnectResult::Failed)
            });

            networkState.server->CloseSocketID(socketID);
        }

        CharacterListRequest charListRequest;
        processedRequests = 0;
        while (processedRequests++ < MaxDatabaseRequestsPerQueuePerUpdate && networkState.characterListRequest.try_dequeue(charListRequest))
        {
            Network::SocketID socketID = charListRequest.socketID;
            entt::entity entity = charListRequest.socketEntity;
            if (!Util::Network::IsSocketActive(networkState, socketID) || !registry.valid(entity))
                continue;

            auto& accountInfo = registry.get<Components::AccountInfo>(entity);
            auto& characterListInfo = registry.get_or_emplace<Components::CharacterListInfo>(entity);
            characterListInfo.list.clear();

            auto charactersResult = gameCache.database->GetCharactersByAccount(accountInfo.id);
            if (!charactersResult)
            {
                networkState.server->CloseSocketID(socketID);
                continue;
            }
            auto& characters = charactersResult.Value();

            PacketWriter writer = networkState.packetArena.Acquire(1024);
            if (!writer.IsValid())
            {
                networkState.server->CloseSocketID(socketID);
                continue;
            }

            Bytebuffer& buffer = writer.GetBuffer();
            bool result = Util::MessageBuilder::CreatePacket(buffer, (Network::OpcodeType)MetaGen::PacketListEnum::ServerCharacterListPacket, [&characters, &characterListInfo, &buffer]() -> bool
            {
                u32 numCharacters = static_cast<u32>(characters.size());
                characterListInfo.list.reserve(numCharacters);

                if (!buffer.PutU8(static_cast<u8>(numCharacters)))
                    return false;

                for (const auto& character : characters)
                {
                    CharacterListEntry& entry = characterListInfo.list.emplace_back();
                    entry.name = character.name;
                    entry.level = character.level;
                    entry.mapID = character.mapId;

                    u16 raceGenderClass = character.raceGenderClass;
                    u8 race = raceGenderClass & 0x7F;
                    u8 gender = (raceGenderClass >> 7) & 0x3;
                    u8 unitClass = (raceGenderClass >> 9) & 0x1FF;

                    entry.race = race;
                    entry.gender = gender;
                    entry.unitClass = unitClass;

                    if (!buffer.PutString(entry.name) ||
                        !buffer.PutU8(entry.race) ||
                        !buffer.PutU8(entry.gender) ||
                        !buffer.PutU8(entry.unitClass) ||
                        !buffer.PutU16(entry.level) ||
                        !buffer.PutU32(entry.mapID))
                    {
                        return false;
                    }
                }

                return true;
            });

            if (!result)
            {
                networkState.server->CloseSocketID(socketID);
                continue;
            }

            Util::Network::SendPacket(networkState, socketID, writer.Seal());
        }
    }
}
