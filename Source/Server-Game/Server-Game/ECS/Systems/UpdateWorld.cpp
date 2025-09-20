#include "UpdateWorld.h"
#include "World/UpdateCharacter.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/CharacterSpellCastInfo.h"
#include "Server-Game/ECS/Components/CreatureAIInfo.h"
#include "Server-Game/ECS/Components/CreatureInfo.h"
#include "Server-Game/ECS/Components/CreatureThreatTable.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Components/SpellInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitSpellCooldownHistory.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/UnitVisualItems.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Components/VisibilityUpdateInfo.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"
#include "Server-Game/ECS/Singletons/CreatureAIState.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/ProximityTriggers.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatUtil.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/ProximityTriggerUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/ECS/Util/Persistence/CharacterUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/CreatureUtils.h>
#include <Server-Common/Database/Util/ProximityTriggerUtils.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <Meta/Generated/Server/LuaEvent.h>
#include <Meta/Generated/Shared/NetworkPacket.h>

#include <Network/Server.h>

#include <Scripting/LuaManager.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void UpdateWorld::Init(entt::registry& registry)
    {
        auto& worldState = registry.ctx().emplace<Singletons::WorldState>();
    }

    bool UpdateWorld::HandleMapInitialization(World& world, Singletons::GameCache& gameCache)
    {
        if (!world.ContainsSingleton<Events::MapNeedsInitialization>())
            return false;

        // Handle Map Initialization
        world.EmplaceSingleton<Singletons::CombatEventState>();
        world.EmplaceSingleton<Singletons::CreatureAIState>();
        world.EmplaceSingleton<Singletons::ProximityTriggers>();

        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        pqxx::result databaseResult;
        if (Database::Util::Creature::CreatureGetInfoByMap(databaseConn, world.mapID, databaseResult))
        {
            databaseResult.for_each([&world, &gameCache](u64 id, u32 templateID, u32 displayID, u16 mapID, f32 posX, f32 posY, f32 posZ, f32 posO, const std::string& scriptName)
            {
                GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
                if (!Util::Cache::GetCreatureTemplateByID(gameCache, templateID, creatureTemplate))
                    return;

                entt::entity creatureEntity = world.CreateEntity();

                Events::CreatureNeedsInitialization event =
                {
                    .guid = ObjectGUID::CreateCreature(id),
                    .templateID = templateID,
                    .displayID = displayID,
                    .mapID = mapID,
                    .position = vec3(posX, posY, posZ),
                    .scale = vec3(creatureTemplate->scale),
                    .orientation = posO,
                    .scriptName = scriptName
                };

                world.Emplace<Events::CreatureNeedsInitialization>(creatureEntity, event);
            });
        }

        if (Database::Util::ProximityTrigger::ProximityTriggerGetInfoByMap(databaseConn, world.mapID, databaseResult))
        {
            databaseResult.for_each([&world, &gameCache](u32 id, const std::string& name, u16 flags, u16 mapID, f32 posX, f32 posY, f32 posZ, f32 extentX, f32 extentY, f32 extentZ)
            {
                entt::entity triggerEntity = world.CreateEntity();

                Events::ProximityTriggerNeedsInitialization event =
                {
                    .triggerID = id,

                    .name = name,
                    .flags = static_cast<Generated::ProximityTriggerFlagEnum>(flags),

                    .mapID = mapID,
                    .position = vec3(posX, posY, posZ),
                    .extents = vec3(extentX, extentY, extentZ)
                };

                world.Emplace<Events::ProximityTriggerNeedsInitialization>(triggerEntity, event);
            });
        }

        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        world.zenithKey = Scripting::ZenithInfoKey::Make(world.mapID, 0, 0);
        luaManager->GetZenithStateManager().Add(world.zenithKey);
        // Stress Test - Spawn a bunch of creatures
        //for (u32 i = 0; i < 999; i++)
        //{
        //    auto guid = ObjectGUID(ObjectGUID::Type::Creature, 500 + i);
        //    u64 characterID = guid.GetCounter();
        //
        //    entt::entity entity = world.CreateEntity();
        //
        //    auto& objectInfo = world.Emplace<Components::ObjectInfo>(entity);
        //    objectInfo.guid = guid;
        //
        //    auto& creatureInfo = world.Emplace<Components::CreatureInfo>(entity);
        //    creatureInfo.name = "Test";
        //    creatureInfo.templateID = 2;
        //
        //    Components::Transform& transform = world.Emplace<Components::Transform>(entity);
        //    u32 col = i % 50;
        //    u32 row = i / 50;
        //
        //    transform.position = vec3(-1200.0f + 1.5f * col, -56.6f + 0.1f * row, 1165.0f + 1.5f * row);
        //    transform.pitchYaw = vec2(0.0f);
        //
        //    world.Emplace<Components::VisibilityInfo>(entity);
        //    auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
        //    visibilityUpdateInfo.updateInterval = 0.25f;
        //    visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;
        //
        //    Components::DisplayInfo& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
        //    Util::Unit::UpdateDisplayRaceGender(*world.registry, entity, displayInfo, GameDefine::UnitRace::NightElf, GameDefine::UnitGender::Male);
        //    Util::Unit::AddStatsComponent(*world.registry, entity);
        //
        //    world.AddEntity(objectInfo.guid, entity, vec2(transform.position.x, transform.position.z));
        //}
        
        world.EraseSingleton<Events::MapNeedsInitialization>();
        return true;
    }

    void UpdateWorld::HandleCreatureDeinitialization(World& world, Singletons::GameCache& gameCache)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        auto view = world.View<Events::CreatureNeedsDeinitialization>();
        view.each([&](entt::entity entity, Events::CreatureNeedsDeinitialization& event)
        {
            entt::entity creatureEntity = world.GetEntity(event.guid);
            if (creatureEntity == entt::null)
                return;

            world.RemoveEntity(event.guid);
            world.DestroyEntity(creatureEntity);

            auto transaction = databaseConn->NewTransaction();
            if (!Database::Util::Creature::CreatureDelete(transaction, event.guid.GetCounter()))
                return;

            transaction.commit();
        });
        world.Clear<Events::CreatureNeedsDeinitialization>();
    }
    void UpdateWorld::HandleCreatureInitialization(World& world, Singletons::GameCache& gameCache)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);

        auto createView = world.View<Events::CreatureCreate>();
        createView.each([&](entt::entity entity, Events::CreatureCreate& event)
        {
            GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
                return;

            auto transaction = databaseConn->NewTransaction();

            u64 creatureID = 0;
            if (!Database::Util::Creature::CreatureCreate(transaction, event.templateID, event.displayID, event.mapID, event.position, event.orientation, event.scriptName, creatureID))
                return;

            transaction.commit();

            Events::CreatureNeedsInitialization initEvent =
            {
                .guid = ObjectGUID::CreateCreature(creatureID),
                .templateID = event.templateID,
                .displayID = 0,
                .mapID = event.mapID,
                .position = event.position,
                .scale = event.scale,
                .orientation = event.orientation,
                .scriptName = event.scriptName
            };

            world.Emplace<Events::CreatureNeedsInitialization>(entity, initEvent);
        });
        world.Clear<Events::CreatureCreate>();

        auto initView = world.View<Events::CreatureNeedsInitialization>();
        initView.each([&](entt::entity entity, Events::CreatureNeedsInitialization& event)
        {
            GameDefine::Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
                return;

            world.Emplace<Tags::IsCreature>(entity);
            auto& objectInfo = world.Emplace<Components::ObjectInfo>(entity);
            objectInfo.guid = event.guid;

            auto& creatureInfo = world.Emplace<Components::CreatureInfo>(entity);
            creatureInfo.templateID = event.templateID;
            creatureInfo.name = creatureTemplate->name;

            auto& creatureTransform = world.Emplace<Components::Transform>(entity);
            creatureTransform.mapID = event.mapID;
            creatureTransform.position = event.position;
            creatureTransform.pitchYaw = vec2(0.0f, event.orientation);
            creatureTransform.scale = event.scale;

            auto& visualItems = world.Emplace<Components::UnitVisualItems>(entity);
            auto& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
            displayInfo.displayID = creatureTemplate->displayID;

            Components::UnitPowersComponent& unitPowersComponent = Util::Unit::AddPowersComponent(world, entity);
            {
                UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, Generated::PowerTypeEnum::Health);
                healthPower.base *= creatureTemplate->healthMod;
                healthPower.current *= creatureTemplate->healthMod;
                healthPower.max *= creatureTemplate->healthMod;

                UnitPower& manaPower = Util::Unit::GetPower(unitPowersComponent, Generated::PowerTypeEnum::Mana);
                manaPower.base *= creatureTemplate->resourceMod;
                manaPower.current *= creatureTemplate->resourceMod;
                manaPower.max *= creatureTemplate->resourceMod;
            }

            Util::Unit::AddResistancesComponent(world, entity);
            Util::Unit::AddStatsComponent(world, entity);

            world.Emplace<Tags::IsAlive>(entity);
            auto& visibilityInfo = world.Emplace<Components::VisibilityInfo>(entity);
            auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
            visibilityUpdateInfo.updateInterval = 0.5f;

            world.Emplace<Components::UnitSpellCooldownHistory>(entity);
            world.Emplace<Components::UnitCombatInfo>(entity);
            Components::TargetInfo& targetInfo = world.Emplace<Components::TargetInfo>(entity);
            targetInfo.target = entt::null;

            world.Emplace<Components::CreatureThreatTable>(entity);
            if (!event.scriptName.empty())
            {
                world.Emplace<Events::CreatureAddScript>(entity, Events::CreatureAddScript
                {
                    .scriptName = event.scriptName
                });
            }

            world.AddEntity(objectInfo.guid, entity, vec2(creatureTransform.position.x, creatureTransform.position.z));
        });
        world.Clear<Events::CreatureNeedsInitialization>();
    }

    void UpdateWorld::HandleCreatureUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        HandleCreatureDeinitialization(world, gameCache);
        HandleCreatureInitialization(world, gameCache);

        auto& creatureAIState = world.GetSingleton<Singletons::CreatureAIState>();

        auto removeScriptView = world.View<const Components::ObjectInfo, Events::CreatureRemoveScript>();
        removeScriptView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo)
        {
            zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnDeinit, Generated::LuaCreatureAIEventDataOnDeinit{
                .creatureEntity = entt::to_integral(entity)
            });
            
            world.Remove<Components::CreatureAIInfo>(entity);
            creatureAIState.creatureGUIDToScriptNameHash.erase(objectInfo.guid);
        });
        world.Clear<Events::CreatureRemoveScript>();

        auto addScriptView = world.View<Components::ObjectInfo, Events::CreatureAddScript>();
        addScriptView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Events::CreatureAddScript& event)
        {
            u32 scriptNameHash = StringUtils::fnv1a_32(event.scriptName.c_str(), event.scriptName.length());
            if (!creatureAIState.loadedScriptNameHashes.contains(scriptNameHash))
                return;
        
            auto& creatureAIInfo = world.GetOrEmplace<Components::CreatureAIInfo>(entity);
            if (creatureAIInfo.scriptName == event.scriptName)
            {
                NC_LOG_WARNING("Creature {} already has script {} assigned", objectInfo.guid.ToString(), event.scriptName);
                return;
            }
        
            creatureAIInfo.timeToNextUpdate = 0.0f;
            creatureAIInfo.scriptName = event.scriptName;
            creatureAIState.creatureGUIDToScriptNameHash[objectInfo.guid] = scriptNameHash;
        
            zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnInit, Generated::LuaCreatureAIEventDataOnInit{
                .creatureEntity = entt::to_integral(entity),
                .scriptNameHash = scriptNameHash
            });
        });
        world.Clear<Events::CreatureAddScript>();
        
        auto updateAIView = world.View<Components::CreatureAIInfo>();
        updateAIView.each([&, deltaTime](entt::entity entity, Components::CreatureAIInfo& creatureAIInfo)
        {
            creatureAIInfo.timeToNextUpdate -= deltaTime;
            if (creatureAIInfo.timeToNextUpdate > 0.0f)
                return;
        
            creatureAIInfo.timeToNextUpdate += 0.1f;
        
            zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnUpdate, Generated::LuaCreatureAIEventDataOnUpdate{
                .creatureEntity = entt::to_integral(entity),
                .deltaTime = 0.1f
            });
        });

        auto updateThreatTableView = world.View<Components::CreatureThreatTable, Events::CreatureNeedsThreatTableUpdate>();
        updateThreatTableView.each([&](entt::entity entity, Components::CreatureThreatTable& creatureThreatTable)
        {
            Util::Combat::SortThreatTable(creatureThreatTable);
        });
        world.Clear<Events::CreatureNeedsThreatTableUpdate>();
    }

    void UpdateWorld::HandleUnitUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        auto deathView = world.View<Components::UnitCombatInfo, Events::UnitDied, Tags::IsAlive>();
        deathView.each([&](entt::entity entity, Components::UnitCombatInfo& combatInfo, Events::UnitDied& event)
        {
            world.Remove<Tags::IsAlive>(entity);
            world.Emplace<Tags::IsDead>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnDied, Generated::LuaCreatureAIEventDataOnDied{
                    .creatureEntity = entt::to_integral(entity),
                    .killerEntity = entt::to_integral(event.killerEntity)
                });
            }
        });
        world.Clear<Events::UnitDied>();

        auto resurrectView = world.View<Components::UnitCombatInfo, Events::UnitResurrected, Tags::IsDead>();
        resurrectView.each([&](entt::entity entity, Components::UnitCombatInfo& combatInfo, Events::UnitResurrected& event)
        {
            world.Remove<Tags::IsDead>(entity);
            world.Emplace<Tags::IsAlive>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnResurrect, Generated::LuaCreatureAIEventDataOnResurrect{
                    .creatureEntity = entt::to_integral(entity),
                    .resurrectorEntity = entt::to_integral(event.resurrectorEntity)
                });
            }
        });
        world.Clear<Events::UnitResurrected>();
    }

    void UpdateWorld::HandleProximityTriggerDeinitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        auto view = world.View<Components::ProximityTrigger, Events::ProximityTriggerNeedsDeinitialization>();
        view.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Events::ProximityTriggerNeedsDeinitialization& event)
        {
            auto transaction = databaseConn->NewTransaction();
            if (!Database::Util::ProximityTrigger::ProximityTriggerDelete(transaction, event.triggerID))
                return;

            transaction.commit();

            Generated::ServerTriggerRemovePacket triggerRemovePacket =
            {
                .triggerID = event.triggerID
            };

            std::shared_ptr<Bytebuffer> triggerRemoveBuffer = Bytebuffer::BorrowRuntime(sizeof(::Network::MessageHeader) + triggerRemovePacket.GetSerializedSize());
            Util::Network::AppendPacketToBuffer(triggerRemoveBuffer, triggerRemovePacket);

            bool isServerSideOnly = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) == Generated::ProximityTriggerFlagEnum::IsServerSideOnly;

            for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
            {
                if (isServerSideOnly && Util::ProximityTrigger::IsPlayerInTrigger(proximityTrigger, playerEntity))
                {
                    Util::ProximityTrigger::OnExit(world, proximityTrigger, playerEntity);
                }

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                    continue;

                Util::Network::SendPacket(networkState, socketID, triggerRemoveBuffer);
            }

            world.DestroyEntity(entity);
            Util::ProximityTrigger::RemoveTriggerFromWorld(proximityTriggers, event.triggerID, isServerSideOnly);
        });
        world.Clear<Events::CreatureNeedsDeinitialization>();
    }
    void UpdateWorld::HandleProximityTriggerInitialization(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        {
            auto transaction = databaseConn->NewTransaction();

            auto createView = world.View<Events::ProximityTriggerCreate>();
            createView.each([&](entt::entity entity, Events::ProximityTriggerCreate& event)
            {
                u32 triggerID;
                if (!Database::Util::ProximityTrigger::ProximityTriggerCreate(transaction, event.name, static_cast<u16>(event.flags), event.mapID, event.position, event.extents, triggerID))
                    return;

                Events::ProximityTriggerNeedsInitialization initEvent =
                {
                    .triggerID = triggerID,

                    .name = event.name,
                    .flags = event.flags,

                    .mapID = event.mapID,
                    .position = event.position,
                    .extents = event.extents
                };

                world.Emplace<Events::ProximityTriggerNeedsInitialization>(entity, initEvent);
            });

            transaction.commit();
            world.Clear<Events::ProximityTriggerCreate>();
        }

        auto view = world.View<Events::ProximityTriggerNeedsInitialization>();
        view.each([&](entt::entity entity, Events::ProximityTriggerNeedsInitialization& event)
        {
            auto& proximityTrigger = world.Emplace<Components::ProximityTrigger>(entity);
            proximityTrigger.triggerID = event.triggerID;
            proximityTrigger.name = event.name;
            proximityTrigger.flags = event.flags;

            auto& transform = world.Emplace<Components::Transform>(entity);
            transform.mapID = event.mapID;
            transform.position = event.position;

            auto& aabb = world.Emplace<Components::AABB>(entity);
            aabb.centerPos = vec3(0, 0, 0);
            aabb.extents = event.extents;

            auto& worldAABB = world.Emplace<Components::WorldAABB>(entity);
            worldAABB.min = transform.position + aabb.centerPos - aabb.extents;
            worldAABB.max = transform.position + aabb.centerPos + aabb.extents;

            bool isServerSideOnly = (proximityTrigger.flags & Generated::ProximityTriggerFlagEnum::IsServerSideOnly) != Generated::ProximityTriggerFlagEnum::None;
            if (isServerSideOnly)
            {
                world.Emplace<Tags::ProximityTriggerIsServerSideOnly>(entity);
            }
            else
            {
                world.Emplace<Tags::ProximityTriggerIsClientSide>(entity);

                Generated::ServerTriggerAddPacket triggerAddPacket =
                {
                    .triggerID = event.triggerID,

                    .name = event.name,
                    .flags = static_cast<u8>(event.flags),

                    .mapID = event.mapID,
                    .position = event.position,
                    .extents = event.extents
                };

                std::shared_ptr<Bytebuffer> triggerAddBuffer = Bytebuffer::BorrowRuntime(sizeof(::Network::MessageHeader) + triggerAddPacket.GetSerializedSize());
                Util::Network::AppendPacketToBuffer(triggerAddBuffer, triggerAddPacket);

                for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
                {
                    Network::SocketID socketID;
                    if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                        continue;

                    Util::Network::SendPacket(networkState, socketID, triggerAddBuffer);
                }
            }

            Util::ProximityTrigger::AddTriggerToWorld(proximityTriggers, entity, event.triggerID, worldAABB, isServerSideOnly);
        });
        world.Clear<Events::ProximityTriggerNeedsInitialization>();
    }
    void UpdateWorld::HandleProximityTriggerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        HandleProximityTriggerDeinitialization(world, gameCache, networkState);
        HandleProximityTriggerInitialization(world, gameCache, networkState);

        robin_hood::unordered_set<entt::entity> playersToRemove;
        playersToRemove.reserve(1024);

        auto serverSideAuthoritativeUpdateView = world.View<Components::ProximityTrigger, Components::Transform, Components::WorldAABB, Tags::ProximityTriggerHasEnteredPlayers>();
        serverSideAuthoritativeUpdateView.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Components::Transform& transform, Components::WorldAABB& worldAABB)
        {
            for (auto playerEntity : proximityTrigger.playersEntered)
            {
                if (!world.ValidEntity(playerEntity))
                    continue;

                Util::ProximityTrigger::OnEnter(world, proximityTrigger, playerEntity);
            }

            proximityTrigger.playersEntered.clear();
        });
        world.Clear<Tags::ProximityTriggerHasEnteredPlayers>();

        auto serverSideOnlyUpdateView = world.View<Components::ProximityTrigger, Components::Transform, Components::WorldAABB, Tags::ProximityTriggerIsServerSideOnly>();
        serverSideOnlyUpdateView.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Components::Transform& transform, Components::WorldAABB& worldAABB)
        {
            playersToRemove = proximityTrigger.playersInside;

            vec4 minMaxRect = vec4(
                worldAABB.min.x,
                worldAABB.min.z,
                worldAABB.max.x,
                worldAABB.max.z
            );

            world.GetPlayersInRect(minMaxRect, [&](const ObjectGUID& guid) -> bool
            {
                entt::entity playerEntity = world.GetEntity(guid);

                if (Util::ProximityTrigger::IsPlayerInTrigger(proximityTrigger, playerEntity))
                {
                    Util::ProximityTrigger::OnStay(world, proximityTrigger, playerEntity);
                    playersToRemove.erase(playerEntity);
                }
                else
                {
                    Util::ProximityTrigger::AddPlayerToTrigger(proximityTrigger, playerEntity);
                    Util::ProximityTrigger::OnEnter(world, proximityTrigger, playerEntity);
                }

                return true;
            });

            // Loop over players that have exited the trigger and call OnExit
            for (auto playerEntity : playersToRemove)
            {
                if (!world.ValidEntity(playerEntity))
                    continue;

                Util::ProximityTrigger::RemovePlayerFromTrigger(proximityTrigger, playerEntity);
                Util::ProximityTrigger::OnExit(world, proximityTrigger, playerEntity);
            }
        });
    }

    void UpdateWorld::HandleReplication(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        robin_hood::unordered_set<ObjectGUID> cachedList;

        auto playerView = world.View<Components::ObjectInfo, Components::NetInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        playerView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            if (!Util::Network::IsSocketActive(networkState, netInfo.socketID))
                return;

            visibilityUpdateInfo.timeSinceLastUpdate += deltaTime;
            if (visibilityUpdateInfo.timeSinceLastUpdate < visibilityUpdateInfo.updateInterval)
                return;

            // Query nearby players
            {
                robin_hood::unordered_set<ObjectGUID>& visiblePlayers = visibilityInfo.visiblePlayers;
                cachedList = visiblePlayers;

                world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                {
                    if (objectInfo.guid == guid)
                        return true;

                    if (cachedList.contains(guid))
                    {
                        cachedList.erase(guid);
                        return true;
                    }

                    visiblePlayers.insert(guid);

                    entt::entity otherEntity = world.GetEntity(guid);
                    auto& otherObjectInfo = world.Get<Components::ObjectInfo>(otherEntity);
                    auto& otherCharacterInfo = world.Get<Components::CharacterInfo>(otherEntity);
                    auto& otherTransform = world.Get<Components::Transform>(otherEntity);

                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                    if (!Util::MessageBuilder::Unit::BuildUnitAdd(buffer, otherObjectInfo.guid, otherCharacterInfo.name, otherTransform.position, otherTransform.scale, otherTransform.pitchYaw))
                        return true;

                    if (!Util::MessageBuilder::Unit::BuildUnitBaseInfo(buffer, *world.registry, otherEntity, otherObjectInfo.guid))
                        return true;

                    networkState.server->SendPacket(netInfo.socketID, buffer);
                    return true;
                });

                for (ObjectGUID& prevVisibleGUID : cachedList)
                {
                    visiblePlayers.erase(prevVisibleGUID);

                    Util::Network::SendPacket(networkState, netInfo.socketID, Generated::UnitRemovePacket{
                        .guid = prevVisibleGUID
                    });
                }
            }

            // Query nearby units
            {
                robin_hood::unordered_set<ObjectGUID>& visibleCreatures = visibilityInfo.visibleCreatures;
                cachedList = visibleCreatures;

                world.GetCreaturesInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                {
                    if (cachedList.contains(guid))
                    {
                        cachedList.erase(guid);
                        return true;
                    }

                    return true;
                });

                if (cachedList.size() > 0)
                {
                    std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<8192>();

                    for (ObjectGUID creatureGUID : cachedList)
                    {
                        visibleCreatures.erase(creatureGUID);

                        if (!Util::Network::AppendPacketToBuffer(buffer, Generated::UnitRemovePacket{
                            .guid = creatureGUID
                        }))
                        {
                            continue;
                        }
                    }

                    if (buffer->writtenData > 0)
                        networkState.server->SendPacket(netInfo.socketID, buffer);
                }
            }

            visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
        });

        robin_hood::unordered_set<ObjectGUID> playersEnteredInView;
        playersEnteredInView.reserve(cachedList.size());

        auto creatureView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        creatureView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            robin_hood::unordered_set<ObjectGUID>& visiblePlayers = visibilityInfo.visiblePlayers;
            cachedList = visiblePlayers;

            playersEnteredInView.clear();

            world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
            {
                if (cachedList.contains(guid))
                {
                    cachedList.erase(guid);
                    return true;
                }

                visiblePlayers.insert(guid);
                playersEnteredInView.insert(guid);
                return true;
            });

            if (playersEnteredInView.size() > 0)
            {
                std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<1024>();
                if (!Util::MessageBuilder::Unit::BuildUnitAdd(buffer, objectInfo.guid, creatureInfo.name, transform.position, transform.scale, transform.pitchYaw))
                    return;

                if (!Util::MessageBuilder::Unit::BuildUnitBaseInfo(buffer, *world.registry, entity, objectInfo.guid))
                    return;

                for (ObjectGUID playerGUID : playersEnteredInView)
                {
                    entt::entity playerEntity = world.GetEntity(playerGUID);
                    if (playerEntity == entt::null)
                        continue;

                    auto& playerVisibilityInfo = world.Get<Components::VisibilityInfo>(playerEntity);
                    if (playerVisibilityInfo.visibleCreatures.contains(objectInfo.guid))
                        continue;

                    if (auto* playerNetInfo = world.TryGet<Components::NetInfo>(playerEntity))
                    {
                        playerVisibilityInfo.visibleCreatures.insert(objectInfo.guid);
                        networkState.server->SendPacket(playerNetInfo->socketID, buffer);
                    }
                }
            }

            if (cachedList.size() > 0)
            {
                for (ObjectGUID playerGUID : cachedList)
                {
                    visiblePlayers.erase(playerGUID);
                }
            }

            visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
        });
    }
    void UpdateWorld::HandleContainerUpdate(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto databaseConn = gameCache.dbController->GetConnection(::Database::DBType::Character);
        std::shared_ptr<Bytebuffer> containerUpdateBuffer = Bytebuffer::Borrow<4096>();
        std::shared_ptr<Bytebuffer> equippedItemsBuffer = Bytebuffer::Borrow<512>();
    
        auto view = world.View<Components::ObjectInfo, Components::NetInfo, Components::VisibilityInfo, Components::PlayerContainers, Events::CharacterNeedsContainerUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::VisibilityInfo& visibilityInfo, Components::PlayerContainers& playerContainers)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            containerUpdateBuffer->Reset();
            equippedItemsBuffer->Reset();
    
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
                    if (!Util::Cache::GetItemInstanceByID(gameCache, equipmentContainerItem.objectGUID.GetCounter(), containerItemInstance))
                        continue;
    
                    u16 numSlots = bag.GetTotalSlots();
                    u16 numFreeSlots = bag.GetFreeSlots();

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddPacket{
                        .index = containerIndex,
                        .itemID = containerItemInstance->itemID,
                        .guid = equipmentContainerItem.objectGUID,
                        .numSlots = numSlots,
                        .numFreeSlots = numFreeSlots
                    });

                    if (numSlots != numFreeSlots)
                    {
                        for (u32 i = 0; i < numSlots; i++)
                        {
                            const Database::ContainerItem& containerItem = bag.GetItem(i);
                            if (containerItem.IsEmpty())
                                continue;

                            bag.SetSlotAsDirty(i);
                        }
                    }
                }

                for (u16 slotIndex : bag.GetDirtySlots())
                {
                    const Database::ContainerItem& containerItem = bag.GetItem(slotIndex);

                    if (containerItem.IsEmpty())
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerRemoveFromSlotPacket{
                            .index = containerIndex,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddToSlotPacket{
                            .index = containerIndex,
                            .slot = slotIndex,
                            .guid = containerItem.objectGUID
                        });
                    }
                }
    
                bag.ClearDirtySlots();
            }
    
            // Base Equipment Container
            {
                if (playerContainers.equipment.IsUninitialized())
                {
                    u16 numSlots = playerContainers.equipment.GetTotalSlots();
                    u16 numFreeSlots = playerContainers.equipment.GetFreeSlots();

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddPacket{
                        .index = PLAYER_BASE_CONTAINER_ID,
                        .itemID = 0,
                        .guid = ObjectGUID::Empty,
                        .numSlots = numSlots,
                        .numFreeSlots = numFreeSlots
                    });

                    if (numSlots != numFreeSlots)
                    {
                        for (u32 i = 0; i < numSlots; i++)
                        {
                            const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(i);
                            if (containerItem.IsEmpty())
                                continue;

                            playerContainers.equipment.SetSlotAsDirty(i);
                        }
                    }
                }

                for (u16 slotIndex : playerContainers.equipment.GetDirtySlots())
                {
                    const Database::ContainerItem& containerItem = playerContainers.equipment.GetItem(slotIndex);
                    bool isContainerItemEmpty = containerItem.IsEmpty();

                    if (isContainerItemEmpty)
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerRemoveFromSlotPacket{
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, Generated::ContainerAddToSlotPacket{
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex,
                            .guid = containerItem.objectGUID
                        });
                    }

                    if (slotIndex >= PLAYER_EQUIPMENT_INDEX_START && slotIndex <= PLAYER_EQUIPMENT_INDEX_END)
                    {
                        u32 itemID = 0;

                        if (isContainerItemEmpty)
                        {
                            Database::ItemInstance* itemInstance = nullptr;
                            if (!Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                                continue;

                            itemID = itemInstance->itemID;
                        }

                        failed |= !Util::Network::AppendPacketToBuffer(equippedItemsBuffer, Generated::ServerUnitEquippedItemUpdatePacket{
                            .guid = objectInfo.guid,
                            .slot = static_cast<u8>(slotIndex),
                            .itemID = itemID
                        });
                    }
                }
    
                playerContainers.equipment.ClearDirtySlots();

                if (equippedItemsBuffer->writtenData > 0)
                {
                    Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, equippedItemsBuffer);
                }
            }
    
            if (failed)
            {
                NC_LOG_ERROR("Failed to build character container update message for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }
    
            networkState.server->SendPacket(netInfo.socketID, containerUpdateBuffer);
        });
    
        world.Clear<Events::CharacterNeedsContainerUpdate>();
    }
    void UpdateWorld::HandleDisplayUpdate(World& world, Singletons::NetworkState& networkState)
    {
        auto view = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::DisplayInfo, Events::CharacterNeedsDisplayUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo, Components::DisplayInfo& displayInfo)
        {
            ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitDisplayInfoUpdatePacket{
                .guid = objectInfo.guid,
                .displayID = displayInfo.displayID,
                .race = static_cast<u8>(displayInfo.unitRace),
                .gender = static_cast<u8>(displayInfo.unitGender)
            });
        });
    
        world.Clear<Events::CharacterNeedsDisplayUpdate>();

        auto visualItemView = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::UnitVisualItems, Events::CharacterNeedsVisualItemUpdate>();
        visualItemView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo, Components::UnitVisualItems& visualItems)
        {
            for (ItemEquipSlot_t slot : visualItems.dirtyItemIDs)
            {
                ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::ServerUnitVisualItemUpdatePacket{
                    .guid = objectInfo.guid,
                    .slot = slot,
                    .itemID = visualItems.equippedItemIDs[slot]
                });
            }

            visualItems.dirtyItemIDs.clear();
        });

        world.Clear<Events::CharacterNeedsVisualItemUpdate>();
    }
    void UpdateWorld::HandleCombatUpdate(World& world, Scripting::Zenith* zenith, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        auto enterCombatView = world.View<Components::ObjectInfo, Components::UnitCombatInfo, Events::UnitEnterCombat>();
        enterCombatView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::UnitCombatInfo& unitCombatInfo, Events::UnitEnterCombat& event)
        {
            world.EmplaceOrReplace<Tags::IsInCombat>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnEnterCombat, Generated::LuaCreatureAIEventDataOnEnterCombat{
                    .creatureEntity = entt::to_integral(entity)
                });
            }
        });
        world.Clear<Events::UnitEnterCombat>();

        auto combatView = world.View<Components::ObjectInfo, Components::UnitCombatInfo, Tags::IsInCombat>();
        combatView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::UnitCombatInfo& unitCombatInfo)
        {
            bool cantDropCombat = false;
            for (auto itr = unitCombatInfo.threatTables.begin(); itr != unitCombatInfo.threatTables.end();)
            {
                ThreatTableEntry& threatTableEntry = itr->second;

                entt::entity threatEntity = world.GetEntity(itr->first);
                if (world.ValidEntity(threatEntity))
                {
                    auto& creatureThreatTable = world.Get<Components::CreatureThreatTable>(threatEntity);
                    if (creatureThreatTable.allowDropCombat)
                    {
                        threatTableEntry.timeSinceLastActivity += deltaTime;
                        if (threatTableEntry.timeSinceLastActivity >= 5.0f)
                        {
                            Util::Combat::RemoveUnitFromThreatTable(creatureThreatTable, objectInfo.guid);
                            itr = unitCombatInfo.threatTables.erase(itr);
                            continue;
                        }
                    }
                }
                else
                {
                    threatTableEntry.timeSinceLastActivity += deltaTime;
                    if (threatTableEntry.timeSinceLastActivity >= 5.0f)
                    {
                        itr = unitCombatInfo.threatTables.erase(itr);
                        continue;
                    }
                }

                itr++;
            }

            if (!unitCombatInfo.threatTables.empty())
                return;

            if (auto* creatureThreatTable = world.TryGet<Components::CreatureThreatTable>(entity))
            {
                if (creatureThreatTable->threatList.size() > 0)
                    return;
            }

            world.Remove<Tags::IsInCombat>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(Generated::LuaCreatureAIEventEnum::OnLeaveCombat, Generated::LuaCreatureAIEventDataOnLeaveCombat{
                    .creatureEntity = entt::to_integral(entity)
                });
            }
        });
    }
    void UpdateWorld::HandlePowerUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        static constexpr f32 HEALTH_REGEN_INTERVAL = 0.5f;

        auto healthUpdateView = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::UnitPowersComponent, Tags::IsMissingHealth, Tags::IsAlive>();
        healthUpdateView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::VisibilityInfo& visibilityInfo, Components::UnitPowersComponent& unitPowersComponent)
        {
            unitPowersComponent.timeToNextUpdate = unitPowersComponent.timeToNextUpdate - deltaTime;
            if (unitPowersComponent.timeToNextUpdate > 0.0f)
                return;

            unitPowersComponent.timeToNextUpdate += HEALTH_REGEN_INTERVAL;

            // Disalllow Health Regen while in combat
            if (world.AllOf<Tags::IsInCombat>(entity))
                return;

            UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, Generated::PowerTypeEnum::Health);
            
            bool isCreature = world.AllOf<Tags::IsCreature>(entity);
            f64 baseRegenRate = isCreature ? (healthPower.base * 0.75) : (healthPower.base * 0.01);
            f64 regenAmount = baseRegenRate * HEALTH_REGEN_INTERVAL;

            f64 newHealth = glm::clamp(healthPower.current + regenAmount, healthPower.current, healthPower.max);
            Util::Unit::SetPower(world, entity, unitPowersComponent, Generated::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

            ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitPowerUpdatePacket{
                .guid = objectInfo.guid,
                .kind = static_cast<u8>(Generated::PowerTypeEnum::Health),
                .base = healthPower.base,
                .current = healthPower.current,
                .max = healthPower.max
            });
        });

        auto dirtyPowerView = world.View<const Components::ObjectInfo, const Components::VisibilityInfo, Components::UnitPowersComponent, Events::UnitNeedsPowerUpdate>();
        dirtyPowerView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::VisibilityInfo& visibilityInfo, Components::UnitPowersComponent& unitPowersComponent)
        {
            for (Generated::PowerTypeEnum dirtyPowerType : unitPowersComponent.dirtyPowerTypes)
            {
                UnitPower& power = Util::Unit::GetPower(unitPowersComponent, dirtyPowerType);

                ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, Generated::UnitPowerUpdatePacket{
                    .guid = objectInfo.guid,
                    .kind = static_cast<u8>(dirtyPowerType),
                    .base = power.base,
                    .current = power.current,
                    .max = power.max
                });
            }

            unitPowersComponent.dirtyPowerTypes.clear();
        });

        auto dirtyResistanceView = world.View<const Components::ObjectInfo, const Components::NetInfo, Components::UnitResistancesComponent, Events::CharacterNeedsResistanceUpdate>();
        dirtyResistanceView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::NetInfo& netInfo, Components::UnitResistancesComponent& unitResistancesComponent)
        {
            for (Generated::ResistanceTypeEnum dirtyResistanceType : unitResistancesComponent.dirtyResistanceTypes)
            {
                UnitResistance& resistance = Util::Unit::GetResistance(unitResistancesComponent, dirtyResistanceType);

                ECS::Util::Network::SendPacket(networkState, netInfo.socketID, Generated::UnitResistanceUpdatePacket{
                    .kind = static_cast<u8>(dirtyResistanceType),
                    .base = resistance.base,
                    .current = resistance.current,
                    .max = resistance.max
                });
            }

            unitResistancesComponent.dirtyResistanceTypes.clear();
        });

        auto dirtyStatsView = world.View<const Components::ObjectInfo, const Components::NetInfo, Components::UnitStatsComponent, Events::CharacterNeedsStatUpdate>();
        dirtyStatsView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::NetInfo& netInfo, Components::UnitStatsComponent& unitStatsComponent)
        {
            for (Generated::StatTypeEnum dirtyStatType : unitStatsComponent.dirtyStatTypes)
            {
                UnitStat& stat = Util::Unit::GetStat(unitStatsComponent, dirtyStatType);

                ECS::Util::Network::SendPacket(networkState, netInfo.socketID, Generated::UnitResistanceUpdatePacket{
                    .kind = static_cast<u8>(dirtyStatType),
                    .base = stat.base,
                    .current = stat.current
                });
            }

            unitStatsComponent.dirtyStatTypes.clear();
        });

        world.Clear<Events::UnitNeedsPowerUpdate>();
        world.Clear<Events::CharacterNeedsResistanceUpdate>();
        world.Clear<Events::CharacterNeedsStatUpdate>();
    }
    void UpdateWorld::HandleSpellUpdate(World& world, Scripting::Zenith* zenith, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        auto prepareView = world.View<Components::SpellInfo, Components::SpellEffectInfo, Tags::IsUnpreparedSpell>();
        prepareView.each([&](entt::entity entity, Components::SpellInfo& spellInfo, Components::SpellEffectInfo& spellEffectInfo)
        {
            GameDefine::Database::Spell* spell = nullptr;
            if (!Util::Cache::GetSpellByID(gameCache, spellInfo.spellID, spell))
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            entt::entity casterEntity = world.GetEntity(spellInfo.caster);
            if (!world.ValidEntity(casterEntity))
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            std::vector<GameDefine::Database::SpellEffect>* spellEffectList = nullptr;
            if (Util::Cache::GetSpellEffectsBySpellID(gameCache, spellInfo.spellID, spellEffectList))
            {
                spellEffectInfo.effects = *spellEffectList;
            }

            // Handle Prepare Spell
            bool prepared = zenith->CallEventBool(Generated::LuaSpellEventEnum::OnPrepare, Generated::LuaSpellEventDataOnPrepare{
                .casterID = entt::to_integral(casterEntity),
                .spellEntity = entt::to_integral(entity),
                .spellID = spellInfo.spellID
            });

            if (!prepared)
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            auto& visibilityInfo = world.Get<Components::VisibilityInfo>(casterEntity);
            ECS::Util::Network::SendToNearby(networkState, world, casterEntity, visibilityInfo, true, Generated::UnitCastSpellPacket{
                .spellID = spellInfo.spellID,
                .guid = spellInfo.caster,
                .castTime = spellInfo.castTime,
                .timeToCast = spellInfo.timeToCast
            });

            world.Emplace<Tags::IsActiveSpell>(entity);
        });
        world.Clear<Tags::IsUnpreparedSpell>();

        auto activeView = world.View<Components::SpellInfo, Components::SpellEffectInfo, Tags::IsActiveSpell>();
        activeView.each([&](entt::entity entity, Components::SpellInfo& spellInfo, Components::SpellEffectInfo& spellEffectInfo)
        {
            spellInfo.timeToCast = glm::max(0.0f, spellInfo.timeToCast - deltaTime);
            if (spellInfo.timeToCast > 0.0f)
                return;

            entt::entity casterEntity = world.GetEntity(spellInfo.caster);
            if (!world.ValidEntity(casterEntity))
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            u8 numEffects = static_cast<u8>(spellEffectInfo.effects.size());
            for (u8 i = 0; i < numEffects; i++)
            {
                const GameDefine::Database::SpellEffect& spellEffect = spellEffectInfo.effects[i];

                zenith->CallEvent(Generated::LuaSpellEventEnum::OnHandleEffect, Generated::LuaSpellEventDataOnHandleEffect{
                    .casterID = entt::to_integral(casterEntity),
                    .spellEntity = entt::to_integral(entity),
                    .spellID = spellInfo.spellID,
                    .effectIndex = i,
                    .effectType = spellEffect.effectType,
                });
            }

            world.Remove<Tags::IsActiveSpell>(entity);
            world.Emplace<Tags::IsCompleteSpell>(entity);
        });

        auto completedView = world.View<Components::SpellInfo, Tags::IsCompleteSpell>();
        completedView.each([&](entt::entity entity, Components::SpellInfo& spellInfo)
        {
            entt::entity casterEntity = world.GetEntity(spellInfo.caster);
            if (world.ValidEntity(casterEntity))
            {
                auto& characterSpellCastInfo = world.Get<Components::CharacterSpellCastInfo>(casterEntity);
                characterSpellCastInfo.activeSpellEntity = entt::null;

                if (characterSpellCastInfo.queuedSpellEntity != entt::null)
                {
                    characterSpellCastInfo.activeSpellEntity = characterSpellCastInfo.queuedSpellEntity;
                    characterSpellCastInfo.queuedSpellEntity = entt::null;

                    world.Emplace<Tags::IsUnpreparedSpell>(characterSpellCastInfo.activeSpellEntity);
                }
            }

            world.DestroyEntity(entity);
        });
    }
    void UpdateWorld::HandleCombatEventUpdate(World& world, Singletons::NetworkState& networkState, f32 deltaTime)
    {
        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        u32 numCombatEvents = static_cast<u32>(combatEventState.combatEvents.size());
        if (numCombatEvents == 0)
            return;

        std::shared_ptr<Bytebuffer> combatEventBuffer = Bytebuffer::Borrow<256>();

        while (numCombatEvents-- > 0)
        {
            combatEventBuffer->Reset();

            const CombatEventInfo& eventInfo = combatEventState.combatEvents.front();

            entt::entity sourceEntity = world.GetEntity(eventInfo.sourceGUID);
            entt::entity targetEntity = world.GetEntity(eventInfo.targetGUID);

            bool validSourceEntity = world.ValidEntity(sourceEntity);
            bool validTargetEntity = world.ValidEntity(targetEntity);

            if (!validTargetEntity)
            {
                combatEventState.combatEvents.pop_front();
                continue;
            }

            auto& unitPowersComponent = world.Get<Components::UnitPowersComponent>(targetEntity);
            UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, Generated::PowerTypeEnum::Health);
            bool isDead = (healthPower.current <= 0.0);

            bool builtMessage = false;
            switch (eventInfo.eventType)
            {
                case CombatEventType::Damage:
                {
                    if (isDead)
                        break;

                    f64 amount = static_cast<f32>(eventInfo.amount);
                    f64 damageDone = glm::min(amount, healthPower.current);
                    f64 overKillDamage = glm::max(0.0, amount - healthPower.current);

                    if (damageDone <= 0)
                        break;

                    bool canAddToThreatTable = validSourceEntity && world.AllOf<Components::CreatureThreatTable>(targetEntity);
                    if (canAddToThreatTable && sourceEntity != targetEntity)
                    {
                        auto& unitCombatInfo = world.Get<Components::UnitCombatInfo>(sourceEntity);
                        f64 threatAmount = damageDone * unitCombatInfo.threatModifier * 1000.0f;

                        Util::Combat::AddUnitToThreatTable(world, unitCombatInfo, targetEntity, sourceEntity, threatAmount);
                    }

                    bool killedTarget = overKillDamage > 0 || damageDone == healthPower.current;
                    f64 newHealth = healthPower.current - damageDone;
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, Generated::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, damageDone, overKillDamage);
                    
                    if (killedTarget)
                    {
                        world.Emplace<Events::UnitDied>(targetEntity, Events::UnitDied{
                            .killerEntity = validSourceEntity ? sourceEntity : entt::null
                        });
                    }
                    break;
                }

                case CombatEventType::Heal:
                {
                    if (isDead)
                        break;

                    f64 amount = static_cast<f64>(eventInfo.amount);
                    f64 healingDone = glm::min(amount, healthPower.max - healthPower.current);
                    f64 overHealing = glm::max(0.0, amount - healingDone);

                    if (healingDone <= 0)
                        break;

                    if (validSourceEntity && sourceEntity != targetEntity)
                    {
                        auto& unitCombatInfo = world.Get<Components::UnitCombatInfo>(targetEntity);
                        f64 threatAmount = healingDone * unitCombatInfo.threatModifier * 1000.0f;

                        Util::Combat::AddUnitToThreatTables(world, unitCombatInfo, targetEntity, sourceEntity, threatAmount);
                    }

                    f64 newHealth = healthPower.current + healingDone;
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, Generated::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildHealingDoneMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, healingDone, overHealing);

                    break;
                }

                case CombatEventType::Resurrect:
                {
                    f64 missingHealth = healthPower.max - healthPower.current;
                    if (missingHealth <= 0)
                        break;

                    f64 newHealth = healthPower.max;
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, Generated::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildResurrectedMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, missingHealth);

                    world.Emplace<Events::UnitResurrected>(targetEntity, Events::UnitResurrected{
                        .resurrectorEntity = validSourceEntity ? sourceEntity : entt::null
                    });
                    break;
                }

                default: break;
            }

            if (builtMessage)
            {
                entt::entity broadcasterEntity = validSourceEntity ? sourceEntity : targetEntity;

                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(broadcasterEntity);

                // TODO : Handle cases where source and target have gone out of visibility range of each other
                Util::Network::SendToNearby(networkState, world, broadcasterEntity, visibilityInfo, true, combatEventBuffer);
            }

            combatEventState.combatEvents.pop_front();
        }
    }

    void UpdateWorld::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();

        // Handle World Transfer Requests
        {
            auto& worldTransferRequests = worldState.GetWorldTransferRequests();
            
            WorldTransferRequest request;
            while (worldTransferRequests.try_dequeue(request))
            {
                if (!Util::Network::IsSocketActive(networkState, request.socketID))
                    continue;

                if (!worldState.HasWorld(request.targetMapID))
                    worldState.AddWorld(request.targetMapID);

                World& world = worldState.GetWorld(request.targetMapID);
                entt::entity socketEntity = world.CreateEntity();

                auto& netInfo = world.Emplace<Components::NetInfo>(socketEntity);
                netInfo.socketID = request.socketID;

                auto& objectInfo = world.Emplace<Components::ObjectInfo>(socketEntity);
                objectInfo.guid = ObjectGUID(ObjectGUID::Type::Player, request.characterID);

                worldState.SetMapIDForSocket(request.socketID, request.targetMapID);
                networkState.socketIDToEntity[request.socketID] = socketEntity;
                networkState.characterIDToEntity[request.characterID] = socketEntity;
                networkState.characterIDToSocketID[request.characterID] = request.socketID;

                Util::Cache::CharacterCreate(gameCache, request.characterID, request.characterName, socketEntity);

                Util::Network::SendPacket(networkState, request.socketID, Generated::ServerWorldTransferPacket{
                });

                Util::Network::SendPacket(networkState, request.socketID, Generated::ServerLoadMapPacket{
                    .mapID = request.targetMapID
                });

                if (request.useTargetPosition)
                {
                    world.Emplace<Tags::CharacterWasWorldTransferred>(socketEntity);
                    Util::Persistence::Character::CharacterSetMapIDPositionOrientation(gameCache, request.characterID, request.targetMapID, request.targetPosition, request.targetOrientation);
                }

                world.Emplace<Events::CharacterNeedsInitialization>(socketEntity);
            }
        }

        for (auto& itr : worldState)
        {
            u32 mapID = itr.first;
            World& world = itr.second;
            
            if (HandleMapInitialization(world, gameCache))
                continue;

            Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(world.zenithKey);
            if (!zenith)
                continue;

            UpdateCharacter::HandleUpdate(worldState, world, zenith, gameCache, networkState, deltaTime);
            HandleCreatureUpdate(world, zenith, gameCache, networkState, deltaTime);
            HandleUnitUpdate(world, zenith, gameCache, networkState, deltaTime);

            HandleProximityTriggerUpdate(world, gameCache, networkState);

            HandleReplication(world, networkState, deltaTime);
            HandleContainerUpdate(world, gameCache, networkState);
            HandleDisplayUpdate(world, networkState);
            HandleCombatUpdate(world, zenith, networkState, deltaTime);
            HandlePowerUpdate(world, networkState, deltaTime);
            HandleSpellUpdate(world, zenith, gameCache, networkState, deltaTime);
            HandleCombatEventUpdate(world, networkState, deltaTime);
        }
    }
}