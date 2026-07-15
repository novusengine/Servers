#include "UpdateWorld.h"
#include "World/Movement.h"
#include "World/UpdateCharacter.h"

#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/AdministrativeOperation.h"
#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/CharacterSpellCastInfo.h"
#include "Server-Game/ECS/Components/CreatureAIInfo.h"
#include "Server-Game/ECS/Components/CreatureCombatInfo.h"
#include "Server-Game/ECS/Components/CreatureFactionPolicy.h"
#include "Server-Game/ECS/Components/CreatureInfo.h"
#include "Server-Game/ECS/Components/CreatureLifecycleInfo.h"
#include "Server-Game/ECS/Components/CreatureThreatTable.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/MovementIntent.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/PermissionInfo.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Components/SpellInfo.h"
#include "Server-Game/ECS/Components/SpellProcInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
#include "Server-Game/ECS/Components/UnitFaction.h"
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
#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatUtil.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/FactionUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/MovementUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/ProximityTriggerUtil.h"
#include "Server-Game/ECS/Util/SpellUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/DatabaseRetryUtil.h"
#include "Server-Game/ECS/Util/FactionModifierUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/ECS/Util/PermissionUtil.h"
#include "Server-Game/ECS/Util/Persistence/CharacterUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <FileFormat/Novus/NavMesh/NavMesh.h>

#include <Gameplay/ECS/Components/ObjectFields.h>
#include <Gameplay/ECS/Components/UnitFields.h>
#include <Gameplay/Network/GameMessageRouter.h>

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Server/Lua/Lua.h>
#include <MetaGen/Shared/NetField/NetField.h>
#include <MetaGen/Shared/Packet/Packet.h>
#include <MetaGen/Shared/ProximityTrigger/ProximityTrigger.h>
#include <MetaGen/Shared/Spell/Spell.h>

#include <Network/Server.h>

#include <Scripting/LuaManager.h>

#include <entt/entt.hpp>

#include <libsodium/sodium.h>
#include <spake2-ee/crypto_spake.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <system_error>
#include <vector>
namespace fs = std::filesystem;

namespace ECS::Systems
{
    namespace
    {
        constexpr u8 MAX_DEFERRED_DATABASE_RETRIES = 5;
        constexpr f32 CREATURE_MELEE_RANGE = 2.5f;
        constexpr f32 CREATURE_MELEE_VERTICAL_RANGE = 3.0f;
        constexpr f32 CREATURE_MELEE_DAMAGE_VARIANCE = 0.1f;
        constexpr f32 DEFAULT_CREATURE_LEASH_RANGE = 40.0f;
        constexpr f32 CREATURE_HOME_REACHED_DISTANCE = 0.5f;
        constexpr f32 CREATURE_HOME_REACHED_VERTICAL_DISTANCE = 2.0f;
        constexpr f32 CREATURE_CORPSE_DURATION = 60.0f;
        constexpr f64 MAX_ARMOR_DAMAGE_REDUCTION = 0.75;
        constexpr f32 MAX_FACTION_ASSISTANCE_RADIUS = Gameplay::Faction::MAX_CREATURE_ASSISTANCE_RANGE;
        constexpr u32 MAX_FACTION_ASSISTANCE_CANDIDATES = 64;

        enum class CreatureMovementType : u16
        {
            Idle = 0,
            Random = 1,
            Waypoint = 2
        };

        Gameplay::Faction::CreatureAggressionPolicy ResolveAggressionPolicy(const Database::CreatureTemplate& creatureTemplate)
        {
            if (Gameplay::Faction::IsValidCreatureAggressionPolicy(creatureTemplate.aggressionPolicy))
                return static_cast<Gameplay::Faction::CreatureAggressionPolicy>(creatureTemplate.aggressionPolicy);

            NC_LOG_ERROR("Creature template {0} has invalid aggression policy {1}; Defensive will be used", creatureTemplate.id, creatureTemplate.aggressionPolicy);
            return Gameplay::Faction::CreatureAggressionPolicy::Defensive;
        }

        Gameplay::Faction::CreatureAssistancePolicy ResolveAssistancePolicy(const Database::CreatureTemplate& creatureTemplate)
        {
            if (Gameplay::Faction::IsValidCreatureAssistancePolicy(creatureTemplate.assistancePolicy))
                return static_cast<Gameplay::Faction::CreatureAssistancePolicy>(creatureTemplate.assistancePolicy);

            NC_LOG_ERROR("Creature template {0} has invalid assistance policy {1}; None will be used", creatureTemplate.id, creatureTemplate.assistancePolicy);
            return Gameplay::Faction::CreatureAssistancePolicy::None;
        }

        f32 ResolveDetectionRange(const Database::CreatureTemplate& creatureTemplate)
        {
            if (!std::isfinite(creatureTemplate.detectionRange) || creatureTemplate.detectionRange < 0.0f)
            {
                NC_LOG_ERROR("Creature template {0} has invalid detection range {1}; zero will be used", creatureTemplate.id, creatureTemplate.detectionRange);
                return 0.0f;
            }

            if (creatureTemplate.detectionRange > Gameplay::Faction::MAX_CREATURE_DETECTION_RANGE)
            {
                NC_LOG_ERROR("Creature template {0} detection range {1} exceeds the bounded maximum {2}; maximum will be used", creatureTemplate.id, creatureTemplate.detectionRange, Gameplay::Faction::MAX_CREATURE_DETECTION_RANGE);
                return Gameplay::Faction::MAX_CREATURE_DETECTION_RANGE;
            }

            return creatureTemplate.detectionRange;
        }

        f32 ResolveAssistanceRange(const Database::CreatureTemplate& creatureTemplate)
        {
            const f32 assistanceRange = static_cast<f32>(creatureTemplate.assistanceRange);
            if (assistanceRange > Gameplay::Faction::MAX_CREATURE_ASSISTANCE_RANGE)
            {
                NC_LOG_ERROR("Creature template {0} assistance range {1} exceeds the bounded maximum {2}; maximum will be used", creatureTemplate.id, assistanceRange, Gameplay::Faction::MAX_CREATURE_ASSISTANCE_RANGE);
                return Gameplay::Faction::MAX_CREATURE_ASSISTANCE_RANGE;
            }

            return assistanceRange;
        }

        void ResetCreatureResources(World& world, entt::entity entity, Components::UnitPowersComponent& powers,
            Components::UnitStatsComponent& stats)
        {
            for (auto& [powerType, power] : powers.powerTypeToValue)
            {
                f64 resetCurrent = 0.0;
                if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Health ||
                    powerType == MetaGen::Shared::Unit::PowerTypeEnum::Mana ||
                    powerType == MetaGen::Shared::Unit::PowerTypeEnum::Energy)
                {
                    resetCurrent = power.max;
                }
                Util::Unit::SetPower(world, entity, powers, powerType, power.base, resetCurrent, power.max);
            }

            if (auto healthStatItr = stats.statTypeToValue.find(MetaGen::Shared::Unit::StatTypeEnum::Health);
                healthStatItr != stats.statTypeToValue.end())
            {
                healthStatItr->second.current = healthStatItr->second.base;
            }
        }

        void StartCreatureDefaultMovement(World& world, entt::entity entity, const Components::CreatureInfo& creatureInfo,
            const Components::CreatureCombatInfo& creatureCombatInfo)
        {
            if (static_cast<CreatureMovementType>(creatureInfo.movementType) != CreatureMovementType::Random ||
                creatureInfo.wanderDistance <= 0.0f || creatureInfo.walkSpeed <= 0.0f)
            {
                return;
            }

            Util::Movement::WanderParams params;
            params.speed = creatureInfo.walkSpeed;
            params.radius = creatureInfo.wanderDistance;
            Util::Movement::Wander(world, entity, creatureCombatInfo.homePosition, params);
        }

        bool CanPlayerSeeCreature(World& world, const Singletons::GameCache& gameCache, entt::entity playerEntity,
            entt::entity creatureEntity)
        {
            const auto* lifecycleInfo = world.TryGet<Components::CreatureLifecycleInfo>(creatureEntity);
            if (!lifecycleInfo || lifecycleInfo->state != Components::CreatureLifecycleState::Despawned)
                return true;

            const auto* permissionInfo = world.TryGet<Components::CharacterPermissionInfo>(playerEntity);
            return permissionInfo && Util::Permission::HasPermission(gameCache, permissionInfo->effectivePermissions,
                                         MetaGen::Server::Permission::Permission::VisibilityCreatureSeeDespawned);
        }

        void HideCreature(World& world, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState, entt::entity entity,
            const Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo)
        {
            for (auto itr = visibilityInfo.visiblePlayers.begin(); itr != visibilityInfo.visiblePlayers.end();)
            {
                const ObjectGUID playerGUID = *itr;
                entt::entity playerEntity = world.GetEntity(playerGUID);
                if (!world.ValidEntity(playerEntity))
                {
                    itr = visibilityInfo.visiblePlayers.erase(itr);
                    continue;
                }

                if (CanPlayerSeeCreature(world, gameCache, playerEntity, entity))
                {
                    ++itr;
                    continue;
                }

                if (auto* playerNetInfo = world.TryGet<Components::NetInfo>(playerEntity))
                {
                    if (!Util::Network::SendPacket(networkState, playerNetInfo->socketID, MetaGen::Shared::Packet::ServerUnitRemovePacket{ .guid = objectInfo.guid }))
                    {
                        ++itr;
                        continue;
                    }
                }

                if (auto* playerVisibilityInfo = world.TryGet<Components::VisibilityInfo>(playerEntity))
                    playerVisibilityInfo->visibleCreatures.erase(objectInfo.guid);
                itr = visibilityInfo.visiblePlayers.erase(itr);
            }
        }

        void ShowCreature(Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;
        }

        GameDefine::UnitClass ResolveCreatureUnitClass(u16 value)
        {
            switch (value)
            {
                case static_cast<u16>(GameDefine::UnitClass::Warrior):
                    return GameDefine::UnitClass::Warrior;
                case static_cast<u16>(GameDefine::UnitClass::Paladin):
                    return GameDefine::UnitClass::Paladin;
                case static_cast<u16>(GameDefine::UnitClass::Hunter):
                    return GameDefine::UnitClass::Hunter;
                case static_cast<u16>(GameDefine::UnitClass::Rogue):
                    return GameDefine::UnitClass::Rogue;
                case static_cast<u16>(GameDefine::UnitClass::Priest):
                    return GameDefine::UnitClass::Priest;
                case static_cast<u16>(GameDefine::UnitClass::Shaman):
                    return GameDefine::UnitClass::Shaman;
                case static_cast<u16>(GameDefine::UnitClass::Mage):
                    return GameDefine::UnitClass::Mage;
                case static_cast<u16>(GameDefine::UnitClass::Warlock):
                    return GameDefine::UnitClass::Warlock;
                case static_cast<u16>(GameDefine::UnitClass::Druid):
                    return GameDefine::UnitClass::Druid;
                default:
                    return GameDefine::UnitClass::None;
            }
        }

        u16 RollCreatureLevel(World& world, const Database::CreatureTemplate& creatureTemplate)
        {
            const u16 minLevel = std::max<u16>(1, std::min(creatureTemplate.minLevel, creatureTemplate.maxLevel));
            const u16 maxLevel = std::max<u16>(minLevel, std::max(creatureTemplate.minLevel, creatureTemplate.maxLevel));
            const u64 levelCount = static_cast<u64>(maxLevel - minLevel) + 1;
            return static_cast<u16>(minLevel + (world.rng.Next() % levelCount));
        }

        bool IsWithinCreatureMeleeRange(const Components::Transform& creatureTransform, const Components::Transform& targetTransform)
        {
            const vec3 offset = targetTransform.position - creatureTransform.position;
            const f32 horizontalDistanceSq = offset.x * offset.x + offset.z * offset.z;
            return horizontalDistanceSq <= CREATURE_MELEE_RANGE * CREATURE_MELEE_RANGE &&
                   std::abs(offset.y) <= CREATURE_MELEE_VERTICAL_RANGE;
        }

        u32 CalculateCreatureMeleeDamage(World& world, const Components::CreatureInfo& creatureInfo,
            const Components::CreatureCombatInfo& creatureCombatInfo,
            const Components::UnitStatsComponent* targetStats)
        {
            const f32 variance = 1.0f - CREATURE_MELEE_DAMAGE_VARIANCE +
                                 world.rng.NextF32() * (CREATURE_MELEE_DAMAGE_VARIANCE * 2.0f);
            const u32 rawDamage = std::max(1u, static_cast<u32>(std::lround(creatureCombatInfo.meleeDamage * variance)));
            if (creatureCombatInfo.damageSchool != 0 || !targetStats)
                return rawDamage;

            const auto armorItr = targetStats->statTypeToValue.find(MetaGen::Shared::Unit::StatTypeEnum::Armor);
            if (armorItr == targetStats->statTypeToValue.end())
                return rawDamage;

            const UnitStat& armorStat = armorItr->second;
            const f64 armor = std::max(0.0, armorStat.current);
            // Classic physical mitigation uses the attacker's level and caps armor reduction at 75%.
            const f64 armorConstant = 400.0 + 85.0 * static_cast<f64>(creatureInfo.level);
            const f64 reduction = std::min(MAX_ARMOR_DAMAGE_REDUCTION, armor / (armor + armorConstant));
            return std::max(1u, static_cast<u32>(std::lround(static_cast<f64>(rawDamage) * (1.0 - reduction))));
        }

        bool IsDeferredDatabaseRetryable(Database::OperationFailure failure)
        {
            return Util::DatabaseRetry::IsDeferredRetryable(failure);
        }

        void RecruitFactionAssistance(World& world, const Gameplay::Faction::FactionRuntimeData& runtime, entt::entity attackedEntity, entt::entity attackerEntity)
        {
            const auto* attackedTransform = world.TryGet<Components::Transform>(attackedEntity);
            auto* attackerCombatInfo = world.TryGet<Components::UnitCombatInfo>(attackerEntity);
            if (!attackedTransform || !attackerCombatInfo)
                return;

            u32 scannedCandidates = 0;
            world.GetCreaturesInRadius(vec2(attackedTransform->position.x, attackedTransform->position.z), MAX_FACTION_ASSISTANCE_RADIUS, [&](ObjectGUID candidateGUID)
            {
                if (++scannedCandidates > MAX_FACTION_ASSISTANCE_CANDIDATES)
                    return false;

                const entt::entity candidate = world.GetEntity(candidateGUID);
                if (!world.ValidEntity(candidate) || candidate == attackedEntity || candidate == attackerEntity || !world.AllOf<Components::CreatureThreatTable, Components::Transform, Tags::IsAlive>(candidate))
                {
                    return true;
                }

                const auto* policy = world.TryGet<Components::CreatureFactionPolicy>(candidate);
                if (!policy || policy->assistance == Gameplay::Faction::CreatureAssistancePolicy::None || policy->assistanceRange <= 0.0f)
                {
                    return true;
                }

                const vec3 offset = world.Get<Components::Transform>(candidate).position - attackedTransform->position;
                if (glm::dot(offset, offset) > policy->assistanceRange * policy->assistanceRange)
                    return true;

                bool policyAllowsAssist = false;
                if (policy->assistance == Gameplay::Faction::CreatureAssistancePolicy::SameFaction)
                {
                    const auto* candidateFaction = world.TryGet<Components::UnitFaction>(candidate);
                    const auto* attackedFaction = world.TryGet<Components::UnitFaction>(attackedEntity);
                    policyAllowsAssist = candidateFaction && attackedFaction && candidateFaction->effectiveFaction == attackedFaction->effectiveFaction;
                }
                else if (policy->assistance == Gameplay::Faction::CreatureAssistancePolicy::Friendly)
                {
                    policyAllowsAssist = Util::Faction::CanAssist(world, candidate, attackedEntity, runtime);
                }

                if (policyAllowsAssist && Util::Faction::CanAttack(world, candidate, attackerEntity, runtime))
                    Util::Combat::AddUnitToThreatTable(world, *attackerCombatInfo, candidate, attackerEntity);

                return scannedCandidates < MAX_FACTION_ASSISTANCE_CANDIDATES;
            });
        }

        bool ScheduleDeferredDatabaseRetry(World& world, entt::entity entity, const Singletons::TimeState& timeState,
            Database::OperationFailure failure)
        {
            if (!IsDeferredDatabaseRetryable(failure))
                return false;

            auto& retry = world.GetOrEmplace<Components::DatabaseRetryState>(entity);
            if (retry.retryCount >= MAX_DEFERRED_DATABASE_RETRIES)
                return false;

            const u64 entropy = static_cast<u64>(entt::to_integral(entity)) ^ timeState.epochAtFrameStart;
            const u64 delay = Util::DatabaseRetry::DelaySeconds(retry.retryCount, entropy);
            ++retry.retryCount;
            retry.retryAfterEpoch = timeState.epochAtFrameStart + delay;
            return true;
        }

        bool DeferredDatabaseRetryIsPending(World& world, entt::entity entity, const Singletons::TimeState& timeState)
        {
            const auto* retry = world.TryGet<Components::DatabaseRetryState>(entity);
            return retry && retry->retryAfterEpoch > timeState.epochAtFrameStart;
        }

        Network::SocketID AdministrativeRequester(World& world, entt::entity entity)
        {
            const auto* context = world.TryGet<Components::AdministrativeRequestContext>(entity);
            return context ? context->requesterSocketID : Network::SOCKET_ID_INVALID;
        }

        void ClearAdministrativeOperationState(World& world, entt::entity entity)
        {
            world.Remove<Components::DatabaseRetryState, Components::AdministrativeRequestContext>(entity);
        }

        void NotifyAdministrativeDatabaseFailure(World& world, Singletons::NetworkState& networkState,
            Network::SocketID socketID, std::string_view operation, Database::OperationFailure failure)
        {
            const bool manualReconciliation = failure == Database::OperationFailure::Indeterminate;
            std::string message = "Administrative operation '" + std::string(operation) + "' failed (" +
                                  std::string(Database::OperationFailureName(failure)) + ")";
            if (manualReconciliation)
                message += "; the database outcome is unknown and requires manual reconciliation";
            if (socketID != Network::SOCKET_ID_INVALID && Util::Network::IsSocketActive(networkState, socketID))
                Util::Unit::SendChatMessage(world, networkState, socketID, message);
            NC_LOG_ERROR("{0}", message);
        }
    }

    namespace
    {
        struct PendingNavmeshTile
        {
        public:
            u32 chunkX;
            u32 chunkY;
            u32 polyCount;
            fs::path path;
        };

        bool TryParseNavmeshChunk(const std::string& mapInternalName, const fs::path& path, u32& chunkX, u32& chunkY)
        {
            std::string stem = path.stem().string();
            std::string prefix = mapInternalName + "_";
            if (!stem.starts_with(prefix))
                return false;

            std::string_view coordinates(stem.data() + prefix.size(), stem.size() - prefix.size());
            size_t separatorIndex = coordinates.find('_');
            if (separatorIndex == std::string_view::npos || coordinates.find('_', separatorIndex + 1) != std::string_view::npos)
                return false;

            std::string_view chunkXStr = coordinates.substr(0, separatorIndex);
            std::string_view chunkYStr = coordinates.substr(separatorIndex + 1);
            if (chunkXStr.empty() || chunkYStr.empty())
                return false;

            auto chunkXResult = std::from_chars(chunkXStr.data(), chunkXStr.data() + chunkXStr.size(), chunkX);
            auto chunkYResult = std::from_chars(chunkYStr.data(), chunkYStr.data() + chunkYStr.size(), chunkY);
            return chunkXResult.ec == std::errc() && chunkXResult.ptr == chunkXStr.data() + chunkXStr.size() &&
                   chunkYResult.ec == std::errc() && chunkYResult.ptr == chunkYStr.data() + chunkYStr.size();
        }

        bool ReadBinaryFile(const fs::path& path, std::vector<u8>& data)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                return false;

            std::streamsize fileSize = file.tellg();
            if (fileSize <= 0 || fileSize > static_cast<std::streamsize>(std::numeric_limits<i32>::max()))
                return false;

            data.resize(static_cast<size_t>(fileSize));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(data.data()), fileSize);
            return file.gcount() == fileSize;
        }

        bool ReadNavmeshTileHeader(const fs::path& path, dtMeshHeader& header)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return false;

            file.read(reinterpret_cast<char*>(&header), sizeof(dtMeshHeader));
            return file.gcount() == sizeof(dtMeshHeader);
        }
    }

    void UpdateWorld::Init(entt::registry& registry)
    {
        auto& networkState = registry.ctx().get<Singletons::NetworkState>();
        registry.ctx().emplace<Singletons::WorldState>(networkState.packetArenaBudget, networkState.packetArenaConfig);
    }

    bool UpdateWorld::HandleMapInitialization(World& world, Singletons::TimeState& timeState, Singletons::GameCache& gameCache)
    {
        if (!world.ContainsSingleton<Events::MapNeedsInitialization>())
            return false;

        auto& initialization = world.GetSingleton<Events::MapNeedsInitialization>();
        if (initialization.retryAfterEpoch > timeState.epochAtFrameStart)
            return false;

        const auto scheduleRetry = [&](std::string_view operation, Database::OperationFailure failure)
        {
            if (!IsDeferredDatabaseRetryable(failure))
            {
                NC_LOG_ERROR("Map {0} initialization {1} failed ({2}); automatic retries stopped",
                    world.mapID, operation, Database::OperationFailureName(failure));
                world.EraseSingleton<Events::MapNeedsInitialization>();
                return;
            }

            const u64 delay = Util::DatabaseRetry::DelaySeconds(initialization.retryCount,
                static_cast<u64>(world.mapID) ^ timeState.epochAtFrameStart, 5);
            ++initialization.retryCount;
            initialization.retryAfterEpoch = timeState.epochAtFrameStart + delay;
            NC_LOG_WARNING("Map {0} initialization {1} failed ({2}); retry {3} in {4} seconds",
                world.mapID, operation, Database::OperationFailureName(failure), initialization.retryCount,
                delay);
        };

        GameDefine::Database::Map* map;
        if (!Util::Cache::GetMapByID(gameCache, world.mapID, map))
            return false;

        auto creaturesResult = gameCache.database->GetCreaturesByMap(world.mapID);
        if (!creaturesResult)
        {
            scheduleRetry("creature load", creaturesResult.Failure());
            return false;
        }
        auto triggersResult = gameCache.database->GetProximityTriggersByMap(world.mapID);
        if (!triggersResult)
        {
            scheduleRetry("proximity-trigger load", triggersResult.Failure());
            return false;
        }

        world.EraseSingleton<Events::MapNeedsInitialization>();

        // Seed RNG
        u64 seedBase = static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        u64 seed = seedBase + (static_cast<u64>(world.mapID) * 0x9E3779B97f4A7C15ull);
        world.rng.Seed(seed);

        // Handle Map Initialization
        world.EmplaceSingleton<Singletons::CombatEventState>();
        world.EmplaceSingleton<Singletons::CreatureAIState>();
        world.EmplaceSingleton<Singletons::ProximityTriggers>();

        const auto& creatures = creaturesResult.Value();
        if (!creatures.empty())
        {
            world.movementScheduler.ReservePathRequests(static_cast<u32>(creatures.size()) + 128);
            for (const auto& creature : creatures)
            {
                Database::CreatureTemplate* creatureTemplate = nullptr;
                if (!Util::Cache::GetCreatureTemplateByID(gameCache, creature.templateId, creatureTemplate))
                    continue;

                entt::entity creatureEntity = world.CreateEntity();

                Events::CreatureNeedsInitialization event = {
                    .guid = ObjectGUID::CreateCreature(creature.id),
                    .templateID = creature.templateId,
                    .displayID = creature.displayId,
                    .mapID = creature.mapId,
                    .position = vec3(creature.positionX, creature.positionY, creature.positionZ),
                    .scale = vec3(creatureTemplate->scale),
                    .orientation = creature.positionO,
                    .scriptName = creature.scriptName.empty() ? creatureTemplate->scriptName : creature.scriptName,
                    .spawnTimeInSecMin = creature.spawnTimeInSecMin,
                    .spawnTimeInSecMax = creature.spawnTimeInSecMax,
                    .wanderDistance = creature.wanderDistance,
                    .movementType = creature.movementType
                };

                world.Emplace<Events::CreatureNeedsInitialization>(creatureEntity, event);
            }
        }

        const auto& triggers = triggersResult.Value();
        if (!triggers.empty())
        {
            for (const auto& trigger : triggers)
            {
                entt::entity triggerEntity = world.CreateEntity();

                Events::ProximityTriggerNeedsInitialization event = {
                    .triggerID = trigger.id,

                    .name = trigger.name,
                    .flags = static_cast<MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum>(trigger.flags),

                    .mapID = trigger.mapId,
                    .position = vec3(trigger.positionX, trigger.positionY, trigger.positionZ),
                    .extents = vec3(trigger.extentsX, trigger.extentsY, trigger.extentsZ)
                };

                world.Emplace<Events::ProximityTriggerNeedsInitialization>(triggerEntity, event);
            }
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

        // The server currently makes the whole map available at once. Keep tile loading
        // chunk-addressable so a future chunk streamer can call AddTile/RemoveTile directly.
        {
            WorldNavmeshData& navmeshData = world.navmeshData;
            fs::path relativeParentPath = "Data/NavMesh/" + map->internalName;
            fs::path absolutePath = fs::absolute(relativeParentPath).make_preferred();
            std::vector<PendingNavmeshTile> pendingTiles;

            std::error_code error;
            fs::directory_iterator directory(absolutePath, error);
            if (error)
            {
                NC_LOG_WARNING("Navmesh directory is unavailable for map {0}: {1}", map->internalName, absolutePath.string());
            }
            else
            {
                std::string internalNameLowerCase = map->internalName;
                StringUtils::ToLower(internalNameLowerCase);
                for (const fs::directory_entry& entry : directory)
                {
                    if (!entry.is_regular_file() || entry.path().extension() != NavMesh::TILE_FILE_EXTENSION)
                        continue;

                    u32 chunkX = 0;
                    u32 chunkY = 0;
                    if (!TryParseNavmeshChunk(internalNameLowerCase, entry.path(), chunkX, chunkY))
                    {
                        NC_LOG_WARNING("Ignoring navmesh tile with an unexpected filename: {0}", entry.path().string());
                        continue;
                    }

                    PendingNavmeshTile& pendingTile = pendingTiles.emplace_back();
                    pendingTile.chunkX = chunkX;
                    pendingTile.chunkY = chunkY;
                    pendingTile.path = entry.path();

                    dtMeshHeader header{};
                    if (!ReadNavmeshTileHeader(entry.path(), header) ||
                        header.magic != DT_NAVMESH_MAGIC ||
                        header.version != DT_NAVMESH_VERSION ||
                        header.x != static_cast<i32>(chunkX) ||
                        header.y != static_cast<i32>(chunkY) ||
                        header.layer != static_cast<i32>(NavMesh::TILE_LAYER) ||
                        header.polyCount <= 0)
                    {
                        NC_LOG_ERROR("Failed to inspect navmesh tile {0}", entry.path().string());
                        pendingTiles.pop_back();
                        continue;
                    }

                    pendingTile.polyCount = static_cast<u32>(header.polyCount);
                }
            }

            u32 maxPolys = 1;
            for (const PendingNavmeshTile& pendingTile : pendingTiles)
            {
                maxPolys = std::max(maxPolys, pendingTile.polyCount);
            }

            if (pendingTiles.size() > static_cast<size_t>(std::numeric_limits<i32>::max()))
            {
                NC_LOG_ERROR("Failed to initialize navmesh for map {0}: {1} tiles exceed the Detour allocation limit",
                    map->internalName, pendingTiles.size());
            }
            else
            {
                u32 maxTiles = std::max(1u, static_cast<u32>(pendingTiles.size()));

                if (!navmeshData.Initialize(maxTiles, maxPolys))
                {
                    NC_LOG_ERROR("Failed to initialize navmesh for map {0} with capacity for {1} tiles and {2} polygons per tile",
                        map->internalName, maxTiles, maxPolys);
                }
                else
                {
                    u32 missingHeightTiles = 0;
                    u32 invalidHeightTiles = 0;
                    for (const PendingNavmeshTile& pendingTile : pendingTiles)
                    {
                        std::vector<u8> navData;
                        if (!ReadBinaryFile(pendingTile.path, navData))
                        {
                            NC_LOG_ERROR("Failed to read navmesh tile {0}", pendingTile.path.string());
                            continue;
                        }

                        if (!navmeshData.AddTile(pendingTile.chunkX, pendingTile.chunkY, navData.data(), static_cast<u32>(navData.size())))
                        {
                            NC_LOG_ERROR("Failed to add navmesh tile {0}", pendingTile.path.string());
                            continue;
                        }

                        fs::path heightPath = pendingTile.path;
                        heightPath.replace_extension(NavMesh::TerrainHeight::FILE_EXTENSION);

                        std::error_code heightError;
                        if (!fs::is_regular_file(heightPath, heightError))
                        {
                            missingHeightTiles++;
                            continue;
                        }

                        std::vector<u8> heightData;
                        if (!ReadBinaryFile(heightPath, heightData) ||
                            !navmeshData.AddHeightTile(pendingTile.chunkX, pendingTile.chunkY, heightData.data(), static_cast<u32>(heightData.size())))
                        {
                            invalidHeightTiles++;
                            NC_LOG_ERROR("Failed to load terrain height sidecar {0}", heightPath.string());
                        }
                    }

                    NC_LOG_INFO("Loaded {0} navmesh tiles and {1} terrain height sidecars for map {2} (capacity: {3} tiles, {4} polygons per tile)",
                        navmeshData.GetNumTiles(), navmeshData.GetNumHeightTiles(), map->internalName, maxTiles, maxPolys);

                    if (missingHeightTiles > 0)
                    {
                        NC_LOG_WARNING("Missing {0} terrain height sidecars for map {1}; affected queries will use Detour height fallback",
                            missingHeightTiles, map->internalName);
                    }

                    if (invalidHeightTiles > 0)
                    {
                        NC_LOG_ERROR("Rejected {0} malformed or unreadable terrain height sidecars for map {1}; affected queries will use Detour height fallback",
                            invalidHeightTiles, map->internalName);
                    }
                }
            }
        }

        return true;
    }

    void UpdateWorld::HandleNetworkMessages(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        u64 laneID = world.zenithKey.GetMapID();
        moodycamel::ConcurrentQueue<Network::SocketMessageEvent>& messageEvents = networkState.server->GetMessageEvents(laneID);

        Network::SocketMessageEvent messageEvent;
        u32 processedMessageCount = 0;
        while (processedMessageCount < networkState.inboundMessagesPerLanePerUpdate && messageEvents.try_dequeue(messageEvent))
        {
            processedMessageCount++;
            if (!Util::Network::IsSocketActive(networkState, messageEvent.socketID))
                continue;

            Network::MessageHeader messageHeader;
            bool canCallHandler = networkState.messageRouter->GetMessageHeader(messageEvent.message, messageHeader) && networkState.messageRouter->HasValidHandlerForHeader(messageHeader);
            if (canCallHandler)
            {
                entt::entity characterEntity;
                Util::Network::GetCharacterEntity(networkState, messageEvent.socketID, characterEntity);

                if (!world.ValidEntity(characterEntity) || networkState.messageRouter->CallHandler(&world, characterEntity, messageEvent.socketID, messageHeader, messageEvent.message))
                    continue;
            }

            // Failed to Call Handler, Close Socket
            {
                Util::Network::RequestSocketToClose(networkState, messageEvent.socketID);
            }
        }
    }

    void UpdateWorld::HandleCreatureDeinitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto view = world.View<Events::CreatureNeedsDeinitialization>();
        view.each([&](entt::entity entity, Events::CreatureNeedsDeinitialization& event)
        {
            if (DeferredDatabaseRetryIsPending(world, entity, timeState))
                return;

            entt::entity creatureEntity = world.GetEntity(event.guid);
            if (creatureEntity == entt::null)
            {
                ClearAdministrativeOperationState(world, entity);
                world.Remove<Events::CreatureNeedsDeinitialization>(entity);
                return;
            }

            auto result = gameCache.database->DeleteCreature(event.guid.GetCounter());
            if (!result)
            {
                if (ScheduleDeferredDatabaseRetry(world, entity, timeState, result.Failure()))
                    return;
                NotifyAdministrativeDatabaseFailure(world, networkState, AdministrativeRequester(world, entity), "delete_creature", result.Failure());
                ClearAdministrativeOperationState(world, entity);
                world.Remove<Events::CreatureNeedsDeinitialization>(entity);
                return;
            }

            world.RemoveEntity(event.guid);
            world.DestroyEntity(entity);
        });
    }
    void UpdateWorld::HandleCreatureInitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto createView = world.View<Events::CreatureCreate>();
        createView.each([&](entt::entity entity, Events::CreatureCreate& event)
        {
            if (DeferredDatabaseRetryIsPending(world, entity, timeState))
                return;

            Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
            {
                world.DestroyEntity(entity);
                return;
            }

            auto result = gameCache.database->CreateCreature(Database::CreatureCreateRequest{
                .templateID = event.templateID, .displayID = event.displayID, .mapID = event.mapID, .position = event.position, .orientation = event.orientation, .scriptName = event.scriptName, .spawnTimeInSecMin = event.spawnTimeInSecMin, .spawnTimeInSecMax = event.spawnTimeInSecMax, .wanderDistance = event.wanderDistance, .movementType = event.movementType });
            if (!result)
            {
                if (ScheduleDeferredDatabaseRetry(world, entity, timeState, result.Failure()))
                    return;
                NotifyAdministrativeDatabaseFailure(world, networkState, AdministrativeRequester(world, entity), "create_creature", result.Failure());
                world.DestroyEntity(entity);
                return;
            }
            auto creature = std::move(result).Value();

            Events::CreatureNeedsInitialization initEvent = {
                .guid = ObjectGUID::CreateCreature(creature.id),
                .templateID = event.templateID,
                .displayID = creature.displayId,
                .mapID = event.mapID,
                .position = event.position,
                .scale = event.scale,
                .orientation = event.orientation,
                .scriptName = event.scriptName,
                .spawnTimeInSecMin = creature.spawnTimeInSecMin,
                .spawnTimeInSecMax = creature.spawnTimeInSecMax,
                .wanderDistance = creature.wanderDistance,
                .movementType = creature.movementType
            };

            world.Emplace<Events::CreatureNeedsInitialization>(entity, initEvent);
            ClearAdministrativeOperationState(world, entity);
            world.Remove<Events::CreatureCreate>(entity);
        });

        auto initView = world.View<Events::CreatureNeedsInitialization>();
        initView.each([&](entt::entity entity, Events::CreatureNeedsInitialization& event)
        {
            Database::CreatureTemplate* creatureTemplate = nullptr;
            if (!Util::Cache::GetCreatureTemplateByID(gameCache, event.templateID, creatureTemplate))
                return;

            Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
            if (!gameCache.factionRuntimeData || !gameCache.factionRuntimeData->TryGetFactionIndex(creatureTemplate->factionId, factionIndex))
            {
                NC_LOG_ERROR("Creature template {0} has no compiled faction for ID {1}", event.templateID, creatureTemplate->factionId);
                return;
            }

            const u8 reactionBounds = gameCache.factionRuntimeData->ResolveCreatureReactionBounds(*creatureTemplate);

            world.Emplace<Tags::IsCreature>(entity);
            auto& objectInfo = world.Emplace<Components::ObjectInfo>(entity);
            objectInfo.guid = event.guid;

            auto& creatureInfo = world.Emplace<Components::CreatureInfo>(entity);
            creatureInfo.templateID = event.templateID;
            creatureInfo.name = creatureTemplate->name;
            creatureInfo.unitClass = ResolveCreatureUnitClass(creatureTemplate->unitClass);
            creatureInfo.level = RollCreatureLevel(world, *creatureTemplate);
            creatureInfo.walkSpeed = Util::Movement::DEFAULT_WALK_SPEED * std::max(0.0f, creatureTemplate->walkSpeedMod);
            creatureInfo.runSpeed = Util::Movement::DEFAULT_RUN_SPEED * std::max(0.0f, creatureTemplate->runSpeedMod);
            creatureInfo.wanderDistance = std::max(0.0f, event.wanderDistance);
            creatureInfo.movementType = event.movementType;

            world.Emplace<Components::CreatureFactionPolicy>(entity, Components::CreatureFactionPolicy{
                .aggression = ResolveAggressionPolicy(*creatureTemplate),
                .assistance = ResolveAssistancePolicy(*creatureTemplate),
                .detectionRange = ResolveDetectionRange(*creatureTemplate),
                .assistanceRange = ResolveAssistanceRange(*creatureTemplate),
                .timeToNextAcquisition = static_cast<f32>(event.guid.GetCounter() % 250u) / 1000.0f
            });

            world.Emplace<Components::UnitFaction>(entity, Components::UnitFaction{
                .assignedFaction = factionIndex,
                .effectiveFaction = factionIndex,
                .assignedPlayerReactionBounds = reactionBounds,
                .effectivePlayerReactionBounds = reactionBounds
            });

            auto& creatureCombatInfo = world.Emplace<Components::CreatureCombatInfo>(entity);
            creatureCombatInfo.homePosition = event.position;
            creatureCombatInfo.homeOrientation = event.orientation;
            creatureCombatInfo.leashRange = creatureTemplate->leashRange > 0.0f ? creatureTemplate->leashRange : DEFAULT_CREATURE_LEASH_RANGE;

            auto& creatureLifecycleInfo = world.Emplace<Components::CreatureLifecycleInfo>(entity);
            creatureLifecycleInfo.spawnTimeInSecMin = std::min(event.spawnTimeInSecMin, event.spawnTimeInSecMax);
            creatureLifecycleInfo.spawnTimeInSecMax = std::max(event.spawnTimeInSecMin, event.spawnTimeInSecMax);

            auto& creatureTransform = world.Emplace<Components::Transform>(entity);
            creatureTransform.mapID = event.mapID;
            creatureTransform.position = event.position;
            creatureTransform.pitchYaw = vec2(0.0f, event.orientation);
            creatureTransform.scale = event.scale;

            auto& objectFields = world.Emplace<Components::ObjectFields>(entity);
            objectFields.fields.SetField(MetaGen::Shared::NetField::ObjectNetFieldEnum::ObjectGUIDLow, objectInfo.guid);
            objectFields.fields.SetField(MetaGen::Shared::NetField::ObjectNetFieldEnum::Scale, 1.0f);

            auto& unitFields = world.Emplace<Components::UnitFields>(entity);

            constexpr u8 unitClassBytesOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::ClassByteOffset;
            constexpr u8 unitClassBitOffset = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::ClassBitOffset;
            constexpr u8 unitClassBitSize = (u8)MetaGen::Shared::NetField::UnitLevelRaceGenderClassPackedInfoEnum::ClassBitSize;
            unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, creatureInfo.level);
            unitFields.fields.SetField(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked, creatureInfo.unitClass, unitClassBytesOffset, unitClassBitOffset, unitClassBitSize);
            const u32 displayID = event.displayID != 0 ? event.displayID : creatureTemplate->displayId;
            Util::Unit::UpdateDisplayID(*world.registry, entity, unitFields, displayID);

            auto& visualItems = world.Emplace<Components::UnitVisualItems>(entity);
            auto& displayInfo = world.Emplace<Components::DisplayInfo>(entity);
            displayInfo.displayID = displayID;

            Database::CreatureClassLevelStats* classLevelStats = nullptr;
            const bool hasClassLevelStats = Util::Cache::GetCreatureClassLevelStats(
                gameCache, creatureTemplate->unitClass, creatureInfo.level, classLevelStats);
            creatureCombatInfo.meleeDamage = hasClassLevelStats
                                                 ? std::max(1.0f, classLevelStats->damage * std::max(0.0f, creatureTemplate->damageMod))
                                                 : std::max(1.0f, creatureTemplate->damageMod);
            creatureCombatInfo.meleeAttackTime = std::max(0.1f, static_cast<f32>(creatureTemplate->meleeAttackTimeMs) / 1000.0f);
            creatureCombatInfo.damageSchool = creatureTemplate->damageSchool;

            Components::UnitPowersComponent& unitPowersComponent = Util::Unit::AddPowersComponent(world, entity, creatureInfo.unitClass);
            UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);
            {
                if (hasClassLevelStats)
                {
                    const f64 health = static_cast<f64>(classLevelStats->health) * std::max(0.0f, creatureTemplate->healthMod);
                    healthPower.base = health;
                    healthPower.current = health;
                    healthPower.max = health;
                }
                else
                {
                    healthPower.base *= creatureTemplate->healthMod;
                    healthPower.current *= creatureTemplate->healthMod;
                    healthPower.max *= creatureTemplate->healthMod;
                }

                for (auto& [powerType, power] : unitPowersComponent.powerTypeToValue)
                {
                    if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Health)
                        continue;

                    const f64 currentRatio = power.max > 0.0 ? power.current / power.max : 0.0;
                    const f64 resource = hasClassLevelStats
                                             ? static_cast<f64>(classLevelStats->resource) * std::max(0.0f, creatureTemplate->resourceMod)
                                             : power.max * std::max(0.0f, creatureTemplate->resourceMod);
                    power.base = resource;
                    power.current = resource * currentRatio;
                    power.max = resource;
                }
            }

            Util::Unit::AddResistancesComponent(world, entity);
            Components::UnitStatsComponent& unitStatsComponent = Util::Unit::AddStatsComponent(world, entity);
            UnitStat& healthStat = Util::Unit::GetStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Health);
            healthStat.base = healthPower.base;
            healthStat.current = healthPower.current;

            if (hasClassLevelStats)
            {
                UnitStat& armorStat = Util::Unit::GetStat(unitStatsComponent, MetaGen::Shared::Unit::StatTypeEnum::Armor);
                const f64 armor = static_cast<f64>(classLevelStats->armor) * std::max(0.0f, creatureTemplate->armorMod);
                armorStat.base = armor;
                armorStat.current = armor;
            }

            world.Emplace<Tags::IsAlive>(entity);
            auto& visibilityInfo = world.Emplace<Components::VisibilityInfo>(entity);
            auto& visibilityUpdateInfo = world.Emplace<Components::VisibilityUpdateInfo>(entity);
            visibilityUpdateInfo.updateInterval = 0.5f;
            visibilityUpdateInfo.timeSinceLastUpdate = visibilityUpdateInfo.updateInterval;

            world.Emplace<Components::UnitSpellCooldownHistory>(entity);
            world.Emplace<Components::UnitCombatInfo>(entity);
            world.Emplace<Components::UnitAuraInfo>(entity);
            Components::TargetInfo& targetInfo = world.Emplace<Components::TargetInfo>(entity);
            targetInfo.target = entt::null;

            world.Emplace<Components::CreatureThreatTable>(entity);
            if (!event.scriptName.empty())
                world.Emplace<Events::CreatureAddScript>(entity, Events::CreatureAddScript{ .scriptName = event.scriptName });

            world.AddEntity(objectInfo.guid, entity, vec2(creatureTransform.position.x, creatureTransform.position.z));

            // Idle spawns need no intent. Waypoint movement remains stationary until the server has a waypoint system.
            if (static_cast<CreatureMovementType>(creatureInfo.movementType) == CreatureMovementType::Random &&
                creatureInfo.wanderDistance > 0.0f && creatureInfo.walkSpeed > 0.0f)
            {
                Util::Movement::WanderParams params;
                params.speed = creatureInfo.walkSpeed;
                params.radius = creatureInfo.wanderDistance;
                Util::Movement::Wander(world, entity, creatureTransform.position, params);
            }
        });
        world.Clear<Events::CreatureNeedsInitialization>();
    }
    void UpdateWorld::HandleCreatureUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        HandleCreatureDeinitialization(world, zenith, timeState, gameCache, networkState);
        HandleCreatureInitialization(world, zenith, timeState, gameCache, networkState);

        auto& creatureAIState = world.GetSingleton<Singletons::CreatureAIState>();

        auto removeScriptView = world.View<const Components::ObjectInfo, Events::CreatureRemoveScript>();
        removeScriptView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo)
        {
            zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnDeinit, MetaGen::Server::Lua::CreatureAIEventDataOnDeinit{
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

            zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnInit, MetaGen::Server::Lua::CreatureAIEventDataOnInit{
                .creatureEntity = entt::to_integral(entity),
                .scriptNameHash = scriptNameHash
            });
        });
        world.Clear<Events::CreatureAddScript>();

        auto updateAIView = world.View<Components::CreatureAIInfo>();
        updateAIView.each([&](entt::entity entity, Components::CreatureAIInfo& creatureAIInfo)
        {
            if (!world.AllOf<Tags::IsAlive>(entity))
                return;

            creatureAIInfo.timeToNextUpdate -= timeState.deltaTime;
            if (creatureAIInfo.timeToNextUpdate > 0.0f)
                return;

            creatureAIInfo.timeToNextUpdate += 0.1f;

            zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnUpdate, MetaGen::Server::Lua::CreatureAIEventDataOnUpdate{
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

        if (gameCache.factionRuntimeData)
        {
            static constexpr u32 MAX_FACTION_ACQUISITION_CANDIDATES = 64;
            auto acquisitionView = world.View<Components::CreatureCombatInfo, Components::CreatureFactionPolicy, Components::Transform, Tags::IsAlive>(entt::exclude<Tags::IsInCombat>);
            acquisitionView.each([&](entt::entity entity, Components::CreatureCombatInfo& combatInfo, Components::CreatureFactionPolicy& policy, Components::Transform& transform)
            {
                if (combatInfo.isEvading || policy.aggression != Gameplay::Faction::CreatureAggressionPolicy::Aggressive || policy.detectionRange <= 0.0f)
                    return;

                policy.timeToNextAcquisition -= timeState.deltaTime;
                if (policy.timeToNextAcquisition > 0.0f)
                    return;

                policy.timeToNextAcquisition = 0.25f;

                entt::entity bestTarget = entt::null;
                f32 bestDistanceSq = std::numeric_limits<f32>::max();
                u64 bestGUID = std::numeric_limits<u64>::max();
                u32 scannedCandidates = 0;
                const f32 detectionRangeSq = policy.detectionRange * policy.detectionRange;
                const auto considerCandidate = [&](ObjectGUID guid) -> bool
                {
                    if (++scannedCandidates > MAX_FACTION_ACQUISITION_CANDIDATES)
                        return false;

                    entt::entity candidate = world.GetEntity(guid);
                    if (!world.ValidEntity(candidate) || candidate == entity || !world.AllOf<Components::ObjectInfo, Components::Transform, Components::UnitCombatInfo, Tags::IsAlive>(candidate))
                        return true;

                    if (guid.GetType() == ObjectGUID::Type::Player && !CanPlayerSeeCreature(world, gameCache, candidate, entity))
                        return true;

                    const vec3 offset = world.Get<Components::Transform>(candidate).position - transform.position;
                    const f32 distanceSq = glm::dot(offset, offset);
                    if (distanceSq > detectionRangeSq || !Util::Faction::IsHostileTo(world, entity, candidate, *gameCache.factionRuntimeData))
                        return true;

                    if (distanceSq < bestDistanceSq || (distanceSq == bestDistanceSq && guid.GetData() < bestGUID))
                    {
                        bestTarget = candidate;
                        bestDistanceSq = distanceSq;
                        bestGUID = guid.GetData();
                    }

                    return scannedCandidates < MAX_FACTION_ACQUISITION_CANDIDATES;
                };

                world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), policy.detectionRange, considerCandidate);
                if (scannedCandidates < MAX_FACTION_ACQUISITION_CANDIDATES)
                    world.GetCreaturesInRadius(vec2(transform.position.x, transform.position.z), policy.detectionRange, considerCandidate);

                if (bestTarget != entt::null)
                {
                    auto& targetCombatInfo = world.Get<Components::UnitCombatInfo>(bestTarget);
                    Util::Combat::AddUnitToThreatTable(world, targetCombatInfo, entity, bestTarget);
                }
            });
        }

        auto evadeResetView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::CreatureCombatInfo, Components::Transform,
            Components::VisibilityInfo, Components::TargetInfo, Components::UnitPowersComponent, Components::UnitStatsComponent,
            Tags::IsAlive>();
        evadeResetView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo,
                                Components::CreatureCombatInfo& creatureCombatInfo, Components::Transform& transform,
                                Components::VisibilityInfo& visibilityInfo, Components::TargetInfo& targetInfo,
                                Components::UnitPowersComponent& powers, Components::UnitStatsComponent& stats)
        {
            if (!creatureCombatInfo.isEvading || world.AllOf<Tags::IsInCombat>(entity))
                return;

            const vec3 homeOffset = transform.position - creatureCombatInfo.homePosition;
            const f32 horizontalDistanceSq = homeOffset.x * homeOffset.x + homeOffset.z * homeOffset.z;
            if (horizontalDistanceSq > CREATURE_HOME_REACHED_DISTANCE * CREATURE_HOME_REACHED_DISTANCE ||
                std::abs(homeOffset.y) > CREATURE_HOME_REACHED_VERTICAL_DISTANCE)
            {
                return;
            }

            Util::Movement::Stop(world, entity);
            transform.position = creatureCombatInfo.homePosition;
            transform.pitchYaw.y = creatureCombatInfo.homeOrientation;
            targetInfo.target = entt::null;

            ResetCreatureResources(world, entity, powers, stats);

            creatureCombatInfo.isEvading = false;
            const Components::MovementFlags movementFlags{
                .grounded = 1
            };
            ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, false,
                ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitMovePacket::PACKET_ID, objectInfo.guid),
                MetaGen::Shared::Packet::ServerUnitMovePacket{
                    .guid = objectInfo.guid,
                    .movementFlags = movementFlags.ToBitMask(),
                    .position = transform.position,
                    .pitchYaw = transform.pitchYaw,
                    .verticalVelocity = 0.0f });
            world.creatureVisData.Update(objectInfo.guid, transform.position.x, transform.position.z);

            StartCreatureDefaultMovement(world, entity, creatureInfo, creatureCombatInfo);
        });
    }

    void UpdateWorld::HandleUnitUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto deathView = world.View<Components::UnitCombatInfo, Events::UnitDied, Tags::IsAlive>();
        deathView.each([&](entt::entity entity, Components::UnitCombatInfo&, Events::UnitDied& event)
        {
            Util::Movement::Stop(world, entity);

            if (auto* lifecycleInfo = world.TryGet<Components::CreatureLifecycleInfo>(entity))
            {
                Util::Combat::ClearCreatureThreatTable(world, entity);
                world.Remove<Tags::IsInCombat>(entity);
                world.Remove<Events::UnitEnterCombat>(entity);
                if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
                    targetInfo->target = entt::null;
                if (auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity))
                    creatureCombatInfo->isEvading = false;

                lifecycleInfo->state = Components::CreatureLifecycleState::Corpse;
                lifecycleInfo->timeRemaining = CREATURE_CORPSE_DURATION;
            }

            world.Remove<Tags::IsAlive>(entity);
            world.Emplace<Tags::IsDead>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnDied, MetaGen::Server::Lua::CreatureAIEventDataOnDied{
                    .creatureEntity = entt::to_integral(entity),
                    .killerEntity = entt::to_integral(event.killerEntity)
                });
            }
        });
        world.Clear<Events::UnitDied>();

        auto resurrectView = world.View<Components::UnitCombatInfo, Events::UnitResurrected, Tags::IsDead>();
        resurrectView.each([&](entt::entity entity, Components::UnitCombatInfo&, Events::UnitResurrected& event)
        {
            world.Remove<Tags::IsDead>(entity);
            world.Emplace<Tags::IsAlive>(entity);

            if (auto* lifecycleInfo = world.TryGet<Components::CreatureLifecycleInfo>(entity))
            {
                lifecycleInfo->state = Components::CreatureLifecycleState::Alive;
                lifecycleInfo->timeRemaining = 0.0f;
                auto& visibilityUpdateInfo = world.Get<Components::VisibilityUpdateInfo>(entity);
                ShowCreature(visibilityUpdateInfo);

                const auto* creatureInfo = world.TryGet<Components::CreatureInfo>(entity);
                const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity);
                if (creatureInfo && creatureCombatInfo)
                    StartCreatureDefaultMovement(world, entity, *creatureInfo, *creatureCombatInfo);
            }

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnResurrect, MetaGen::Server::Lua::CreatureAIEventDataOnResurrect{
                    .creatureEntity = entt::to_integral(entity),
                    .resurrectorEntity = entt::to_integral(event.resurrectorEntity)
                });
            }
        });
        world.Clear<Events::UnitResurrected>();

        auto lifecycleView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::CreatureCombatInfo,
            Components::CreatureLifecycleInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo,
            Components::TargetInfo, Components::UnitPowersComponent, Components::UnitStatsComponent>();
        lifecycleView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo,
                               Components::CreatureCombatInfo& creatureCombatInfo, Components::CreatureLifecycleInfo& lifecycleInfo,
                               Components::Transform& transform, Components::VisibilityInfo& visibilityInfo,
                               Components::VisibilityUpdateInfo& visibilityUpdateInfo, Components::TargetInfo& targetInfo,
                               Components::UnitPowersComponent& powers, Components::UnitStatsComponent& stats)
        {
            if (lifecycleInfo.state == Components::CreatureLifecycleState::Alive)
                return;

            lifecycleInfo.timeRemaining = std::max(0.0f, lifecycleInfo.timeRemaining - timeState.deltaTime);
            if (lifecycleInfo.timeRemaining > 0.0f)
                return;

            if (lifecycleInfo.state == Components::CreatureLifecycleState::Corpse)
            {
                lifecycleInfo.state = Components::CreatureLifecycleState::Despawned;
                HideCreature(world, gameCache, networkState, entity, objectInfo, visibilityInfo);
                const u64 delayCount = static_cast<u64>(lifecycleInfo.spawnTimeInSecMax - lifecycleInfo.spawnTimeInSecMin) + 1;
                lifecycleInfo.timeRemaining = static_cast<f32>(lifecycleInfo.spawnTimeInSecMin + world.rng.Next() % delayCount);
                return;
            }

            Util::Combat::ClearCreatureThreatTable(world, entity);
            transform.position = creatureCombatInfo.homePosition;
            transform.pitchYaw.y = creatureCombatInfo.homeOrientation;
            world.creatureVisData.Update(objectInfo.guid, transform.position.x, transform.position.z);
            targetInfo.target = entt::null;
            creatureCombatInfo.isEvading = false;
            creatureCombatInfo.timeToNextMeleeAttack = 0.0f;
            ResetCreatureResources(world, entity, powers, stats);

            world.Remove<Tags::IsDead>(entity);
            world.EmplaceOrReplace<Tags::IsAlive>(entity);
            lifecycleInfo.state = Components::CreatureLifecycleState::Alive;
            Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, false,
                MetaGen::Shared::Packet::ServerUnitTeleportPacket{
                    .guid = objectInfo.guid,
                    .position = transform.position,
                    .orientation = transform.pitchYaw.y });
            ShowCreature(visibilityUpdateInfo);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnResurrect, MetaGen::Server::Lua::CreatureAIEventDataOnResurrect{
                    .creatureEntity = entt::to_integral(entity),
                    .resurrectorEntity = entt::to_integral(entt::entity{ entt::null })
                });
            }

            StartCreatureDefaultMovement(world, entity, creatureInfo, creatureCombatInfo);
        });

        auto netFieldUpdateView = world.View<Components::ObjectFields, Components::UnitFields, Components::VisibilityInfo, Events::ObjectNeedsNetFieldUpdate>();
        netFieldUpdateView.each([&](entt::entity entity, Components::ObjectFields& objectFields, Components::UnitFields& unitFields, Components::VisibilityInfo& visibilityInfo)
        {
            PacketWriter writer = world.packetArena.Acquire(1024);
            if (!writer.IsValid())
            {
                NC_LOG_ERROR("Failed to allocate unit net field update packet for entity: {0}", entt::to_integral(entity));
                return;
            }

            Bytebuffer& buffer = writer.GetBuffer();

            bool failed = false;
            auto objectGUID = objectFields.fields.GetField<ObjectGUID>(MetaGen::Shared::NetField::ObjectNetFieldEnum::ObjectGUIDLow);
            const bool objectFieldsWereDirty = objectFields.fields.IsDirty();
            const bool unitFieldsWereDirty = unitFields.fields.IsDirty();

            if (objectFieldsWereDirty)
            {
                failed |= !Util::MessageBuilder::CreatePacket(buffer, (Network::OpcodeType)MetaGen::PacketListEnum::ServerObjectNetFieldUpdatePacket, [&buffer, &objectFields, objectGUID]() -> bool
                    {
                    return buffer.Serialize(objectGUID) && objectFields.fields.SerializeDirtyFields(&buffer, false);
                });
            }

            if (unitFieldsWereDirty)
            {
                failed |= !Util::MessageBuilder::CreatePacket(buffer, (Network::OpcodeType)MetaGen::PacketListEnum::ServerUnitNetFieldUpdatePacket, [&buffer, &unitFields, objectGUID]() -> bool
                    {
                    return buffer.Serialize(objectGUID) && unitFields.fields.SerializeDirtyFields(&buffer, false);
                });
            }

            if (failed)
            {
                NC_LOG_ERROR("Failed to build unit net field update message for entity: {0}", entt::to_integral(entity));
                return;
            }

            bool sendToSelf = objectGUID.GetType() == ObjectGUID::Type::Player;
            if (!Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, sendToSelf, writer.Seal()))
            {
                return;
            }

            if (objectFieldsWereDirty)
                objectFields.fields.ClearDirtyMask();
            if (unitFieldsWereDirty)
                unitFields.fields.ClearDirtyMask();
            world.Remove<Events::ObjectNeedsNetFieldUpdate>(entity);
        });
    }

    void UpdateWorld::HandleProximityTriggerDeinitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();
        auto view = world.View<Components::ProximityTrigger, Events::ProximityTriggerNeedsDeinitialization>();
        view.each([&](entt::entity entity, Components::ProximityTrigger& proximityTrigger, Events::ProximityTriggerNeedsDeinitialization& event)
        {
            if (DeferredDatabaseRetryIsPending(world, entity, timeState))
                return;

            auto result = gameCache.database->DeleteProximityTrigger(event.triggerID);
            if (!result)
            {
                if (ScheduleDeferredDatabaseRetry(world, entity, timeState, result.Failure()))
                    return;
                NotifyAdministrativeDatabaseFailure(world, networkState, AdministrativeRequester(world, entity), "delete_proximity_trigger", result.Failure());
                ClearAdministrativeOperationState(world, entity);
                world.Remove<Events::ProximityTriggerNeedsDeinitialization>(entity);
                return;
            }

            MetaGen::Shared::Packet::ServerTriggerRemovePacket triggerRemovePacket = {
                .triggerID = event.triggerID
            };

            PacketRef triggerRemovePacketRef = Util::Network::CreatePacketBuffer(world.packetArena, triggerRemovePacket);
            if (!triggerRemovePacketRef.IsValid())
                NC_LOG_ERROR("Failed to build trigger remove packet");

            bool isServerSideOnly = (proximityTrigger.flags & MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::IsServerSideOnly) == MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::IsServerSideOnly;

            for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
            {
                if (isServerSideOnly && Util::ProximityTrigger::IsPlayerInTrigger(proximityTrigger, playerEntity))
                    Util::ProximityTrigger::OnExit(world, proximityTrigger, playerEntity);

                if (!triggerRemovePacketRef.IsValid())
                    continue;

                Network::SocketID socketID;
                if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                    continue;

                Util::Network::SendPacket(networkState, socketID, triggerRemovePacketRef);
            }

            world.DestroyEntity(entity);
            Util::ProximityTrigger::RemoveTriggerFromWorld(proximityTriggers, event.triggerID, isServerSideOnly);
        });
    }
    void UpdateWorld::HandleProximityTriggerInitialization(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        auto createView = world.View<Events::ProximityTriggerCreate>();
        createView.each([&](entt::entity entity, Events::ProximityTriggerCreate& event)
        {
            if (DeferredDatabaseRetryIsPending(world, entity, timeState))
                return;

            auto result = gameCache.database->CreateProximityTrigger(Database::ProximityTriggerCreateRequest{
                .name = event.name, .flags = static_cast<u16>(event.flags), .mapID = event.mapID, .position = event.position, .extents = event.extents });
            if (!result)
            {
                if (ScheduleDeferredDatabaseRetry(world, entity, timeState, result.Failure()))
                    return;
                NotifyAdministrativeDatabaseFailure(world, networkState, AdministrativeRequester(world, entity), "create_proximity_trigger", result.Failure());
                world.DestroyEntity(entity);
                return;
            }
            auto trigger = std::move(result).Value();

            Events::ProximityTriggerNeedsInitialization initEvent = {
                .triggerID = trigger.id,

                .name = event.name,
                .flags = event.flags,

                .mapID = event.mapID,
                .position = event.position,
                .extents = event.extents
            };

            world.Emplace<Events::ProximityTriggerNeedsInitialization>(entity, initEvent);
            ClearAdministrativeOperationState(world, entity);
            world.Remove<Events::ProximityTriggerCreate>(entity);
        });

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

            bool isServerSideOnly = (proximityTrigger.flags & MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::IsServerSideOnly) != MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::None;
            if (isServerSideOnly)
            {
                world.Emplace<Tags::ProximityTriggerIsServerSideOnly>(entity);
            }
            else
            {
                world.Emplace<Tags::ProximityTriggerIsClientSide>(entity);

                MetaGen::Shared::Packet::ServerTriggerAddPacket triggerAddPacket = {
                    .triggerID = event.triggerID,

                    .name = event.name,
                    .flags = static_cast<u8>(event.flags),

                    .mapID = event.mapID,
                    .position = event.position,
                    .extents = event.extents
                };

                PacketRef triggerAddPacketRef = Util::Network::CreatePacketBuffer(world.packetArena, triggerAddPacket);
                if (!triggerAddPacketRef.IsValid())
                {
                    NC_LOG_ERROR("Failed to build trigger add packet");
                    return;
                }

                for (const auto& [playerGUID, playerEntity] : world.playerVisData.GetGUIDToEntityMap())
                {
                    Network::SocketID socketID;
                    if (!Util::Network::GetSocketIDFromCharacterID(networkState, playerGUID.GetCounter(), socketID))
                        continue;

                    Util::Network::SendPacket(networkState, socketID, triggerAddPacketRef);
                }
            }

            Util::ProximityTrigger::AddTriggerToWorld(proximityTriggers, entity, event.triggerID, worldAABB, isServerSideOnly);
        });
        world.Clear<Events::ProximityTriggerNeedsInitialization>();
    }
    void UpdateWorld::HandleProximityTriggerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        HandleProximityTriggerDeinitialization(world, zenith, timeState, gameCache, networkState);
        HandleProximityTriggerInitialization(world, zenith, timeState, gameCache, networkState);

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
                worldAABB.max.z);

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

    void UpdateWorld::HandleReplication(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        robin_hood::unordered_set<ObjectGUID> cachedList;

        auto playerView = world.View<Components::ObjectInfo, Components::NetInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        playerView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            if (!Util::Network::IsSocketActive(networkState, netInfo.socketID))
                return;

            visibilityUpdateInfo.timeSinceLastUpdate += timeState.deltaTime;
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

                    entt::entity otherEntity = world.GetEntity(guid);
                    auto& otherObjectInfo = world.Get<Components::ObjectInfo>(otherEntity);
                    auto& otherCharacterInfo = world.Get<Components::CharacterInfo>(otherEntity);
                    auto& otherTransform = world.Get<Components::Transform>(otherEntity);

                    PacketWriter writer = world.packetArena.Acquire(2048);
                    if (!writer.IsValid())
                        return true;

                    Bytebuffer& buffer = writer.GetBuffer();
                    if (!Util::MessageBuilder::Unit::BuildUnitAdd(buffer, otherObjectInfo.guid, otherCharacterInfo.name, otherCharacterInfo.unitClass, otherTransform.position, otherTransform.scale, otherTransform.pitchYaw))
                        return true;

                    if (!Util::MessageBuilder::Unit::BuildUnitBaseInfo(buffer, *world.registry, otherEntity,
                            otherObjectInfo.guid, *gameCache.factionRuntimeData))
                        return true;

                    if (networkState.server->SendPacket(netInfo.socketID, writer.Seal()))
                        visiblePlayers.insert(guid);

                    return true;
                });

                for (ObjectGUID& prevVisibleGUID : cachedList)
                {
                    if (Util::Network::SendPacket(networkState, netInfo.socketID, MetaGen::Shared::Packet::ServerUnitRemovePacket{ .guid = prevVisibleGUID }))
                    {
                        visiblePlayers.erase(prevVisibleGUID);
                    }
                }
            }

            // Query nearby units
            {
                robin_hood::unordered_set<ObjectGUID>& visibleCreatures = visibilityInfo.visibleCreatures;
                cachedList = visibleCreatures;

                world.GetCreaturesInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                    {
                    entt::entity creatureEntity = world.GetEntity(guid);
                    if (!world.ValidEntity(creatureEntity) || !CanPlayerSeeCreature(world, gameCache, entity, creatureEntity))
                        return true;

                    if (cachedList.contains(guid))
                    {
                        cachedList.erase(guid);
                        return true;
                    }

                    return true;
                });

                if (cachedList.size() > 0)
                {
                    PacketWriter writer = world.packetArena.Acquire(8192);
                    if (!writer.IsValid())
                        return;

                    Bytebuffer& buffer = writer.GetBuffer();
                    std::vector<ObjectGUID> removedCreatures;
                    removedCreatures.reserve(cachedList.size());

                    for (ObjectGUID creatureGUID : cachedList)
                    {
                        if (!Util::Network::AppendPacketToBuffer(buffer, MetaGen::Shared::Packet::ServerUnitRemovePacket{ .guid = creatureGUID }))
                        {
                            continue;
                        }

                        removedCreatures.push_back(creatureGUID);
                    }

                    if (buffer.writtenData > 0 && networkState.server->SendPacket(netInfo.socketID, writer.Seal()))
                    {
                        for (ObjectGUID creatureGUID : removedCreatures)
                        {
                            visibleCreatures.erase(creatureGUID);
                        }
                    }
                }
            }

            visibilityUpdateInfo.timeSinceLastUpdate = 0.0f;
        });

        robin_hood::unordered_set<ObjectGUID> playersEnteredInView;
        playersEnteredInView.reserve(cachedList.size());

        auto creatureView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::Transform, Components::VisibilityInfo, Components::VisibilityUpdateInfo>();
        creatureView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo, Components::Transform& transform, Components::VisibilityInfo& visibilityInfo, Components::VisibilityUpdateInfo& visibilityUpdateInfo)
        {
            visibilityUpdateInfo.timeSinceLastUpdate += timeState.deltaTime;
            if (visibilityUpdateInfo.timeSinceLastUpdate < visibilityUpdateInfo.updateInterval)
                return;

            robin_hood::unordered_set<ObjectGUID>& visiblePlayers = visibilityInfo.visiblePlayers;
            cachedList = visiblePlayers;

            playersEnteredInView.clear();

            world.GetPlayersInRadius(vec2(transform.position.x, transform.position.z), World::DEFAULT_VISIBILITY_DISTANCE, [&](const ObjectGUID guid) -> bool
                {
                entt::entity playerEntity = world.GetEntity(guid);
                if (!world.ValidEntity(playerEntity) || !CanPlayerSeeCreature(world, gameCache, playerEntity, entity))
                    return true;

                if (cachedList.contains(guid))
                {
                    cachedList.erase(guid);
                    return true;
                }

                playersEnteredInView.insert(guid);
                return true;
            });

            if (playersEnteredInView.size() > 0)
            {
                PacketWriter writer = world.packetArena.Acquire(2048);
                if (!writer.IsValid())
                    return;

                Bytebuffer& buffer = writer.GetBuffer();
                if (!Util::MessageBuilder::Unit::BuildUnitAdd(buffer, objectInfo.guid, creatureInfo.name, creatureInfo.unitClass, transform.position, transform.scale, transform.pitchYaw))
                    return;

                if (!Util::MessageBuilder::Unit::BuildUnitBaseInfo(buffer, *world.registry, entity, objectInfo.guid,
                        *gameCache.factionRuntimeData))
                    return;

                PacketRef packet = writer.Seal();

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
                        if (networkState.server->SendPacket(playerNetInfo->socketID, packet))
                        {
                            visibilityInfo.visiblePlayers.insert(playerGUID);
                            playerVisibilityInfo.visibleCreatures.insert(objectInfo.guid);
                        }
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

        if (!gameCache.factionRuntimeData)
            return;

        auto factionUpdateView = world.View<const Components::ObjectInfo, const Components::UnitFaction, const Components::VisibilityInfo, Events::UnitNeedsFactionUpdate>();
        factionUpdateView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::UnitFaction& unitFaction, const Components::VisibilityInfo& visibilityInfo)
        {
            const Gameplay::Faction::FactionID factionID = gameCache.factionRuntimeData->GetFactionID(unitFaction.effectiveFaction);
            const auto sendOptions = Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitFactionUpdatePacket::PACKET_ID, objectInfo.guid);
            MetaGen::Shared::Packet::ServerUnitFactionUpdatePacket packet{
                .guid = objectInfo.guid,
                .factionID = factionID,
                .playerReactionBounds = unitFaction.effectivePlayerReactionBounds
            };

            if (Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, sendOptions, packet))
            {
                world.Remove<Events::UnitNeedsFactionUpdate>(entity);
            }
        });
    }
    void UpdateWorld::HandleContainerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto view = world.View<Components::ObjectInfo, Components::NetInfo, Components::VisibilityInfo, Components::PlayerContainers, Events::CharacterNeedsContainerUpdate>();
        view.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::NetInfo& netInfo, Components::VisibilityInfo& visibilityInfo, Components::PlayerContainers& playerContainers)
        {
            u64 characterID = objectInfo.guid.GetCounter();
            PacketWriter containerUpdateWriter = world.packetArena.Acquire(4096);
            PacketWriter equippedItemsWriter = world.packetArena.Acquire(512);
            if (!containerUpdateWriter.IsValid() || !equippedItemsWriter.IsValid())
            {
                NC_LOG_ERROR("Failed to allocate character container update packet for character: {0}", characterID);
                networkState.server->CloseSocketID(netInfo.socketID);
                return;
            }

            Bytebuffer& containerUpdateBuffer = containerUpdateWriter.GetBuffer();
            Bytebuffer& equippedItemsBuffer = equippedItemsWriter.GetBuffer();

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

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerAddPacket{
                        .guid = equipmentContainerItem.objectGUID,
                        .index = containerIndex,
                        .itemID = containerItemInstance->itemID,
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
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerRemoveFromSlotPacket{
                            .index = containerIndex,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerAddToSlotPacket{
                            .guid = containerItem.objectGUID,
                            .index = containerIndex,
                            .slot = slotIndex
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

                    failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerAddPacket{
                        .guid = ObjectGUID::Empty,
                        .index = PLAYER_BASE_CONTAINER_ID,
                        .itemID = 0,
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
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerRemoveFromSlotPacket{
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex
                        });
                    }
                    else
                    {
                        failed |= !Util::Network::AppendPacketToBuffer(containerUpdateBuffer, MetaGen::Shared::Packet::ServerContainerAddToSlotPacket{
                            .guid = containerItem.objectGUID,
                            .index = PLAYER_BASE_CONTAINER_ID,
                            .slot = slotIndex
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

                        failed |= !Util::Network::AppendPacketToBuffer(equippedItemsBuffer, MetaGen::Shared::Packet::ServerUnitEquippedItemUpdatePacket{
                            .guid = objectInfo.guid,
                            .slot = static_cast<u8>(slotIndex),
                            .itemID = itemID
                        });
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

            if (equippedItemsBuffer.writtenData > 0)
            {
                Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, equippedItemsWriter.Seal());
            }

            if (containerUpdateBuffer.writtenData > 0)
            {
                networkState.server->SendPacket(netInfo.socketID, containerUpdateWriter.Seal());
            }
        });

        world.Clear<Events::CharacterNeedsContainerUpdate>();
    }
    void UpdateWorld::HandleDisplayUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState)
    {
        auto visualItemView = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::UnitVisualItems, Events::CharacterNeedsVisualItemUpdate>();
        visualItemView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::VisibilityInfo& visibilityInfo, Components::UnitVisualItems& visualItems)
        {
            bool sentAllUpdates = true;
            for (ItemEquipSlot_t slot : visualItems.dirtyItemIDs)
            {
                sentAllUpdates &= ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitVisualItemUpdatePacket{ .guid = objectInfo.guid, .slot = slot, .itemID = visualItems.equippedItemIDs[slot] });
            }

            if (sentAllUpdates)
            {
                visualItems.dirtyItemIDs.clear();
                world.Remove<Events::CharacterNeedsVisualItemUpdate>(entity);
            }
        });
    }
    void UpdateWorld::HandleCombatUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState)
    {
        auto enterCombatView = world.View<Components::ObjectInfo, Components::UnitCombatInfo, Events::UnitEnterCombat>();
        enterCombatView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::UnitCombatInfo& unitCombatInfo, Events::UnitEnterCombat& event)
        {
            world.EmplaceOrReplace<Tags::IsInCombat>(entity);

            if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(entity))
            {
                zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnEnterCombat, MetaGen::Server::Lua::CreatureAIEventDataOnEnterCombat{ .creatureEntity = entt::to_integral(entity) });
            }

            if (auto* creatureInfo = world.TryGet<Components::CreatureInfo>(entity))
            {
                if (auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity))
                {
                    creatureCombatInfo->timeToNextMeleeAttack = 0.0f;
                    Util::Movement::Follow(world, entity, event.target, Util::Movement::FollowParams{ .speed = creatureInfo->runSpeed, .stopDistance = Util::Movement::DEFAULT_FOLLOW_DISTANCE });
                }
            }
        });
        world.Clear<Events::UnitEnterCombat>();

        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();
        auto creatureMeleeView = world.View<Components::ObjectInfo, Components::CreatureInfo, Components::CreatureCombatInfo,
            Components::CreatureThreatTable, Components::Transform, Components::TargetInfo, Tags::IsInCombat, Tags::IsAlive>();
        creatureMeleeView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::CreatureInfo& creatureInfo,
                                   Components::CreatureCombatInfo& creatureCombatInfo, Components::CreatureThreatTable& threatTable,
                                   Components::Transform& transform, Components::TargetInfo& targetInfo)
        {
            const vec3 homeOffset = transform.position - creatureCombatInfo.homePosition;
            const f32 horizontalHomeDistanceSq = homeOffset.x * homeOffset.x + homeOffset.z * homeOffset.z;
            if (horizontalHomeDistanceSq > creatureCombatInfo.leashRange * creatureCombatInfo.leashRange ||
                std::abs(homeOffset.y) > creatureCombatInfo.leashRange)
            {
                creatureCombatInfo.isEvading = true;
                targetInfo.target = entt::null;
                Util::Movement::Stop(world, entity);
                Util::Combat::ClearCreatureThreatTable(world, entity);
                return;
            }

            creatureCombatInfo.timeToNextMeleeAttack = std::max(0.0f, creatureCombatInfo.timeToNextMeleeAttack - timeState.deltaTime);

            entt::entity targetEntity = entt::null;
            while (!threatTable.threatList.empty())
            {
                const ObjectGUID targetGUID = threatTable.threatList.front().guid;
                targetEntity = world.GetEntity(targetGUID);
                if (world.ValidEntity(targetEntity) &&
                    world.AllOf<Components::ObjectInfo, Components::Transform, Tags::IsAlive>(targetEntity))
                {
                    break;
                }

                Util::Combat::RemoveUnitFromThreatTable(threatTable, targetGUID);
                targetEntity = entt::null;
            }

            if (targetEntity == entt::null)
            {
                targetInfo.target = entt::null;
                return;
            }

            targetInfo.target = targetEntity;
            const auto* movementIntent = world.TryGet<Components::MovementIntent>(entity);
            const bool needsFollowIntent = !movementIntent ||
                                           movementIntent->type != Components::MovementIntentType::Follow ||
                                           movementIntent->follow.target != targetEntity;
            if (needsFollowIntent)
            {
                Util::Movement::Follow(world, entity, targetEntity, Util::Movement::FollowParams{ .speed = creatureInfo.runSpeed, .stopDistance = Util::Movement::DEFAULT_FOLLOW_DISTANCE });
            }

            const auto& targetTransform = world.Get<Components::Transform>(targetEntity);
            if (!IsWithinCreatureMeleeRange(transform, targetTransform) || creatureCombatInfo.timeToNextMeleeAttack > 0.0f)
                return;

            const auto* targetStats = world.TryGet<Components::UnitStatsComponent>(targetEntity);
            const u32 damage = CalculateCreatureMeleeDamage(world, creatureInfo, creatureCombatInfo, targetStats);
            const auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);
            Util::Combat::RefreshCreatureCombatParticipants(world, entity, creatureCombatInfo.leashRange);
            Util::CombatEvent::AddDamageEvent(combatEventState, objectInfo.guid, targetObjectInfo.guid, damage);
            creatureCombatInfo.timeToNextMeleeAttack = creatureCombatInfo.meleeAttackTime;
        });
    }

    void UpdateWorld::HandleCombatTimeoutUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState)
    {
        auto combatView = world.View<Components::ObjectInfo, Components::UnitCombatInfo, Tags::IsInCombat>();
        combatView.each([&](entt::entity entity, Components::ObjectInfo& objectInfo, Components::UnitCombatInfo& unitCombatInfo)
        {
            for (auto itr = unitCombatInfo.threatTables.begin(); itr != unitCombatInfo.threatTables.end();)
            {
                ThreatTableEntry& threatTableEntry = itr->second;

                entt::entity threatEntity = world.GetEntity(itr->first);
                if (world.ValidEntity(threatEntity))
                {
                    auto& creatureThreatTable = world.Get<Components::CreatureThreatTable>(threatEntity);
                    if (creatureThreatTable.allowDropCombat)
                    {
                        threatTableEntry.timeSinceLastActivity += timeState.deltaTime;
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
                    threatTableEntry.timeSinceLastActivity += timeState.deltaTime;
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
                zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnLeaveCombat, MetaGen::Server::Lua::CreatureAIEventDataOnLeaveCombat{ .creatureEntity = entt::to_integral(entity) });
            }

            if (auto* creatureInfo = world.TryGet<Components::CreatureInfo>(entity))
            {
                auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity);
                if (creatureCombatInfo && world.AllOf<Tags::IsAlive>(entity))
                {
                    creatureCombatInfo->isEvading = true;
                    if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
                        targetInfo->target = entt::null;

                    Util::Movement::MoveTo(world, entity, creatureCombatInfo->homePosition, Util::Movement::PointParams{ .speed = creatureInfo->runSpeed, .repathDistance = CREATURE_HOME_REACHED_DISTANCE, .clearOnReach = true });
                }
                else
                {
                    Util::Movement::Stop(world, entity);
                }
            }
        });
    }
    void UpdateWorld::HandlePowerUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::NetworkState& networkState)
    {
        static constexpr f32 HEALTH_REGEN_INTERVAL = 0.5f;

        auto healthUpdateView = world.View<Components::ObjectInfo, Components::VisibilityInfo, Components::UnitPowersComponent, Tags::IsMissingHealth, Tags::IsAlive>();
        healthUpdateView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::VisibilityInfo& visibilityInfo, Components::UnitPowersComponent& unitPowersComponent)
        {
            unitPowersComponent.timeToNextUpdate = unitPowersComponent.timeToNextUpdate - timeState.deltaTime;
            if (unitPowersComponent.timeToNextUpdate > 0.0f)
                return;

            unitPowersComponent.timeToNextUpdate += HEALTH_REGEN_INTERVAL;

            // Disalllow Health Regen while in combat
            if (world.AllOf<Tags::IsInCombat>(entity))
                return;
            if (const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity);
                creatureCombatInfo && creatureCombatInfo->isEvading)
            {
                return;
            }

            UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);

            bool isCreature = world.AllOf<Tags::IsCreature>(entity);
            f64 baseRegenRate = isCreature ? (healthPower.base * 0.75) : (healthPower.base * 0.01);
            f64 regenAmount = baseRegenRate * HEALTH_REGEN_INTERVAL;

            f64 newHealth = glm::clamp(healthPower.current + regenAmount, healthPower.current, healthPower.max);
            Util::Unit::SetPower(world, entity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);
        });

        auto powerUpdateView = world.View<Components::ObjectInfo, Components::UnitPowersComponent>();
        powerUpdateView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, Components::UnitPowersComponent& unitPowersComponent)
        {
            if (world.AllOf<Tags::IsDead>(entity))
                return;

            if (const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(entity);
                creatureCombatInfo && creatureCombatInfo->isEvading)
            {
                return;
            }

            for (auto& pair : unitPowersComponent.powerTypeToValue)
            {
                MetaGen::Shared::Unit::PowerTypeEnum powerType = pair.first;
                UnitPower& power = pair.second;
                if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Health)
                    continue;

                // Mana Regen
                if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Mana)
                {
                    if (power.current >= power.max)
                        continue;

                    f64 baseRegenRate = power.base * 0.05;
                    f64 regenAmount = baseRegenRate * timeState.deltaTime;
                    f64 newMana = glm::clamp(power.current + regenAmount, power.current, power.max);
                    Util::Unit::SetPower(world, entity, unitPowersComponent, powerType, power.base, newMana, power.max);
                }
                // Focus/Energy Regen
                else if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Focus || powerType == MetaGen::Shared::Unit::PowerTypeEnum::Energy)
                {
                    if (power.current >= power.max)
                        continue;

                    f64 baseRegenRate = 10.0;
                    f64 regenAmount = baseRegenRate * timeState.deltaTime;
                    f64 newEnergy = glm::clamp(power.current + regenAmount, power.current, power.max);
                    Util::Unit::SetPower(world, entity, unitPowersComponent, powerType, power.base, newEnergy, power.max);
                }
                // Rage Regen
                else if (powerType == MetaGen::Shared::Unit::PowerTypeEnum::Rage)
                {
                    bool isInCombat = world.AllOf<Tags::IsInCombat>(entity);
                    f64 baseRegenRate = (1.0 * isInCombat) + (-1.0 * !isInCombat);
                    f64 regenAmount = baseRegenRate * timeState.deltaTime;
                    f64 newRage = 0.0f;

                    if (isInCombat)
                    {
                        newRage = glm::clamp(power.current + regenAmount, power.current, power.max);
                    }
                    else
                    {
                        newRage = glm::clamp(power.current + regenAmount, 0.0, power.current);
                    }

                    if (newRage >= power.max)
                        continue;

                    Util::Unit::SetPower(world, entity, unitPowersComponent, powerType, power.base, newRage, power.max);
                }
            }
        });

        auto dirtyPowerView = world.View<const Components::ObjectInfo, const Components::VisibilityInfo, Components::UnitPowersComponent, Events::UnitNeedsPowerUpdate>();
        dirtyPowerView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::VisibilityInfo& visibilityInfo, Components::UnitPowersComponent& unitPowersComponent)
        {
            bool sentAllUpdates = true;
            for (MetaGen::Shared::Unit::PowerTypeEnum dirtyPowerType : unitPowersComponent.dirtyPowerTypes)
            {
                UnitPower& power = Util::Unit::GetPower(unitPowersComponent, dirtyPowerType);

                sentAllUpdates &= ECS::Util::Network::SendToNearby(networkState, world, entity, visibilityInfo, true, ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitPowerUpdatePacket::PACKET_ID, objectInfo.guid, static_cast<u8>(dirtyPowerType)), MetaGen::Shared::Packet::ServerUnitPowerUpdatePacket{ .guid = objectInfo.guid, .kind = static_cast<u8>(dirtyPowerType), .base = power.base, .current = power.current, .max = power.max });
            }

            if (sentAllUpdates)
            {
                unitPowersComponent.dirtyPowerTypes.clear();
                world.Remove<Events::UnitNeedsPowerUpdate>(entity);
            }
        });

        auto dirtyResistanceView = world.View<const Components::ObjectInfo, const Components::NetInfo, Components::UnitResistancesComponent, Events::CharacterNeedsResistanceUpdate>();
        dirtyResistanceView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::NetInfo& netInfo, Components::UnitResistancesComponent& unitResistancesComponent)
        {
            bool sentAllUpdates = true;
            for (MetaGen::Shared::Unit::ResistanceTypeEnum dirtyResistanceType : unitResistancesComponent.dirtyResistanceTypes)
            {
                UnitResistance& resistance = Util::Unit::GetResistance(unitResistancesComponent, dirtyResistanceType);

                sentAllUpdates &= ECS::Util::Network::SendPacket(networkState, netInfo.socketID, ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitResistanceUpdatePacket::PACKET_ID, ObjectGUID::Empty, static_cast<u8>(dirtyResistanceType)), MetaGen::Shared::Packet::ServerUnitResistanceUpdatePacket{ .kind = static_cast<u8>(dirtyResistanceType), .base = resistance.base, .current = resistance.current, .max = resistance.max });
            }

            if (sentAllUpdates)
            {
                unitResistancesComponent.dirtyResistanceTypes.clear();
                world.Remove<Events::CharacterNeedsResistanceUpdate>(entity);
            }
        });

        auto dirtyStatsView = world.View<const Components::ObjectInfo, const Components::NetInfo, Components::UnitStatsComponent, Events::CharacterNeedsStatUpdate>();
        dirtyStatsView.each([&](entt::entity entity, const Components::ObjectInfo& objectInfo, const Components::NetInfo& netInfo, Components::UnitStatsComponent& unitStatsComponent)
        {
            bool sentAllUpdates = true;
            for (MetaGen::Shared::Unit::StatTypeEnum dirtyStatType : unitStatsComponent.dirtyStatTypes)
            {
                UnitStat& stat = Util::Unit::GetStat(unitStatsComponent, dirtyStatType);

                sentAllUpdates &= ECS::Util::Network::SendPacket(networkState, netInfo.socketID, ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitStatUpdatePacket::PACKET_ID, ObjectGUID::Empty, static_cast<u8>(dirtyStatType)), MetaGen::Shared::Packet::ServerUnitStatUpdatePacket{ .kind = static_cast<u8>(dirtyStatType), .base = stat.base, .current = stat.current });
            }

            if (sentAllUpdates)
            {
                unitStatsComponent.dirtyStatTypes.clear();
                world.Remove<Events::CharacterNeedsStatUpdate>(entity);
            }
        });
    }
    void UpdateWorld::HandleAuraUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto prepareView = world.View<Components::AuraInfo, Components::AuraEffectInfo, Tags::IsUnpreparedAura>();
        prepareView.each([&](entt::entity entity, Components::AuraInfo& auraInfo, Components::AuraEffectInfo& auraEffectInfo)
        {
            GameDefine::Database::Spell* spell = nullptr;
            if (!Util::Cache::GetSpellByID(gameCache, auraInfo.spellID, spell))
            {
                world.Emplace<Events::AuraExpired>(entity);
                return;
            }

            entt::entity casterEntity = world.GetEntity(auraInfo.caster);
            entt::entity targetEntity = world.GetEntity(auraInfo.target);
            Database::SpellEffectInfo* dbSpellEffectInfo = nullptr;

            if (!world.ValidEntity(targetEntity) || !Util::Cache::GetSpellEffectsBySpellID(gameCache, auraInfo.spellID, dbSpellEffectInfo))
            {
                world.Emplace<Events::AuraExpired>(entity);
                return;
            }

            u8 numEffects = static_cast<u8>(dbSpellEffectInfo->effects.size());
            auraEffectInfo.effects.resize(numEffects);
            auraEffectInfo.periodicEffects.reserve(numEffects);

            for (u8 i = 0; i < numEffects; i++)
            {
                GameDefine::Database::SpellEffect& spellEffect = dbSpellEffectInfo->effects[i];

                AuraEffect& auraEffect = auraEffectInfo.effects[i];
                auraEffect.effectID = spellEffect.id;
                auraEffect.priority = spellEffect.effectPriority;
                auraEffect.type = spellEffect.effectType;

                auraEffect.value1 = spellEffect.effectValue1;
                auraEffect.value2 = spellEffect.effectValue2;
                auraEffect.value3 = spellEffect.effectValue3;

                auraEffect.miscValue1 = spellEffect.effectMiscValue1;
                auraEffect.miscValue2 = spellEffect.effectMiscValue2;
                auraEffect.miscValue3 = spellEffect.effectMiscValue3;

                // TODO : Provide a better way to determine if an effect is periodic
                if (auraEffect.type != (u8)MetaGen::Shared::Spell::SpellEffectTypeEnum::AuraPeriodicDamage &&
                    auraEffect.type != (u8)MetaGen::Shared::Spell::SpellEffectTypeEnum::AuraPeriodicHeal)
                    continue;

                auraEffectInfo.periodicEffectsMask |= (1ull << i);

                f32 interval = static_cast<f32>(auraEffect.value1) / 1000.0f;
                auraEffectInfo.periodicEffects.push_back(AuraPeriodicInfo{
                    .interval = interval,
                    .timeToNextTick = interval,
                    .index = i });
            }

            u32 numPeriodicEffects = static_cast<u32>(auraEffectInfo.periodicEffects.size());
            if (numPeriodicEffects > 0)
                world.Emplace<Tags::IsPeriodicAura>(entity);

            // Handle Prepare Spell
            bool prepared = zenith->CallEventBool(MetaGen::Server::Lua::AuraEvent::OnApply, MetaGen::Server::Lua::AuraEventDataOnApply{
                .casterID = entt::to_integral(casterEntity),
                .targetID = entt::to_integral(targetEntity),
                .auraEntity = entt::to_integral(entity),
                .spellID = auraInfo.spellID
            });

            u64 procEffectsMask = 0;
            if (Util::Spell::SetupAuraProcInfo(world, gameCache, auraEffectInfo, auraInfo.spellID, entity))
            {
                auto& spellProcInfo = world.Get<Components::SpellProcInfo>(entity);
                procEffectsMask = spellProcInfo.procEffectsMask;

                Util::Spell::CheckAuraProc(world, zenith, timeState, gameCache, auraEffectInfo, spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnAuraApply, auraInfo.spellID, entity, casterEntity, targetEntity);
            }

            if (!prepared)
            {
                world.Emplace<Events::AuraExpired>(entity);
                return;
            }

            if (gameCache.factionRuntimeData)
                Util::FactionModifier::ApplyAura(world, entity, targetEntity, auraInfo, auraEffectInfo, *gameCache.factionRuntimeData);

            auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity);

            // Handle Non-Periodic Effects Immediately
            for (u32 i = 0; i < numEffects; i++)
            {
                AuraEffect& auraEffect = auraEffectInfo.effects[i];

                // TODO : Provide a better way to determine if an effect is periodic
                bool isProcEffect = (procEffectsMask & (1ull << i)) != 0;
                bool isPeriodicAura = (auraEffectInfo.periodicEffectsMask & (1ull << i)) != 0;
                if (isProcEffect || isPeriodicAura)
                    continue;

                zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnHandleEffect, MetaGen::Server::Lua::AuraEventDataOnHandleEffect{
                    .casterID = entt::to_integral(casterEntity),
                    .targetID = entt::to_integral(targetEntity),
                    .auraEntity = entt::to_integral(entity),
                    .spellID = auraInfo.spellID,
                    .effectIndex = static_cast<u8>(i),
                    .effectType = auraEffect.type
                });

                if (spellProcInfo)
                {
                    Util::Spell::CheckAuraEffectProc(world, zenith, timeState, gameCache, auraEffectInfo, *spellProcInfo, auraInfo.spellID, entity, casterEntity, targetEntity, i);
                }
            }

            auto& objectInfo = world.Get<Components::ObjectInfo>(targetEntity);
            auto& visibilityInfo = world.Get<Components::VisibilityInfo>(targetEntity);
            ECS::Util::Network::SendToNearby(networkState, world, targetEntity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitAddAuraPacket{ .guid = objectInfo.guid, .auraInstanceID = entt::to_integral(entity), .spellID = auraInfo.spellID, .duration = auraInfo.timeRemaining, .stacks = auraInfo.stacks });

            world.Emplace<Tags::IsActiveAura>(entity);
        });
        world.Clear<Tags::IsUnpreparedAura>();

        auto refreshedView = world.View<Components::AuraInfo, Components::AuraEffectInfo, Events::AuraRefreshed>();
        refreshedView.each([&](entt::entity entity, Components::AuraInfo& auraInfo, Components::AuraEffectInfo& auraEffectInfo)
        {
            entt::entity casterEntity = world.GetEntity(auraInfo.caster);
            entt::entity targetEntity = world.GetEntity(auraInfo.target);
            if (!world.ValidEntity(targetEntity))
            {
                world.Emplace<Events::AuraExpired>(entity);
                return;
            }

            zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnApply, MetaGen::Server::Lua::AuraEventDataOnApply{
                .casterID = entt::to_integral(casterEntity),
                .targetID = entt::to_integral(targetEntity),
                .auraEntity = entt::to_integral(entity),
                .spellID = auraInfo.spellID
            });

            if (auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity))
            {
                Util::Spell::CheckAuraProc(world, zenith, timeState, gameCache, auraEffectInfo, *spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnAuraApply, auraInfo.spellID, entity, casterEntity, targetEntity);
            }

            if (gameCache.factionRuntimeData)
                Util::FactionModifier::RefreshAura(world, entity, targetEntity, auraInfo, auraEffectInfo, *gameCache.factionRuntimeData);

            auto& objectInfo = world.Get<Components::ObjectInfo>(targetEntity);
            auto& visibilityInfo = world.Get<Components::VisibilityInfo>(targetEntity);
            ECS::Util::Network::SendToNearby(networkState, world, targetEntity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitUpdateAuraPacket{ .guid = objectInfo.guid, .auraInstanceID = entt::to_integral(entity), .duration = auraInfo.timeRemaining, .stacks = auraInfo.stacks });
        });
        world.Clear<Events::AuraRefreshed>();

        auto activeView = world.View<Components::AuraInfo, Components::AuraEffectInfo, Tags::IsActiveAura>();
        activeView.each([&](entt::entity entity, Components::AuraInfo& auraInfo, Components::AuraEffectInfo& auraEffectInfo)
        {
            if (auraInfo.duration != -1.0f)
                auraInfo.timeRemaining = glm::max(0.0f, auraInfo.timeRemaining - timeState.deltaTime);

            bool isPeriodicAura = world.AllOf<Tags::IsPeriodicAura>(entity);
            if (isPeriodicAura)
            {
                entt::entity casterEntity = world.GetEntity(auraInfo.caster);
                entt::entity targetEntity = world.GetEntity(auraInfo.target);
                if (!world.ValidEntity(targetEntity))
                {
                    world.Remove<Tags::IsActiveAura>(entity);
                    world.Emplace<Events::AuraExpired>(entity);
                    return;
                }

                auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity);

                u8 numEffects = static_cast<u8>(auraEffectInfo.periodicEffects.size());
                for (u8 i = 0; i < numEffects; i++)
                {
                    AuraPeriodicInfo& periodicInfo = auraEffectInfo.periodicEffects[i];

                    // TODO : timeToNextTick, should be capped by the max ticks left (This is needed for when deltaTime spikes, to avoid too many ticks from happening)
                    periodicInfo.timeToNextTick -= timeState.deltaTime;
                    while (periodicInfo.timeToNextTick < 0)
                    {
                        const AuraEffect& auraEffect = auraEffectInfo.effects[periodicInfo.index];

                        zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnHandleEffect, MetaGen::Server::Lua::AuraEventDataOnHandleEffect{
                            .casterID = entt::to_integral(casterEntity),
                            .targetID = entt::to_integral(targetEntity),
                            .auraEntity = entt::to_integral(entity),
                            .spellID = auraInfo.spellID,
                            .procID = 0,
                            .effectIndex = periodicInfo.index,
                            .effectType = auraEffect.type
                        });

                        if (spellProcInfo)
                        {
                            Util::Spell::CheckAuraEffectProc(world, zenith, timeState, gameCache, auraEffectInfo, *spellProcInfo, auraInfo.spellID, entity, casterEntity, targetEntity, periodicInfo.index);
                        }

                        periodicInfo.timeToNextTick += periodicInfo.interval;
                    }
                }
            }

            if (auraInfo.timeRemaining == 0.0f)
            {
                world.Remove<Tags::IsActiveAura>(entity);
                world.Emplace<Events::AuraExpired>(entity);
            }
        });

        auto expiredView = world.View<Components::AuraInfo, Components::AuraEffectInfo, Events::AuraExpired>();
        expiredView.each([&](entt::entity entity, Components::AuraInfo& auraInfo, Components::AuraEffectInfo& auraEffectInfo)
        {
            entt::entity casterEntity = world.GetEntity(auraInfo.caster);
            entt::entity targetEntity = world.GetEntity(auraInfo.target);
            if (world.ValidEntity(targetEntity))
            {
                auto& characterAuraInfo = world.Get<Components::UnitAuraInfo>(targetEntity);

                zenith->CallEvent(MetaGen::Server::Lua::AuraEvent::OnRemove, MetaGen::Server::Lua::AuraEventDataOnRemove{
                    .casterID = entt::to_integral(casterEntity),
                    .targetID = entt::to_integral(targetEntity),
                    .auraEntity = entt::to_integral(entity),
                    .spellID = auraInfo.spellID
                });

                if (auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity))
                {
                    Util::Spell::CheckAuraProc(world, zenith, timeState, gameCache, auraEffectInfo, *spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnAuraRemove, auraInfo.spellID, entity, casterEntity, targetEntity);
                }

                Util::FactionModifier::RemoveAura(world, entity, targetEntity);

                // Remove from active auras
                auto itr = characterAuraInfo.spellIDToAuraEntity.find(auraInfo.spellID);
                if (itr != characterAuraInfo.spellIDToAuraEntity.end() && itr->second == entity)
                    characterAuraInfo.spellIDToAuraEntity.erase(itr);

                auto& objectInfo = world.Get<Components::ObjectInfo>(targetEntity);
                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(targetEntity);
                ECS::Util::Network::SendToNearby(networkState, world, targetEntity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitRemoveAuraPacket{ .guid = objectInfo.guid, .auraInstanceID = entt::to_integral(entity) });
            }

            world.DestroyEntity(entity);
        });
    }
    void UpdateWorld::HandleSpellUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        u64 currentTimeInMS = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

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

            Database::SpellEffectInfo* dbSpellEffectInfo = nullptr;
            if (Util::Cache::GetSpellEffectsBySpellID(gameCache, spellInfo.spellID, dbSpellEffectInfo))
            {
                spellEffectInfo.regularEffectsMask = dbSpellEffectInfo->regularEffectsMask;
                spellEffectInfo.effects = dbSpellEffectInfo->effects;
            }

            // Handle Prepare Spell
            bool prepared = zenith->CallEventBool(MetaGen::Server::Lua::SpellEvent::OnPrepare, MetaGen::Server::Lua::SpellEventDataOnPrepare{
                .casterID = entt::to_integral(casterEntity),
                .spellEntity = entt::to_integral(entity),
                .spellID = spellInfo.spellID
            });

            if (!prepared)
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            if (Util::Spell::SetupSpellProcInfo(world, gameCache, spellInfo.spellID, entity))
            {
                auto& spellProcInfo = world.Get<Components::SpellProcInfo>(entity);
                Util::Spell::CheckSpellProc(world, zenith, timeState, gameCache, spellEffectInfo, spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnSpellCast, spellInfo.spellID, entity, casterEntity);
            }

            if (const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(casterEntity);
                creatureCombatInfo && world.AllOf<Tags::IsInCombat>(casterEntity))
            {
                Util::Combat::RefreshCreatureCombatParticipants(world, casterEntity, creatureCombatInfo->leashRange);
            }

            auto& visibilityInfo = world.Get<Components::VisibilityInfo>(casterEntity);
            ECS::Util::Network::SendToNearby(networkState, world, casterEntity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitCastSpellPacket{ .guid = spellInfo.caster, .spellID = spellInfo.spellID, .castTime = spellInfo.castTime, .timeToCast = spellInfo.timeToCast });

            world.Emplace<Tags::IsActiveSpell>(entity);
        });
        world.Clear<Tags::IsUnpreparedSpell>();

        auto activeView = world.View<Components::SpellInfo, Components::SpellEffectInfo, Tags::IsActiveSpell>();
        activeView.each([&](entt::entity entity, Components::SpellInfo& spellInfo, Components::SpellEffectInfo& spellEffectInfo)
        {
            spellInfo.timeToCast = glm::max(0.0f, spellInfo.timeToCast - timeState.deltaTime);
            if (spellInfo.timeToCast > 0.0f)
                return;

            entt::entity casterEntity = world.GetEntity(spellInfo.caster);
            if (!world.ValidEntity(casterEntity))
            {
                world.Emplace<Tags::IsCompleteSpell>(entity);
                return;
            }

            auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity);

            u64 regularEffectMask = spellEffectInfo.regularEffectsMask;
            while (regularEffectMask)
            {
                u8 effectIndex = std::countr_zero(regularEffectMask);

                const GameDefine::Database::SpellEffect& spellEffect = spellEffectInfo.effects[effectIndex];
                zenith->CallEvent(MetaGen::Server::Lua::SpellEvent::OnHandleEffect, MetaGen::Server::Lua::SpellEventDataOnHandleEffect{
                    .casterID = entt::to_integral(casterEntity),
                    .spellEntity = entt::to_integral(entity),
                    .spellID = spellInfo.spellID,
                    .procID = 0,
                    .effectIndex = effectIndex,
                    .effectType = spellEffect.effectType
                });

                if (spellProcInfo)
                {
                    Util::Spell::CheckSpellEffectProc(world, zenith, timeState, gameCache, spellEffectInfo, *spellProcInfo, spellInfo.spellID, entity, casterEntity, effectIndex);
                }

                regularEffectMask &= ~(1ull << effectIndex);
            }

            world.Remove<Tags::IsActiveSpell>(entity);
            world.Emplace<Tags::IsCompleteSpell>(entity);
        });

        auto completedView = world.View<Components::SpellInfo, Components::SpellEffectInfo, Tags::IsCompleteSpell>();
        completedView.each([&](entt::entity entity, Components::SpellInfo& spellInfo, Components::SpellEffectInfo& spellEffectInfo)
        {
            entt::entity casterEntity = world.GetEntity(spellInfo.caster);
            if (!world.ValidEntity(casterEntity))
            {
                world.DestroyEntity(entity);
                return;
            }

            zenith->CallEvent(MetaGen::Server::Lua::SpellEvent::OnFinish, MetaGen::Server::Lua::SpellEventDataOnFinish{
                .casterID = entt::to_integral(casterEntity),
                .spellEntity = entt::to_integral(entity),
                .spellID = spellInfo.spellID
            });

            if (auto* spellProcInfo = world.TryGet<Components::SpellProcInfo>(entity))
            {
                Util::Spell::CheckSpellProc(world, zenith, timeState, gameCache, spellEffectInfo, *spellProcInfo, MetaGen::Shared::Spell::SpellProcPhaseTypeEnum::OnSpellFinish, spellInfo.spellID, entity, casterEntity);
            }

            if (auto* characterSpellCastInfo = world.TryGet<Components::CharacterSpellCastInfo>(casterEntity))
            {
                characterSpellCastInfo->activeSpellEntity = entt::null;

                if (characterSpellCastInfo->queuedSpellEntity != entt::null)
                {
                    characterSpellCastInfo->activeSpellEntity = characterSpellCastInfo->queuedSpellEntity;
                    characterSpellCastInfo->queuedSpellEntity = entt::null;

                    world.Emplace<Tags::IsUnpreparedSpell>(characterSpellCastInfo->activeSpellEntity);
                }
            }

            world.DestroyEntity(entity);
        });
    }
    void UpdateWorld::HandleCombatEventUpdate(World& world, Scripting::Zenith* zenith, Singletons::TimeState& timeState, Singletons::GameCache& gameCache, Singletons::NetworkState& networkState)
    {
        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        u32 numCombatEvents = static_cast<u32>(combatEventState.combatEvents.size());
        if (numCombatEvents == 0)
            return;

        while (numCombatEvents-- > 0)
        {
            PacketWriter combatEventWriter = world.packetArena.Acquire(64);
            if (!combatEventWriter.IsValid())
            {
                NC_LOG_ERROR("Failed to allocate combat event packet");
                return;
            }

            Bytebuffer& combatEventBuffer = combatEventWriter.GetBuffer();

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
            UnitPower& healthPower = Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);
            bool isDead = (healthPower.current <= 0.0);

            if (validSourceEntity && sourceEntity != targetEntity && (eventInfo.eventType == CombatEventType::Damage || eventInfo.eventType == CombatEventType::Heal))
            {
                if (const auto* creatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(sourceEntity))
                    Util::Combat::RefreshCreatureCombatParticipants(world, sourceEntity, creatureCombatInfo->leashRange);
            }

            bool builtMessage = false;
            switch (eventInfo.eventType)
            {
                case CombatEventType::Damage:
                {
                    const auto* targetCreatureCombatInfo = world.TryGet<Components::CreatureCombatInfo>(targetEntity);
                    if (isDead || (targetCreatureCombatInfo && targetCreatureCombatInfo->isEvading))
                        break;

                    f64 amount = static_cast<f32>(eventInfo.amount);
                    f64 damageDone = glm::min(amount, healthPower.current);
                    f64 overKillDamage = glm::max(0.0, amount - healthPower.current);

                    if (damageDone <= 0)
                        break;

                    const auto* targetFactionPolicy = world.TryGet<Components::CreatureFactionPolicy>(targetEntity);
                    const bool allowsDefensiveRetaliation = !targetFactionPolicy || targetFactionPolicy->aggression != Gameplay::Faction::CreatureAggressionPolicy::Passive;
                    bool canAddToThreatTable = validSourceEntity && allowsDefensiveRetaliation && world.AllOf<Components::CreatureThreatTable>(targetEntity);
                    if (canAddToThreatTable && sourceEntity != targetEntity)
                    {
                        auto& unitCombatInfo = world.Get<Components::UnitCombatInfo>(sourceEntity);
                        f64 threatAmount = damageDone * unitCombatInfo.threatModifier * 1000.0f;

                        Util::Combat::AddUnitToThreatTable(world, unitCombatInfo, targetEntity, sourceEntity, threatAmount);
                    }

                    if (validSourceEntity && sourceEntity != targetEntity && gameCache.factionRuntimeData)
                        RecruitFactionAssistance(world, *gameCache.factionRuntimeData, targetEntity, sourceEntity);

                    bool killedTarget = overKillDamage > 0 || damageDone == healthPower.current;
                    f64 newHealth = healthPower.current - damageDone;
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, damageDone, overKillDamage);

                    if (killedTarget)
                    {
                        Util::Movement::Stop(world, targetEntity);
                        world.Emplace<Events::UnitDied>(targetEntity, Events::UnitDied{ .killerEntity = validSourceEntity ? sourceEntity : entt::null });
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
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildHealingDoneMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, healingDone, overHealing);

                    break;
                }

                case CombatEventType::Resurrect:
                {
                    f64 missingHealth = healthPower.max - healthPower.current;
                    if (missingHealth <= 0)
                        break;

                    f64 newHealth = healthPower.max;
                    Util::Unit::SetPower(world, targetEntity, unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health, healthPower.base, newHealth, healthPower.max);

                    builtMessage = Util::MessageBuilder::CombatLog::BuildResurrectedMessage(combatEventBuffer, eventInfo.sourceGUID, eventInfo.targetGUID, missingHealth);

                    world.Emplace<Events::UnitResurrected>(targetEntity, Events::UnitResurrected{ .resurrectorEntity = validSourceEntity ? sourceEntity : entt::null });
                    break;
                }

                default:
                    break;
            }

            if (builtMessage)
            {
                entt::entity broadcasterEntity = validSourceEntity ? sourceEntity : targetEntity;

                auto& visibilityInfo = world.Get<Components::VisibilityInfo>(broadcasterEntity);

                // TODO : Handle cases where source and target have gone out of visibility range of each other
                Util::Network::SendToNearby(networkState, world, broadcasterEntity, visibilityInfo, true, combatEventWriter.Seal());
            }

            combatEventState.combatEvents.pop_front();
        }
    }

    void UpdateWorld::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& timeState = ctx.get<Singletons::TimeState>();
        auto& worldState = ctx.get<Singletons::WorldState>();

        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();

        // Handle World Transfer Requests
        {
            auto& worldTransferRequests = worldState.GetWorldTransferRequests();

            WorldTransferRequest request;
            while (worldTransferRequests.try_dequeue(request))
            {
                if (!Util::Network::IsSocketActive(networkState, request.socketID))
                    continue;

                // TODO : Upgrade targetMapID to ZenithKey
                u64 laneID = request.targetMapID;
                if (!worldState.HasWorld(request.targetMapID))
                {
                    worldState.AddWorld(request.targetMapID);
                    networkState.server->AddLane(laneID);
                }

                World& world = worldState.GetWorld(request.targetMapID);
                entt::entity characterEntity = world.CreateEntity();

                auto& netInfo = world.Emplace<Components::NetInfo>(characterEntity);
                netInfo.socketID = request.socketID;

                auto& objectInfo = world.Emplace<Components::ObjectInfo>(characterEntity);
                objectInfo.guid = ObjectGUID(ObjectGUID::Type::Player, request.characterID);

                auto& permissionInfo = world.Emplace<Components::CharacterPermissionInfo>(characterEntity);
                permissionInfo.accountPermissions = std::move(request.accountPermissions);

                worldState.SetMapIDForSocket(request.socketID, request.targetMapID);
                Util::Network::LinkSocketToCharacter(networkState, request.socketID, request.characterID, characterEntity);
                networkState.server->SetSocketIDLane(request.socketID, laneID);

                Util::Cache::CharacterCreate(gameCache, request.characterID, request.characterName, characterEntity);

                Util::Network::SendPacket(networkState, request.socketID, MetaGen::Shared::Packet::ServerWorldTransferPacket{});

                Util::Network::SendPacket(networkState, request.socketID, MetaGen::Shared::Packet::ServerLoadMapPacket{ .mapID = request.targetMapID });

                if (request.useTargetPosition)
                {
                    world.Emplace<Tags::CharacterWasWorldTransferred>(characterEntity);
                    Util::Persistence::Character::CharacterSetMapIDPositionOrientation(gameCache, request.characterID, request.targetMapID, request.targetPosition, request.targetOrientation);
                }

                world.Emplace<Events::CharacterNeedsInitialization>(characterEntity);
            }
        }

        for (auto& itr : worldState)
        {
            u32 mapID = itr.first;
            World& world = itr.second;

            world.packetArenaTrimTimer -= deltaTime;
            if (world.packetArenaTrimTimer <= 0.0f)
            {
                world.packetArena.Trim(networkState.packetArenaConfig.warmBlocksPerSizeClass);
                world.packetArenaTrimTimer = networkState.packetArenaConfig.trimIntervalSeconds;
            }

            if (HandleMapInitialization(world, timeState, gameCache))
                continue;

            Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(world.zenithKey);
            if (!zenith)
                continue;

            HandleNetworkMessages(world, zenith, timeState, gameCache, networkState);

            UpdateCharacter::HandleUpdate(worldState, world, zenith, timeState, gameCache, networkState, deltaTime);
            HandleCreatureUpdate(world, zenith, timeState, gameCache, networkState);
            HandleUnitUpdate(world, zenith, timeState, gameCache, networkState);

            HandleProximityTriggerUpdate(world, zenith, timeState, gameCache, networkState);

            HandleReplication(world, zenith, timeState, gameCache, networkState);
            HandleContainerUpdate(world, zenith, timeState, gameCache, networkState);
            HandleDisplayUpdate(world, zenith, timeState, networkState);
            HandleCombatUpdate(world, zenith, timeState);
            Movement::HandleUpdate(world, timeState, networkState);
            HandlePowerUpdate(world, zenith, timeState, networkState);
            HandleAuraUpdate(world, zenith, timeState, gameCache, networkState);
            HandleSpellUpdate(world, zenith, timeState, gameCache, networkState);
            HandleCombatEventUpdate(world, zenith, timeState, gameCache, networkState);
            HandleCombatTimeoutUpdate(world, zenith, timeState);
        }
    }
}
