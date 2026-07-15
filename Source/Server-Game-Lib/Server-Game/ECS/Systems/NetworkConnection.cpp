#include "NetworkConnection.h"

#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/AABB.h"
#include "Server-Game/ECS/Components/AdministrativeOperation.h"
#include "Server-Game/ECS/Components/AccountInfo.h"
#include "Server-Game/ECS/Components/AuthenticationInfo.h"
#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/CharacterReputation.h"
#include "Server-Game/ECS/Components/CharacterListInfo.h"
#include "Server-Game/ECS/Components/CharacterSpellCastInfo.h"
#include "Server-Game/ECS/Components/CreatureAIInfo.h"
#include "Server-Game/ECS/Components/CreatureCombatInfo.h"
#include "Server-Game/ECS/Components/CreatureFactionPolicy.h"
#include "Server-Game/ECS/Components/CreatureInfo.h"
#include "Server-Game/ECS/Components/CreatureLifecycleInfo.h"
#include "Server-Game/ECS/Components/CreatureThreatTable.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Components/FactionModifiers.h"
#include "Server-Game/ECS/Components/NetInfo.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/PermissionInfo.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Components/ProximityTrigger.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Components/SpellInfo.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Components/TargetInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitSpellCooldownHistory.h"
#include "Server-Game/ECS/Components/UnitFaction.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
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
#include "Server-Game/ECS/Util/CollisionUtil.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/ContainerUtil.h"
#include "Server-Game/ECS/Util/FactionUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/MovementUtil.h"
#include "Server-Game/ECS/Util/PermissionUtil.h"
#include "Server-Game/ECS/Util/ProximityTriggerUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/ECS/Util/Network/NetworkUtil.h"
#include "Server-Game/ECS/Util/Persistence/CharacterUtil.h"
#include "Server-Game/Scripting/Util/NetworkUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>
#include <Base/CVarSystem/CVarSystem.h>

#include <FileFormat/Shared.h>

#include <Gameplay/GameDefine.h>
#include <Gameplay/ECS/Components/UnitFields.h>
#include <Gameplay/Network/GameMessageRouter.h>

#include <MetaGen/PacketList.h>
#include <MetaGen/Shared/ClientDB/ClientDB.h>
#include <MetaGen/Shared/Packet/Packet.h>
#include <MetaGen/Shared/Spell/Spell.h>
#include <MetaGen/Shared/Unit/Unit.h>

#include <Network/Server.h>

#include <Scripting/LuaManager.h>

#include <entt/entt.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <iterator>
#include <limits>

namespace ECS::Systems
{
    namespace
    {
        void ReportAdministrativeDatabaseFailure(entt::registry* registry, World& world, Network::SocketID socketID, std::string_view operation, Database::OperationFailure failure)
        {
            auto& networkState = registry->ctx().get<Singletons::NetworkState>();
            std::string response = "Administrative operation '" + std::string(operation) + "' failed (" + std::string(Database::OperationFailureName(failure)) + ")";

            if (failure == Database::OperationFailure::Indeterminate)
                response += "; the database outcome is unknown and requires manual reconciliation";

            Util::Unit::SendChatMessage(world, networkState, socketID, response);
        }

        bool SpellRequiresAttackPermission(Singletons::GameCache& gameCache, u32 spellID)
        {
            Database::SpellEffectInfo* effects = nullptr;
            if (!Util::Cache::GetSpellEffectsBySpellID(gameCache, spellID, effects))
                return false;

            using EffectType = MetaGen::Shared::Spell::SpellEffectTypeEnum;
            return std::any_of(effects->effects.begin(), effects->effects.end(), [](const auto& effect)
            {
                const EffectType type = static_cast<EffectType>(effect.effectType);
                return type == EffectType::WeaponDamage || type == EffectType::AuraPeriodicDamage;
            });
        }

        bool GetCheatCommandPermission(MetaGen::Shared::Cheat::CheatCommandEnum command, MetaGen::Server::Permission::Permission& permission)
        {
            using CheatCommand = MetaGen::Shared::Cheat::CheatCommandEnum;
            using Permission = MetaGen::Server::Permission::Permission;
            switch (command)
            {
                case CheatCommand::Damage:
                    permission = Permission::CommandDamage;
                    return true;
                case CheatCommand::Heal:
                    permission = Permission::CommandHeal;
                    return true;
                case CheatCommand::Kill:
                    permission = Permission::CommandKill;
                    return true;
                case CheatCommand::Resurrect:
                    permission = Permission::CommandResurrect;
                    return true;
                case CheatCommand::UnitMorph:
                    permission = Permission::CommandUnitMorph;
                    return true;
                case CheatCommand::UnitDemorph:
                    permission = Permission::CommandUnitDemorph;
                    return true;
                case CheatCommand::Teleport:
                    permission = Permission::CommandTeleport;
                    return true;
                case CheatCommand::CharacterAdd:
                    permission = Permission::CommandCharacterAdd;
                    return true;
                case CheatCommand::CharacterRemove:
                    permission = Permission::CommandCharacterRemove;
                    return true;
                case CheatCommand::UnitSetRace:
                    permission = Permission::CommandUnitSetRace;
                    return true;
                case CheatCommand::UnitSetGender:
                    permission = Permission::CommandUnitSetGender;
                    return true;
                case CheatCommand::UnitSetClass:
                    permission = Permission::CommandUnitSetClass;
                    return true;
                case CheatCommand::UnitSetLevel:
                    permission = Permission::CommandUnitSetLevel;
                    return true;
                case CheatCommand::ItemSetTemplate:
                    permission = Permission::CommandItemSetTemplate;
                    return true;
                case CheatCommand::ItemSetStatTemplate:
                    permission = Permission::CommandItemSetStatTemplate;
                    return true;
                case CheatCommand::ItemSetArmorTemplate:
                    permission = Permission::CommandItemSetArmorTemplate;
                    return true;
                case CheatCommand::ItemSetWeaponTemplate:
                    permission = Permission::CommandItemSetWeaponTemplate;
                    return true;
                case CheatCommand::ItemSetShieldTemplate:
                    permission = Permission::CommandItemSetShieldTemplate;
                    return true;
                case CheatCommand::ItemAdd:
                    permission = Permission::CommandItemAdd;
                    return true;
                case CheatCommand::ItemRemove:
                    permission = Permission::CommandItemRemove;
                    return true;
                case CheatCommand::CreatureAdd:
                    permission = Permission::CommandCreatureAdd;
                    return true;
                case CheatCommand::CreatureRemove:
                    permission = Permission::CommandCreatureRemove;
                    return true;
                case CheatCommand::CreatureInfo:
                    permission = Permission::CommandCreatureInfo;
                    return true;
                case CheatCommand::MapAdd:
                    permission = Permission::CommandMapAdd;
                    return true;
                case CheatCommand::GotoAdd:
                    permission = Permission::CommandGotoAdd;
                    return true;
                case CheatCommand::GotoAddHere:
                    permission = Permission::CommandGotoAddHere;
                    return true;
                case CheatCommand::GotoRemove:
                    permission = Permission::CommandGotoRemove;
                    return true;
                case CheatCommand::GotoMap:
                    permission = Permission::CommandGotoMap;
                    return true;
                case CheatCommand::GotoLocation:
                    permission = Permission::CommandGotoLocation;
                    return true;
                case CheatCommand::GotoXYZ:
                    permission = Permission::CommandGotoXYZ;
                    return true;
                case CheatCommand::TriggerAdd:
                    permission = Permission::CommandTriggerAdd;
                    return true;
                case CheatCommand::TriggerRemove:
                    permission = Permission::CommandTriggerRemove;
                    return true;
                case CheatCommand::SpellSet:
                    permission = Permission::CommandSpellSet;
                    return true;
                case CheatCommand::SpellEffectSet:
                    permission = Permission::CommandSpellEffectSet;
                    return true;
                case CheatCommand::SpellProcDataSet:
                    permission = Permission::CommandSpellProcDataSet;
                    return true;
                case CheatCommand::SpellProcLinkSet:
                    permission = Permission::CommandSpellProcLinkSet;
                    return true;
                case CheatCommand::CreatureAddScript:
                    permission = Permission::CommandCreatureAddScript;
                    return true;
                case CheatCommand::CreatureRemoveScript:
                    permission = Permission::CommandCreatureRemoveScript;
                    return true;
                case CheatCommand::CreatureMove:
                    permission = Permission::CommandCreatureMove;
                    return true;
                case CheatCommand::CreatureFollow:
                    permission = Permission::CommandCreatureFollow;
                    return true;
                case CheatCommand::CreatureWander:
                    permission = Permission::CommandCreatureWander;
                    return true;
                case CheatCommand::CreatureStop:
                    permission = Permission::CommandCreatureStop;
                    return true;
                case CheatCommand::FactionReaction:
                    permission = Permission::CommandFactionReaction;
                    return true;
                case CheatCommand::FactionReputationInfo:
                    permission = Permission::CommandFactionReputationView;
                    return true;
                case CheatCommand::FactionReputationSet:
                case CheatCommand::FactionReputationModify:
                case CheatCommand::FactionReputationRemove:
                case CheatCommand::FactionReputationSetFlags:
                case CheatCommand::FactionReputationLock:
                    permission = Permission::CommandFactionReputationModify;
                    return true;
                case CheatCommand::UnitSetFaction:
                    permission = Permission::CommandUnitSetFaction;
                    return true;

                default:
                    return false;
            }
        }

        std::string_view ReactionName(Gameplay::Faction::Reaction reaction)
        {
            using Gameplay::Faction::Reaction;
            switch (reaction)
            {
                case Reaction::Hostile:
                    return "Hostile";
                case Reaction::Unfriendly:
                    return "Unfriendly";
                case Reaction::Neutral:
                    return "Neutral";
                case Reaction::Friendly:
                    return "Friendly";
                default:
                    return "Invalid";
            }
        }

        bool TryGetAdministrativeReputationTarget(World& world, Singletons::NetworkState& networkState, Network::SocketID socketID, ObjectGUID characterGUID, entt::entity& characterEntity, Components::CharacterReputation*& reputation, Components::UnitFaction*& characterFaction)
        {
            characterEntity = world.GetEntity(characterGUID);
            reputation = world.TryGet<Components::CharacterReputation>(characterEntity);
            characterFaction = world.TryGet<Components::UnitFaction>(characterEntity);
            if (characterEntity != entt::null && characterGUID.GetType() == ObjectGUID::Type::Player && reputation && characterFaction)
                return true;

            Util::Unit::SendChatMessage(world, networkState, socketID, "Reputation administration requires a live player GUID in the current world.");
            return false;
        }

        bool TryGetAdministrativeReputationFaction(World& world, Singletons::NetworkState& networkState, Network::SocketID socketID, const Gameplay::Faction::FactionRuntimeData& runtime, Gameplay::Faction::FactionID factionID, Gameplay::Faction::FactionIndex& factionIndex)
        {
            if (factionID != Gameplay::Faction::NONE_FACTION_ID && runtime.TryGetFactionIndex(factionID, factionIndex) && Gameplay::Faction::HasFlag(runtime.GetDefinition(factionIndex).flags, Gameplay::Faction::DefinitionFlags::AllowsReputation))
                return true;

            Util::Unit::SendChatMessage(world, networkState, socketID, std::format("Faction {} is unknown or does not allow reputation.", factionID));
            return false;
        }

        void SendAdministrativeReputationEntry(World& world, Singletons::NetworkState& networkState, Network::SocketID socketID, entt::entity characterEntity, const Gameplay::Faction::FactionRuntimeData& runtime, const Components::UnitFaction& characterFaction, const Components::CharacterReputation& reputation, Gameplay::Faction::FactionIndex factionIndex)
        {
            const auto& definition = runtime.GetDefinition(factionIndex);
            const auto* state = Util::Faction::FindReputation(reputation, factionIndex);
            const bool dirty = reputation.dirtyByFaction.contains(factionIndex);
            const bool removedDirty = reputation.removedDirtyByFaction.contains(factionIndex);
            const bool pendingNetwork = reputation.pendingNetworkByFaction.contains(factionIndex);
            if (!state)
            {
                const i32 fallback = runtime.GetStartingReputation(characterFaction.assignedFaction, factionIndex);
                const auto& standing = runtime.GetStanding(fallback);
                const auto& effectiveStanding = Util::Faction::GetEffectiveStanding(world, characterEntity, runtime, definition.id);
                Util::Unit::SendChatMessage(world, networkState, socketID,
                    std::format("- {} ({}): absent, fallback {} ({}), effective standing {}, pending delete {}, pending network {}",
                        definition.id, definition.name, fallback, standing.name, effectiveStanding.name,
                        removedDirty, pendingNetwork));
                return;
            }

            const auto& standing = runtime.GetStanding(state->value);
            const auto& effectiveStanding = Util::Faction::GetEffectiveStanding(world, characterEntity, runtime, definition.id);
            const u16 flags = state->flags;
            using Gameplay::Faction::ReputationFlags;
            Util::Unit::SendChatMessage(world, networkState, socketID,
                std::format("- {} ({}): value {} ({}), effective standing {}, flags {} "
                            "[Visible {}, Tracked {}, AtWar {}, Inactive {}, Locked {}], dirty {}, pending network {}",
                    definition.id, definition.name, state->value, standing.name, effectiveStanding.name, flags,
                    (flags & static_cast<u16>(ReputationFlags::Visible)) != 0,
                    (flags & static_cast<u16>(ReputationFlags::Tracked)) != 0,
                    (flags & static_cast<u16>(ReputationFlags::AtWar)) != 0,
                    (flags & static_cast<u16>(ReputationFlags::Inactive)) != 0,
                    (flags & static_cast<u16>(ReputationFlags::Locked)) != 0,
                    dirty, pendingNetwork));
        }

        AutoCVar_Int CVAR_NetworkMaxConnections(CVarCategory::Server, "network.maxConnections", "Maximum concurrent server connections. Read during server startup.", 20000);
        AutoCVar_Int CVAR_NetworkPacketArenaGlobalBudgetMiB(CVarCategory::Server, "network.packetArenaGlobalBudgetMiB", "Shared packet-arena reservation budget in MiB across network, world, and inbound arenas. Read during server startup.", 256);
        AutoCVar_Int CVAR_NetworkPacketArenaPerArenaBudgetMiB(CVarCategory::Server, "network.packetArenaPerArenaBudgetMiB", "Maximum packet-arena reservation budget per producer arena in MiB. Read during server startup.", 128);
        AutoCVar_Int CVAR_NetworkInboundPacketArenaBudgetMiB(CVarCategory::Server, "network.inboundPacketArenaBudgetMiB", "Maximum reservation budget in MiB for the ASIO-owned inbound packet arena. Read during server startup.", 64);
        AutoCVar_Int CVAR_NetworkPacketArenaBlockMiB(CVarCategory::Server, "network.packetArenaBlockMiB", "Packet-arena block size in MiB. Read during server startup.", 2);
        AutoCVar_Int CVAR_NetworkPacketArenaWarmBlocks(CVarCategory::Server, "network.packetArenaWarmBlocksPerSizeClass", "Fully free packet-arena blocks retained per used size class.", 1);
        AutoCVar_Int CVAR_NetworkPacketArenaTrimIntervalSeconds(CVarCategory::Server, "network.packetArenaTrimIntervalSeconds", "Simulation-time interval between packet-arena trim passes.", 1);
        AutoCVar_Int CVAR_NetworkOutboundQueueMiB(CVarCategory::Server, "network.outboundQueueMiB", "Maximum global end-to-end outbound queue size in MiB. Read during server startup.", 64);
        AutoCVar_Int CVAR_NetworkOutboundQueueEvents(CVarCategory::Server, "network.outboundQueueEvents", "Maximum global end-to-end outbound queue events. Read during server startup.", 65536);
        AutoCVar_Int CVAR_NetworkCriticalQueueReserveMiB(CVarCategory::Server, "network.criticalQueueReserveMiB", "Global outbound queue MiB reserved from replication traffic for critical packets. Read during server startup.", 8);
        AutoCVar_Int CVAR_NetworkCriticalQueueReserveEvents(CVarCategory::Server, "network.criticalQueueReserveEvents", "Global outbound queue events reserved from replication traffic for critical packets. Zero derives one event per configured connection. Read during server startup.", 0);
        AutoCVar_Int CVAR_NetworkSessionQueueKiB(CVarCategory::Server, "network.sessionQueueKiB", "Maximum outbound queue size per session in KiB. Read during server startup.", 256);
        AutoCVar_Int CVAR_NetworkSessionQueueEvents(CVarCategory::Server, "network.sessionQueueEvents", "Maximum outbound queue events per session. Read during server startup.", 256);
        AutoCVar_Int CVAR_NetworkCriticalSessionQueueReserveKiB(CVarCategory::Server, "network.criticalSessionQueueReserveKiB", "Per-session outbound queue KiB reserved from replication traffic for critical packets. Read during server startup.", 32);
        AutoCVar_Int CVAR_NetworkCriticalSessionQueueReserveEvents(CVarCategory::Server, "network.criticalSessionQueueReserveEvents", "Per-session outbound queue events reserved from replication traffic for critical packets. Read during server startup.", 16);
        AutoCVar_Int CVAR_NetworkInboundQueueMiB(CVarCategory::Server, "network.inboundQueueMiB", "Maximum global queued inbound packet size in MiB. Read during server startup.", 64);
        AutoCVar_Int CVAR_NetworkInboundQueueEvents(CVarCategory::Server, "network.inboundQueueEvents", "Maximum global queued inbound packet events. Read during server startup.", 65536);
        AutoCVar_Int CVAR_NetworkSessionInboundQueueKiB(CVarCategory::Server, "network.sessionInboundQueueKiB", "Maximum queued inbound packet size per session in KiB. Read during server startup.", 64);
        AutoCVar_Int CVAR_NetworkSessionInboundQueueEvents(CVarCategory::Server, "network.sessionInboundQueueEvents", "Maximum queued inbound packet events per session. Read during server startup.", 64);
        AutoCVar_Int CVAR_NetworkSessionReadTimeoutSeconds(CVarCategory::Server, "network.sessionReadTimeoutSeconds", "Maximum time allowed for each packet header or body read. Zero disables the timeout. Read during server startup.", 60);
        AutoCVar_Int CVAR_NetworkSessionCloseDrainMilliseconds(CVarCategory::Server, "network.sessionCloseDrainMilliseconds", "Maximum graceful outbound drain time before a closing socket is forced closed. Read during server startup.", 100);
        AutoCVar_Int CVAR_NetworkMinimumPingIntervalMilliseconds(CVarCategory::Server, "network.minimumPingIntervalMilliseconds", "Minimum interval between accepted client pings. Zero disables Engine-side ping admission control. Read during server startup.", 1000);
        AutoCVar_Int CVAR_NetworkDeferredRequestsPerDispatch(CVarCategory::Server, "network.deferredRequestsPerDispatch", "Maximum requests drained from each ASIO handoff queue per dispatch. Read during server startup.", 4096);
        AutoCVar_Int CVAR_NetworkInboundMessagesPerLanePerUpdate(CVarCategory::Server, "network.inboundMessagesPerLanePerUpdate", "Maximum inbound packets handled from each lane per Server-Game update. Read during server startup.", 4096);
        AutoCVar_Int CVAR_NetworkReplicationCoalescing(CVarCategory::Server, "network.replicationCoalescing", "Coalesce queued latest-state replication packets for a session. Read during server startup.", 1, CVarFlags::EditCheckbox);
        AutoCVar_Int CVAR_NetworkTelemetryEnabled(CVarCategory::Server, "network.telemetryEnabled", "Periodically log packet-arena and network queue telemetry.", 0, CVarFlags::EditCheckbox);
        AutoCVar_Int CVAR_NetworkTelemetryIntervalSeconds(CVarCategory::Server, "network.telemetryIntervalSeconds", "Simulation-time interval between network telemetry reports.", 5);
        AutoCVar_Int CVAR_NetworkConnectionLogDetail(CVarCategory::Server, "network.connectionLogDetail", "Log every socket connect and disconnect instead of one aggregate line per update.", 0, CVarFlags::EditCheckbox);

        size_t GetConfiguredBytes(AutoCVar_Int& cvar, size_t unitBytes, i32 minimumValue = 1)
        {
            return static_cast<size_t>(std::max(cvar.Get(), minimumValue)) * unitBytes;
        }

        Singletons::PacketArenaConfig CreatePacketArenaConfig()
        {
            const size_t globalMaxReservedBytes = GetConfiguredBytes(CVAR_NetworkPacketArenaGlobalBudgetMiB, 1024ull * 1024ull);
            const size_t maxReservedBytes = GetConfiguredBytes(CVAR_NetworkPacketArenaPerArenaBudgetMiB, 1024ull * 1024ull);
            return {
                .globalMaxReservedBytes = globalMaxReservedBytes,
                .maxReservedBytes = maxReservedBytes,
                .blockSize = std::min(GetConfiguredBytes(CVAR_NetworkPacketArenaBlockMiB, 1024ull * 1024ull), std::min(globalMaxReservedBytes, maxReservedBytes)),
                .warmBlocksPerSizeClass = static_cast<size_t>(std::max(CVAR_NetworkPacketArenaWarmBlocks.Get(), 0)),
                .trimIntervalSeconds = static_cast<f32>(std::max(CVAR_NetworkPacketArenaTrimIntervalSeconds.Get(), 1))
            };
        }

        Network::ServerConfig CreateServerConfig()
        {
            const u32 maxConnections = static_cast<u32>(std::max(CVAR_NetworkMaxConnections.Get(), 1));
            const size_t maxOutboundQueueEvents = static_cast<size_t>(std::max(CVAR_NetworkOutboundQueueEvents.Get(), 1));
            const i32 configuredCriticalReserveEvents = CVAR_NetworkCriticalQueueReserveEvents.Get();
            const size_t globalMaxReservedBytes = GetConfiguredBytes(CVAR_NetworkPacketArenaGlobalBudgetMiB, 1024ull * 1024ull);
            const size_t inboundMaxReservedBytes = GetConfiguredBytes(CVAR_NetworkInboundPacketArenaBudgetMiB, 1024ull * 1024ull);
            return {
                .maxConnections = maxConnections,
                .maxOutboundQueueBytes = GetConfiguredBytes(CVAR_NetworkOutboundQueueMiB, 1024ull * 1024ull),
                .maxOutboundQueueEvents = maxOutboundQueueEvents,
                .criticalOutboundQueueReserveBytes = GetConfiguredBytes(CVAR_NetworkCriticalQueueReserveMiB, 1024ull * 1024ull, 0),
                .criticalOutboundQueueReserveEvents = configuredCriticalReserveEvents > 0 ? std::min(static_cast<size_t>(configuredCriticalReserveEvents), maxOutboundQueueEvents) : std::min(static_cast<size_t>(maxConnections), maxOutboundQueueEvents),
                .maxSessionQueueBytes = GetConfiguredBytes(CVAR_NetworkSessionQueueKiB, 1024ull),
                .maxSessionQueueEvents = static_cast<size_t>(std::max(CVAR_NetworkSessionQueueEvents.Get(), 1)),
                .criticalSessionQueueReserveBytes = GetConfiguredBytes(CVAR_NetworkCriticalSessionQueueReserveKiB, 1024ull, 0),
                .criticalSessionQueueReserveEvents = static_cast<size_t>(std::max(CVAR_NetworkCriticalSessionQueueReserveEvents.Get(), 0)),
                .maxInboundQueueBytes = GetConfiguredBytes(CVAR_NetworkInboundQueueMiB, 1024ull * 1024ull),
                .maxInboundQueueEvents = static_cast<size_t>(std::max(CVAR_NetworkInboundQueueEvents.Get(), 1)),
                .maxSessionInboundQueueBytes = GetConfiguredBytes(CVAR_NetworkSessionInboundQueueKiB, 1024ull),
                .maxSessionInboundQueueEvents = static_cast<size_t>(std::max(CVAR_NetworkSessionInboundQueueEvents.Get(), 1)),
                .inboundPacketArenaMaxReservedBytes = inboundMaxReservedBytes,
                .inboundPacketArenaBlockSize = std::min(GetConfiguredBytes(CVAR_NetworkPacketArenaBlockMiB, 1024ull * 1024ull), std::min(globalMaxReservedBytes, inboundMaxReservedBytes)),
                .inboundPacketArenaWarmBlocksPerSizeClass = static_cast<size_t>(std::max(CVAR_NetworkPacketArenaWarmBlocks.Get(), 0)),
                .sessionReadTimeoutMilliseconds = static_cast<u32>(std::max(CVAR_NetworkSessionReadTimeoutSeconds.Get(), 0)) * 1000u,
                .sessionCloseDrainMilliseconds = static_cast<u32>(std::max(CVAR_NetworkSessionCloseDrainMilliseconds.Get(), 0)),
                .maxDeferredRequestsPerDispatch = static_cast<u32>(std::max(CVAR_NetworkDeferredRequestsPerDispatch.Get(), 1)),
                .inboundPacketArenaTrimIntervalMilliseconds = static_cast<u32>(std::max(CVAR_NetworkPacketArenaTrimIntervalSeconds.Get(), 1)) * 1000u,
                .replicationCoalescingEnabled = CVAR_NetworkReplicationCoalescing.Get() != 0,
                .minimumPingIntervalMilliseconds = static_cast<u32>(std::max(CVAR_NetworkMinimumPingIntervalMilliseconds.Get(), 0))
            };
        }

        void LogNetworkTelemetry(const Singletons::NetworkState& networkState, Singletons::WorldState& worldState)
        {
            size_t worldArenaReservedBytes = 0;
            size_t worldArenaInUseBytes = 0;
            size_t worldArenaInUseAllocations = 0;
            size_t worldArenaAllocationFailures = 0;
            size_t worldCount = 0;
            for (const auto& worldEntry : worldState)
            {
                const World& world = worldEntry.second;
                worldArenaReservedBytes += world.packetArena.GetReservedBytes();
                worldArenaInUseBytes += world.packetArena.GetInUseBytes();
                worldArenaInUseAllocations += world.packetArena.GetInUseAllocationCount();
                worldArenaAllocationFailures += world.packetArena.GetAllocationFailureCount();
                worldCount++;
            }

            const Network::Server& server = *networkState.server;
            const Network::ServerTelemetryDetail telemetryDetail = server.GetTelemetryDetail();
            constexpr f64 BYTES_PER_MIB = 1024.0 * 1024.0;
            NC_LOG_INFO(
                "Network telemetry: sessions {}, shared arena {:.2f}/{:.2f} MiB; network {:.2f} MiB reserved, {:.2f} MiB in use ({} in-use allocations, {} failures); worlds {} ({:.2f} MiB reserved, {:.2f} MiB in use, {} in-use allocations, {} failures); inbound arena {:.2f} MiB reserved, {:.2f} MiB in use ({} in-use allocations, {} failures); inbound queue {:.2f} MiB/{} events, overflows {}; outbound {:.2f} MiB/{} events, overflows {}, session overflows {}, replication drops {}, coalesced {}",
                networkState.activeSocketIDs.size(),
                static_cast<f64>(networkState.packetArena.GetSharedReservedBytes()) / BYTES_PER_MIB,
                static_cast<f64>(networkState.packetArena.GetSharedMaxReservedBytes()) / BYTES_PER_MIB,
                static_cast<f64>(networkState.packetArena.GetReservedBytes()) / BYTES_PER_MIB,
                static_cast<f64>(networkState.packetArena.GetInUseBytes()) / BYTES_PER_MIB,
                networkState.packetArena.GetInUseAllocationCount(),
                networkState.packetArena.GetAllocationFailureCount(),
                worldCount,
                static_cast<f64>(worldArenaReservedBytes) / BYTES_PER_MIB,
                static_cast<f64>(worldArenaInUseBytes) / BYTES_PER_MIB,
                worldArenaInUseAllocations,
                worldArenaAllocationFailures,
                static_cast<f64>(server.GetInboundPacketArenaReservedBytes()) / BYTES_PER_MIB,
                static_cast<f64>(server.GetInboundPacketArenaInUseBytes()) / BYTES_PER_MIB,
                server.GetInboundPacketArenaInUseAllocationCount(),
                server.GetInboundPacketArenaAllocationFailureCount(),
                static_cast<f64>(server.GetInboundQueueBytes()) / BYTES_PER_MIB,
                server.GetInboundQueueEvents(),
                server.GetInboundQueueOverflowCount(),
                static_cast<f64>(server.GetOutboundQueueBytes()) / BYTES_PER_MIB,
                server.GetOutboundQueueEvents(),
                server.GetOutboundQueueOverflowCount(),
                server.GetSessionQueueOverflowCount(),
                server.GetReplicationPacketDropCount(),
                server.GetReplicationPacketCoalesceCount());

            NC_LOG_INFO(
                "Network telemetry detail: inbound high-water {:.2f} MiB/{} events; outbound high-water {:.2f} MiB/{} events; reads header/body {}/{}, writes {}; read timers arm/cancel/timeout {}/{}/{}, ping limited {}; deferred posts/dispatches {}/{}; largest staged session batch {}",
                static_cast<f64>(telemetryDetail.inboundQueueBytesHighWater) / BYTES_PER_MIB,
                telemetryDetail.inboundQueueEventsHighWater,
                static_cast<f64>(telemetryDetail.outboundQueueBytesHighWater) / BYTES_PER_MIB,
                telemetryDetail.outboundQueueEventsHighWater,
                telemetryDetail.readHeaderCount,
                telemetryDetail.readBodyCount,
                telemetryDetail.writeCount,
                telemetryDetail.readTimeoutArmCount,
                telemetryDetail.readTimeoutCancelCount,
                telemetryDetail.readTimeoutCount,
                telemetryDetail.pingRateLimitCount,
                telemetryDetail.deferredPostCount,
                telemetryDetail.deferredDispatchCount,
                telemetryDetail.pendingPacketBatchHighWater);
        }
    }

    bool HandleOnCheatCharacterAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        struct Result
        {
        public:
            u8 NameIsInvalid : 1 = 0;
            u8 CharacterAlreadyExists : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 Unused : 5 = 0;
        };

        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        Result result = { 0 };
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
        {
            result.NameIsInvalid = 1;
        }
        else if (Util::Cache::CharacterExistsByName(gameCache, charName))
        {
            result.CharacterAlreadyExists = 1;
        }

        u8 resultAsValue = *reinterpret_cast<u8*>(&result);
        if (resultAsValue != 0)
        {
            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCheatCommandResultPacket{ .command = static_cast<u8>(MetaGen::Shared::Cheat::CheatCommandEnum::CharacterAdd), .result = resultAsValue, .response = "Unknown" });
            return true;
        }

        const u64 accountID = world.Get<Components::CharacterInfo>(entity).accountID;
        u64 characterID;
        ECS::Result creationResult = Util::Persistence::Character::CharacterCreate(gameCache, accountID, charName, 1, characterID);

        Result cheatCommandResult = {
            .NameIsInvalid = 0,
            .CharacterAlreadyExists = creationResult == ECS::Result::CharacterAlreadyExists,
            .DatabaseTransactionFailed = creationResult == ECS::Result::DatabaseError || creationResult == ECS::Result::DatabaseNotConnected,
            .Unused = 0
        };

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&cheatCommandResult);

        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCheatCommandResultPacket{ .command = static_cast<u8>(MetaGen::Shared::Cheat::CheatCommandEnum::CharacterAdd), .result = cheatCommandResultVal, .response = "Unknown" });
        return true;
    }

    bool HandleOnCheatCharacterRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        struct Result
        {
        public:
            u8 CharacterDoesNotExist : 1 = 0;
            u8 DatabaseTransactionFailed : 1 = 0;
            u8 InsufficientPermission : 1 = 0;
            u8 Unused : 5 = 0;
        };

        std::string charName = "";
        if (!message.buffer->GetString(charName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        Result result = { 0 };
        if (!StringUtils::StringIsAlphaAndAtLeastLength(charName, 2))
        {
            result.CharacterDoesNotExist = 1;
            u8 resultAsValue = *reinterpret_cast<u8*>(&result);

            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCheatCommandResultPacket{ .command = static_cast<u8>(MetaGen::Shared::Cheat::CheatCommandEnum::CharacterRemove), .result = resultAsValue, .response = "Unknown" });
            return true;
        }

        u64 characterIDToDelete = std::numeric_limits<u64>::max();
        if (!Util::Cache::GetCharacterIDByName(gameCache, charName, characterIDToDelete))
        {
            auto character = gameCache.database->GetCharacterByName(charName);
            if (!character)
            {
                ReportAdministrativeDatabaseFailure(registry, world, socketID, "find_character", character.Failure());
                result.DatabaseTransactionFailed = 1;
            }
            else if (!character.Value())
            {
                result.CharacterDoesNotExist = 1;
            }
            else
            {
                characterIDToDelete = character.Value()->id;
            }
        }

        if (result.CharacterDoesNotExist || result.DatabaseTransactionFailed)
        {
            u8 resultAsValue = *reinterpret_cast<u8*>(&result);
            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCheatCommandResultPacket{ .command = static_cast<u8>(MetaGen::Shared::Cheat::CheatCommandEnum::CharacterRemove), .result = resultAsValue, .response = "Unknown" });
            return true;
        }

        auto deletion = gameCache.database->DeleteCharacter(characterIDToDelete);
        if (!deletion)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "delete_character", deletion.Failure());
            result.DatabaseTransactionFailed = 1;
        }
        else
        {
            result.CharacterDoesNotExist = deletion.Value() == Database::DeleteOutcome::AlreadyAbsent;

            // Disconnect only after the database commit is confirmed.
            Network::SocketID characterSocketID;
            if (Util::Network::GetSocketIDFromCharacterID(networkState, characterIDToDelete, characterSocketID))
                networkState.server->CloseSocketID(characterSocketID);
        }

        u8 cheatCommandResultVal = *reinterpret_cast<u8*>(&result);

        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCheatCommandResultPacket{ .command = static_cast<u8>(MetaGen::Shared::Cheat::CheatCommandEnum::CharacterRemove), .result = cheatCommandResultVal, .response = "Unknown" });

        return true;
    }

    bool HandleOnCheatCreatureAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 creatureTemplateID = 0;
        if (!message.buffer->GetU32(creatureTemplateID))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        Database::CreatureTemplate* creatureTemplate = nullptr;
        if (!Util::Cache::GetCreatureTemplateByID(gameCache, creatureTemplateID, creatureTemplate))
            return true;

        auto& playerTransform = world.Get<Components::Transform>(entity);

        entt::entity creatureEntity = world.CreateEntity();
        world.EmplaceOrReplace<Events::CreatureCreate>(creatureEntity, Events::CreatureCreate{
            .templateID = creatureTemplateID,
            .displayID = creatureTemplate->displayId,
            .mapID = playerTransform.mapID,
            .position = playerTransform.position,
            .scale = vec3(creatureTemplate->scale),
            .orientation = playerTransform.pitchYaw.y,
            .scriptName = creatureTemplate->scriptName,
            .wanderDistance = Util::Movement::DEFAULT_WANDER_RADIUS,
            .movementType = creatureTemplate->defaultMovementType
        });
        world.EmplaceOrReplace<Components::AdministrativeRequestContext>(creatureEntity, socketID);

        return true;
    }
    bool HandleOnCheatCreatureRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID creatureGUID;
        if (!message.buffer->Deserialize(creatureGUID))
            return false;

        entt::entity creatureEntity = world.GetEntity(creatureGUID);
        if (creatureEntity == entt::null)
            return true;

        world.EmplaceOrReplace<Events::CreatureNeedsDeinitialization>(creatureEntity, creatureGUID);
        world.EmplaceOrReplace<Components::AdministrativeRequestContext>(creatureEntity, socketID);
        return true;
    }
    bool HandleOnCheatCreatureInfo(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID creatureGUID;
        if (!message.buffer->Deserialize(creatureGUID))
            return false;

        entt::entity creatureEntity = world.GetEntity(creatureGUID);
        if (creatureEntity == entt::null)
            return true;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        if (!world.AllOf<Components::CreatureInfo, Components::CreatureCombatInfo, Components::CreatureLifecycleInfo>(creatureEntity))
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Failed to get creature info, entity is not a creature");
            return true;
        }

        auto& creatureInfo = world.Get<Components::CreatureInfo>(creatureEntity);
        auto& creatureCombatInfo = world.Get<Components::CreatureCombatInfo>(creatureEntity);
        auto& creatureLifecycleInfo = world.Get<Components::CreatureLifecycleInfo>(creatureEntity);

        Database::CreatureTemplate* creatureTemplate = nullptr;
        if (!Util::Cache::GetCreatureTemplateByID(gameCache, creatureInfo.templateID, creatureTemplate))
            return true;

        const auto* faction = world.TryGet<Components::UnitFaction>(creatureEntity);
        const auto* factionPolicy = world.TryGet<Components::CreatureFactionPolicy>(creatureEntity);
        const auto* factionModifiers = world.TryGet<Components::FactionModifiers>(creatureEntity);
        const auto* threatTable = world.TryGet<Components::CreatureThreatTable>(creatureEntity);
        const auto* targetInfo = world.TryGet<Components::TargetInfo>(creatureEntity);
        const ObjectGUID targetGUID = targetInfo && world.ValidEntity(targetInfo->target)
                                          ? world.Get<Components::ObjectInfo>(targetInfo->target).guid
                                          : ObjectGUID::Empty;

        std::string factionText = "unavailable";
        if (faction && gameCache.factionRuntimeData)
        {
            const auto& assigned = gameCache.factionRuntimeData->GetDefinition(faction->assignedFaction);
            const auto& effective = gameCache.factionRuntimeData->GetDefinition(faction->effectiveFaction);
            const auto bounds = Gameplay::Faction::ReactionBounds::Unpack(faction->effectivePlayerReactionBounds);
            factionText = std::format("Assigned : {} ({}), Effective : {} ({}), Bounds : {}..{}, Flags : {}, Modifiers : {}",
                assigned.id, assigned.name, effective.id, effective.name, ReactionName(bounds.minimum),
                ReactionName(bounds.maximum), faction->flags,
                factionModifiers ? factionModifiers->contributors.size() : 0);
        }

        const std::string policyText = factionPolicy
                                           ? std::format("Aggression : {}, Assistance : {}, Detection Range : {}, Assistance Range : {}",
                                                 Gameplay::Faction::CreatureAggressionPolicyName(factionPolicy->aggression),
                                                 Gameplay::Faction::CreatureAssistancePolicyName(factionPolicy->assistance),
                                                 factionPolicy->detectionRange, factionPolicy->assistanceRange)
                                           : "unavailable";

        std::string response = std::format("Creature Info:\n- GUID : {}\n- TemplateID : {}\n- Name : {}\n- Display ID : {}\n- Level : {}\n- Unit Class : {}\n- Movement : (Type : {}, Radius : {}, Walk Speed : {}, Run Speed : {})\n- Combat : (In Combat : {}, Target : {}, Threat Entries : {}, Evading : {})\n- Faction : ({})\n- Faction Policy : ({})\n- Leash : (Range : {})\n- Lifecycle : (State : {}, Timer : {:.1f}, Respawn : {}-{} sec)\n- Melee : (Damage : {}, Attack Time : {:.0f} ms, School : {})\n- Mods : (Health : {}, Armor : {}, Resource : {}, Damage : {})\n- AI Script Name : ",
            creatureGUID.ToString(),
            creatureInfo.templateID,
            creatureInfo.name,
            creatureTemplate->displayId,
            creatureInfo.level,
            static_cast<u16>(creatureInfo.unitClass),
            creatureInfo.movementType,
            creatureInfo.wanderDistance,
            creatureInfo.walkSpeed,
            creatureInfo.runSpeed,
            world.AllOf<Tags::IsInCombat>(creatureEntity),
            targetGUID.ToString(),
            threatTable ? threatTable->threatList.size() : 0,
            creatureCombatInfo.isEvading,
            factionText,
            policyText,
            creatureCombatInfo.leashRange,
            static_cast<u16>(creatureLifecycleInfo.state),
            creatureLifecycleInfo.timeRemaining,
            creatureLifecycleInfo.spawnTimeInSecMin,
            creatureLifecycleInfo.spawnTimeInSecMax,
            creatureCombatInfo.meleeDamage,
            creatureCombatInfo.meleeAttackTime * 1000.0f,
            creatureCombatInfo.damageSchool,
            creatureTemplate->healthMod,
            creatureTemplate->armorMod,
            creatureTemplate->resourceMod,
            creatureTemplate->damageMod);

        if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(creatureEntity))
        {
            response += std::format("(\"{}\")", creatureAIInfo->scriptName);
        }
        else
        {
            response += "(none)";
        }

        Util::Unit::SendChatMessage(world, networkState, socketID, response);

        return true;
    }

    bool HandleOnCheatDamage(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 damage = 0;
        if (!message.buffer->GetU32(damage))
            return false;

        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                targetEntity = targetInfo->target;
            }
        }

        auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

        ObjectGUID::Type targetType = targetObjectInfo.guid.GetType();
        if (targetType != ObjectGUID::Type::Creature && targetType != ObjectGUID::Type::Player)
            return true;

        Util::CombatEvent::AddDamageEvent(combatEventState, srcObjectInfo.guid, targetObjectInfo.guid, damage);

        return true;
    }
    bool HandleOnCheatKill(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
                targetEntity = targetInfo->target;
        }

        auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

        ObjectGUID::Type targetType = targetObjectInfo.guid.GetType();
        if (targetType != ObjectGUID::Type::Creature && targetType != ObjectGUID::Type::Player)
            return true;

        constexpr u32 amount = std::numeric_limits<u32>().max();
        Util::CombatEvent::AddDamageEvent(combatEventState, srcObjectInfo.guid, targetObjectInfo.guid, amount);

        return true;
    }
    bool HandleOnCheatResurrect(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& combatEventState = world.GetSingleton<Singletons::CombatEventState>();

        entt::entity targetEntity = entity;
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                targetEntity = targetInfo->target;
            }
        }

        auto& srcObjectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& targetObjectInfo = world.Get<Components::ObjectInfo>(targetEntity);

        ObjectGUID::Type targetType = targetObjectInfo.guid.GetType();
        if (targetType != ObjectGUID::Type::Creature && targetType != ObjectGUID::Type::Player)
            return true;

        Util::CombatEvent::AddResurrectEvent(combatEventState, srcObjectInfo.guid, targetObjectInfo.guid);
        return true;
    }
    bool HandleOnCheatMorph(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 displayID = 0;
        if (!message.buffer->GetU32(displayID))
            return false;

        auto& unitFields = world.Get<Components::UnitFields>(entity);
        Util::Unit::UpdateDisplayID(*world.registry, entity, unitFields, displayID);

        return true;
    }
    bool HandleOnCheatDemorph(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        auto& unitFields = world.Get<Components::UnitFields>(entity);

        u32 levelRaceGenderClassPacked = unitFields.fields.GetField<u32>(MetaGen::Shared::NetField::UnitNetFieldEnum::LevelRaceGenderClassPacked);
        GameDefine::UnitRace unitRace = static_cast<GameDefine::UnitRace>((levelRaceGenderClassPacked >> 16) & 0x7F);
        GameDefine::UnitGender unitGender = static_cast<GameDefine::UnitGender>((levelRaceGenderClassPacked >> 23) & 0x3);

        u32 nativeDisplayID = Util::Unit::GetDisplayIDFromRaceGender(unitRace, unitGender);
        Util::Unit::UpdateDisplayID(*world.registry, entity, unitFields, nativeDisplayID);

        return true;
    }
    bool HandleOnCheatUnitSetRace(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitRace unitRace = GameDefine::UnitRace::None;
        if (!message.buffer->Get(unitRace))
            return false;

        if (unitRace == GameDefine::UnitRace::None || unitRace > GameDefine::UnitRace::Troll)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetRace(gameCache, *world.registry, characterID, unitRace) != ECS::Result::Success)
            return false;

        auto& unitFields = world.Get<Components::UnitFields>(entity);
        Util::Unit::UpdateDisplayRace(*world.registry, entity, unitFields, unitRace);

        return true;
    }
    bool HandleOnCheatUnitSetGender(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitGender unitGender = GameDefine::UnitGender::None;
        if (!message.buffer->Get(unitGender))
            return false;

        if (unitGender == GameDefine::UnitGender::None || unitGender > GameDefine::UnitGender::Other)
            return false;

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetGender(gameCache, *world.registry, characterID, unitGender) != ECS::Result::Success)
            return false;

        auto& unitFields = world.Get<Components::UnitFields>(entity);
        Util::Unit::UpdateDisplayGender(*world.registry, entity, unitFields, unitGender);

        return true;
    }
    bool HandleOnCheatUnitSetClass(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::UnitClass unitClass = GameDefine::UnitClass::None;
        if (!message.buffer->Get(unitClass))
            return false;

        if (unitClass == GameDefine::UnitClass::None || unitClass > GameDefine::UnitClass::Druid)
            return false;

        entt::registry::context& ctx = registry->ctx();
        Singletons::GameCache& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetClass(gameCache, *world.registry, characterID, unitClass) != ECS::Result::Success)
            return false;

        return true;
    }
    bool HandleOnCheatUnitSetLevel(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u16 level = 0;
        if (!message.buffer->Get(level))
            return false;

        if (level == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        if (Util::Persistence::Character::CharacterSetLevel(gameCache, *world.registry, characterID, level) != ECS::Result::Success)
            return false;

        return true;
    }

    bool HandleOnCheatUnitSetFaction(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        if (!message.buffer->GetU16(factionID))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        if (!gameCache.factionRuntimeData)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction runtime data is unavailable.");
            return true;
        }

        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        auto* unitFaction = world.TryGet<Components::UnitFaction>(entity);
        auto* objectInfo = world.TryGet<Components::ObjectInfo>(entity);
        if (!unitFaction || !objectInfo || objectInfo->guid.GetType() != ObjectGUID::Type::Player)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction assignment requires a live player character.");
            return true;
        }

        if (!gameCache.factionRuntimeData->TryGetFactionIndex(factionID, factionIndex))
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, std::format("Faction {} is unknown.", factionID));
            return true;
        }

        auto result = gameCache.database->SetCharacterFaction(objectInfo->guid.GetCounter(), factionID);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_character_faction", result.Failure());
            return true;
        }
        if (result.Value() != Database::UpdateOutcome::Updated)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction assignment failed because the character row was not found.");
            return true;
        }

        if (!Util::Faction::SetAssignedFaction(world, entity, *gameCache.factionRuntimeData, factionID))
        {
            NC_LOG_ERROR("Failed to apply validated faction ID {0} to character {1} after persisting it", factionID, objectInfo->guid.GetCounter());
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction was persisted but could not be applied in memory; relog and inspect the server log.");
            return true;
        }

        const auto& assigned = gameCache.factionRuntimeData->GetDefinition(unitFaction->assignedFaction);
        const auto& effective = gameCache.factionRuntimeData->GetDefinition(unitFaction->effectiveFaction);
        Util::Unit::SendChatMessage(world, networkState, socketID, std::format("Assigned faction set to {} ({}); effective faction is {} ({}).", assigned.id, assigned.name, effective.id, effective.name));
        return true;
    }

    bool HandleOnCheatItemAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        bool isValidItemID = Util::Cache::ItemTemplateExistsByID(gameCache, itemID);
        bool playerHasBags = world.AllOf<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = world.Get<Components::PlayerContainers>(entity);

        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;
        bool canAddItem = Util::Container::GetFirstFreeSlotInBags(playerContainers, containerIndex, slotIndex);
        if (!canAddItem)
            return true;

        u64 characterID = world.Get<Components::ObjectInfo>(entity).guid.GetCounter();

        const auto& baseContainerItem = playerContainers.equipment.GetItem(containerIndex);

        u64 containerID = baseContainerItem.objectGUID.GetCounter();
        Database::Container& container = playerContainers.bags[containerIndex];

        u64 itemInstanceID;
        Database::OperationFailure databaseFailure = Database::OperationFailure::None;
        if (Util::Persistence::Character::ItemAdd(gameCache, *world.registry, entity, characterID, itemID, container,
                containerID, slotIndex, itemInstanceID, &databaseFailure) == ECS::Result::Success)
        {
            Database::ItemInstance* itemInstance = nullptr;
            if (Util::Cache::GetItemInstanceByID(gameCache, itemInstanceID, itemInstance))
            {
                ObjectGUID itemInstanceGUID = ObjectGUID::CreateItem(itemInstanceID);
                Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerItemAddPacket{ .guid = itemInstanceGUID, .itemID = itemInstance->itemID, .count = itemInstance->count, .durability = itemInstance->durability });
            }
        }
        else if (databaseFailure != Database::OperationFailure::None)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "add_character_item", databaseFailure);
        }

        return true;
    }
    bool HandleOnCheatItemRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 itemID = 0;
        u32 itemCount = 0;

        if (!message.buffer->GetU32(itemID))
            return false;

        if (!message.buffer->GetU32(itemCount))
            return false;

        if (itemCount == 0)
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        bool isValidItemID = Util::Cache::ItemTemplateExistsByID(gameCache, itemID);
        bool playerHasBags = world.AllOf<Components::PlayerContainers>(entity);

        if (!playerHasBags || !isValidItemID)
            return true;

        auto& playerContainers = world.Get<Components::PlayerContainers>(entity);
        Database::Container* container = nullptr;
        u64 containerID = 0;
        u16 containerIndex = Database::Container::INVALID_SLOT;
        u16 slotIndex = Database::Container::INVALID_SLOT;

        bool foundItem = Util::Container::GetFirstItemSlot(gameCache, playerContainers, itemID, container, containerID, containerIndex, slotIndex);
        if (!foundItem)
            return true;

        u64 characterID = world.Get<Components::ObjectInfo>(entity).guid.GetCounter();
        Database::OperationFailure databaseFailure = Database::OperationFailure::None;
        Util::Persistence::Character::ItemDelete(gameCache, *world.registry, entity, characterID, *container,
            containerID, slotIndex, &databaseFailure);
        if (databaseFailure != Database::OperationFailure::None)
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "delete_character_item", databaseFailure);

        return true;
    }
    bool HandleOnCheatItemSetTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemTemplate itemTemplate;
        if (!GameDefine::Database::ItemTemplate::Read(message.buffer, itemTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetItemTemplate(itemTemplate);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_item_template", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.itemTables.templateIDToTemplateDefinition[itemTemplate.id] = itemTemplate;
        }

        return true;
    }
    bool HandleOnCheatSetItemStatTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemStatTemplate itemStatTemplate;
        if (!GameDefine::Database::ItemStatTemplate::Read(message.buffer, itemStatTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetItemStatTemplate(itemStatTemplate);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_item_stat_template", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.itemTables.statTemplateIDToTemplateDefinition[itemStatTemplate.id] = itemStatTemplate;
        }

        return true;
    }
    bool HandleOnCheatSetItemArmorTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemArmorTemplate itemArmorTemplate;
        if (!GameDefine::Database::ItemArmorTemplate::Read(message.buffer, itemArmorTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetItemArmorTemplate(itemArmorTemplate);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_item_armor_template", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.itemTables.armorTemplateIDToTemplateDefinition[itemArmorTemplate.id] = itemArmorTemplate;
        }

        return true;
    }
    bool HandleOnCheatSetItemWeaponTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemWeaponTemplate itemWeaponTemplate;
        if (!GameDefine::Database::ItemWeaponTemplate::Read(message.buffer, itemWeaponTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetItemWeaponTemplate(itemWeaponTemplate);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_item_weapon_template", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.itemTables.weaponTemplateIDToTemplateDefinition[itemWeaponTemplate.id] = itemWeaponTemplate;
        }

        return true;
    }
    bool HandleOnCheatSetItemShieldTemplate(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::ItemShieldTemplate itemShieldTemplate;
        if (!GameDefine::Database::ItemShieldTemplate::Read(message.buffer, itemShieldTemplate))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetItemShieldTemplate(itemShieldTemplate);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_item_shield_template", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.itemTables.shieldTemplateIDToTemplateDefinition[itemShieldTemplate.id] = itemShieldTemplate;
        }

        return true;
    }

    bool HandleOnCheatMapAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::Map map;
        if (!GameDefine::Database::Map::Read(message.buffer, map))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetMap(map);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_map", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.mapTables.idToDefinition[map.id] = map;
        }

        return true;
    }
    bool HandleOnCheatGotoAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        u32 mapID = 0;
        vec3 position = vec3(0.0f);
        f32 orientation = 0.0f;

        if (!message.buffer->GetString(locationName))
            return false;

        if (!message.buffer->GetU32(mapID))
            return false;

        if (!message.buffer->Get(position))
            return false;

        if (!message.buffer->GetF32(orientation))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        auto createResult = gameCache.database->CreateMapLocation(locationName, mapID, position, orientation);
        if (createResult)
        {
            const auto& location = createResult.Value();
            gameCache.mapTables.locationIDToDefinition[location.id] = GameDefine::Database::MapLocation{ location.id, locationName, mapID, position.x, position.y, position.z, orientation };
            gameCache.mapTables.locationNameHashToID[locationNameHash] = location.id;
        }
        else
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "create_map_location", createResult.Failure());
        }

        return true;
    }
    bool HandleOnCheatGotoAddHere(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        auto& playerTransform = world.Get<Components::Transform>(entity);
        auto createResult = gameCache.database->CreateMapLocation(locationName, playerTransform.mapID,
            playerTransform.position, playerTransform.pitchYaw.y);
        if (createResult)
        {
            const auto& location = createResult.Value();
            gameCache.mapTables.locationIDToDefinition[location.id] = GameDefine::Database::MapLocation{ location.id, locationName, playerTransform.mapID, playerTransform.position.x, playerTransform.position.y, playerTransform.position.z, playerTransform.pitchYaw.y };
            gameCache.mapTables.locationNameHashToID[locationNameHash] = location.id;
        }
        else
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "create_map_location", createResult.Failure());
        }

        return true;
    }
    bool HandleOnCheatGotoRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());
        if (locationName.length() < 2 || locationName[0] == ' ' || !gameCache.mapTables.locationNameHashToID.contains(locationNameHash))
        {
            // Location is Invalid or Already Exists
            return true;
        }

        u32 locationID = gameCache.mapTables.locationNameHashToID[locationNameHash];
        auto deleteResult = gameCache.database->DeleteMapLocation(locationID);
        if (deleteResult)
        {
            gameCache.mapTables.locationIDToDefinition.erase(locationID);
            gameCache.mapTables.locationNameHashToID.erase(locationNameHash);
        }
        else if (!deleteResult)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "delete_map_location", deleteResult.Failure());
        }

        return true;
    }
    bool HandleOnCheatGotoMap(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 mapID = 0;
        if (!message.buffer->GetU32(mapID))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        GameDefine::Database::Map* mapDefinition = nullptr;
        if (!Util::Cache::GetMapByID(gameCache, mapID, mapDefinition))
            return true;

        std::string locationName = mapDefinition->name;
        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

        auto& transform = world.Get<Components::Transform>(entity);

        vec3 position = transform.position;
        f32 orientation = transform.pitchYaw.y;

        GameDefine::Database::MapLocation* location = nullptr;
        if (Util::Cache::GetLocationByHash(gameCache, locationNameHash, location))
        {
            position = vec3(location->positionX, location->positionY, location->positionZ);
            orientation = location->orientation;
        }

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);

        Util::Unit::TeleportToLocation(worldState, world, gameCache, networkState, entity, objectInfo, transform, visibilityInfo, mapID, position, orientation);

        return true;
    }
    bool HandleOnCheatGotoLocation(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string locationName;
        if (!message.buffer->GetString(locationName))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        StringUtils::ToLower(locationName);
        u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

        GameDefine::Database::MapLocation* location = nullptr;
        if (!Util::Cache::GetLocationByHash(gameCache, locationNameHash, location))
            return true;

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& transform = world.Get<Components::Transform>(entity);
        auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);

        vec3 position = vec3(location->positionX, location->positionY, location->positionZ);

        Util::Unit::TeleportToLocation(worldState, world, gameCache, networkState, entity, objectInfo, transform, visibilityInfo, location->mapID, position, location->orientation);

        return true;
    }
    bool HandleOnCheatGotoXYZ(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        vec3 position = vec3(0.0f);
        if (!message.buffer->Get(position))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& objectInfo = world.Get<Components::ObjectInfo>(entity);
        auto& transform = world.Get<Components::Transform>(entity);
        auto& visibilityInfo = world.Get<Components::VisibilityInfo>(entity);

        Util::Unit::TeleportToXYZ(world, networkState, entity, objectInfo, transform, visibilityInfo, position, transform.pitchYaw.y);
        return true;
    }

    bool HandleOnCheatTriggerAdd(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string name;
        u16 flags;
        u32 mapID;
        vec3 position;
        vec3 extents;

        if (!message.buffer->GetString(name))
            return false;

        if (!message.buffer->GetU16(flags))
            return false;

        if (!message.buffer->GetU32(mapID))
            return false;

        if (!message.buffer->Get(position))
            return false;

        if (!message.buffer->Get(extents))
            return false;

        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        const auto& playerTransform = world.Get<Components::Transform>(entity);
        if (playerTransform.mapID != mapID)
            return false;

        entt::entity triggerEntity = world.CreateEntity();

        Events::ProximityTriggerCreate event = {
            .name = name,
            .flags = static_cast<MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum>(flags),

            .mapID = mapID,
            .position = position,
            .extents = extents
        };

        world.Emplace<Events::ProximityTriggerCreate>(triggerEntity, event);
        world.EmplaceOrReplace<Components::AdministrativeRequestContext>(triggerEntity, socketID);

        return true;
    }
    bool HandleOnCheatTriggerRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 triggerID;

        if (!message.buffer->GetU32(triggerID))
            return false;

        auto& proximityTriggers = world.GetSingleton<Singletons::ProximityTriggers>();

        entt::entity triggerEntity;
        if (!Util::ProximityTrigger::ProximityTriggerGetByID(proximityTriggers, triggerID, triggerEntity))
            return true;

        world.EmplaceOrReplace<Events::ProximityTriggerNeedsDeinitialization>(triggerEntity, triggerID);
        world.EmplaceOrReplace<Components::AdministrativeRequestContext>(triggerEntity, socketID);

        return true;
    }

    bool HandleOnCheatSpellSet(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::Spell spell;
        if (!GameDefine::Database::Spell::Read(message.buffer, spell))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetSpell(spell);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_spell", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            gameCache.spellTables.idToDefinition[spell.id] = spell;
        }

        return true;
    }
    bool HandleOnCheatSpellEffectSet(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        GameDefine::Database::SpellEffect spellEffect;
        if (!GameDefine::Database::SpellEffect::Read(message.buffer, spellEffect))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetSpellEffect(spellEffect);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_spell_effect", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            auto cache = gameCache.database->LoadSpellCache();
            if (cache)
            {
                gameCache.spellTables = std::move(cache).Value();
            }
            else
            {
                ReportAdministrativeDatabaseFailure(registry, world, socketID, "reload_spell_cache", cache.Failure());
            }
        }

        return true;
    }
    bool HandleOnCheatSpellProcDataSet(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 spellProcDataID = 0;
        MetaGen::Shared::ClientDB::SpellProcDataRecord spellProcData;

        if (!message.buffer->GetU32(spellProcDataID))
            return false;

        if (!message.buffer->Deserialize(spellProcData))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetSpellProcData(spellProcDataID, spellProcData);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_spell_proc_data", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            auto cache = gameCache.database->LoadSpellCache();
            if (cache)
            {
                gameCache.spellTables = std::move(cache).Value();
            }
            else
            {
                ReportAdministrativeDatabaseFailure(registry, world, socketID, "reload_spell_cache", cache.Failure());
            }
        }

        return true;
    }
    bool HandleOnCheatSpellProcLinkSet(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        u32 spellProcLinkID = 0;
        MetaGen::Shared::ClientDB::SpellProcLinkRecord spellProcLink;

        if (!message.buffer->GetU32(spellProcLinkID))
            return false;

        if (!message.buffer->Deserialize(spellProcLink))
            return false;

        entt::registry::context& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();

        auto result = gameCache.database->SetSpellProcLink(spellProcLinkID, spellProcLink);
        if (!result)
        {
            ReportAdministrativeDatabaseFailure(registry, world, socketID, "set_spell_proc_link", result.Failure());
        }
        else if (result.Value() == Database::UpdateOutcome::Updated)
        {
            auto cache = gameCache.database->LoadSpellCache();
            if (cache)
            {
                gameCache.spellTables = std::move(cache).Value();
            }
            else
            {
                ReportAdministrativeDatabaseFailure(registry, world, socketID, "reload_spell_cache", cache.Failure());
            }
        }

        return true;
    }

    bool HandleOnCheatCreatureAddScript(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        std::string scriptName;
        if (!message.buffer->GetString(scriptName))
            return false;

        auto& creatureAIState = world.GetSingleton<Singletons::CreatureAIState>();

        u32 scriptNameHash = StringUtils::fnv1a_32(scriptName.c_str(), scriptName.length());
        if (!creatureAIState.loadedScriptNameHashes.contains(scriptNameHash))
            return true;

        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                if (auto* creatureAIInfo = world.TryGet<Components::CreatureAIInfo>(targetInfo->target))
                {
                    if (creatureAIInfo->scriptName == scriptName)
                        return true;

                    world.Emplace<Events::CreatureRemoveScript>(targetInfo->target);
                }

                world.Emplace<Events::CreatureAddScript>(targetInfo->target, scriptName);
            }
        }

        return true;
    }
    bool HandleOnCheatCreatureRemoveScript(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target))
            {
                world.Emplace<Events::CreatureRemoveScript>(targetInfo->target);
            }
        }

        return true;
    }

    bool HandleOnCheatCreatureMove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target) && world.AllOf<Components::CreatureInfo>(targetInfo->target))
            {
                auto& transform = world.Get<Components::Transform>(entity);
                Util::Movement::MoveTo(world, targetInfo->target, transform.position);
            }
        }

        return true;
    }
    bool HandleOnCheatCreatureFollow(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target) && world.AllOf<Components::CreatureInfo>(targetInfo->target))
            {
                Util::Movement::Follow(world, targetInfo->target, entity);
            }
        }

        return true;
    }
    bool HandleOnCheatCreatureWander(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target) && world.AllOf<Components::CreatureInfo>(targetInfo->target))
            {
                auto& targetTransform = world.Get<Components::Transform>(targetInfo->target);
                Util::Movement::Wander(world, targetInfo->target, targetTransform.position);
            }
        }

        return true;
    }
    bool HandleOnCheatCreatureStop(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        if (auto* targetInfo = world.TryGet<Components::TargetInfo>(entity))
        {
            if (world.ValidEntity(targetInfo->target) && world.AllOf<Components::CreatureInfo>(targetInfo->target))
            {
                Util::Movement::Stop(world, targetInfo->target);
            }
        }

        return true;
    }

    bool HandleOnCheatFactionReaction(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID observerGUID;
        ObjectGUID targetGUID;
        if (!message.buffer->Deserialize(observerGUID) || !message.buffer->Deserialize(targetGUID))
            return false;

        auto& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        if (!gameCache.factionRuntimeData)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction runtime data is unavailable.");
            return true;
        }

        const entt::entity observer = world.GetEntity(observerGUID);
        const entt::entity target = world.GetEntity(targetGUID);
        const auto* observerFaction = world.TryGet<Components::UnitFaction>(observer);
        const auto* targetFaction = world.TryGet<Components::UnitFaction>(target);
        if (observer == entt::null || target == entt::null || !observerFaction || !targetFaction)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction reaction requires two live unit GUIDs in the current world.");
            return true;
        }

        const auto& runtime = *gameCache.factionRuntimeData;
        const auto& observerAssigned = runtime.GetDefinition(observerFaction->assignedFaction);
        const auto& observerEffective = runtime.GetDefinition(observerFaction->effectiveFaction);
        const auto& targetAssigned = runtime.GetDefinition(targetFaction->assignedFaction);
        const auto& targetEffective = runtime.GetDefinition(targetFaction->effectiveFaction);
        const auto observerBounds = Gameplay::Faction::ReactionBounds::Unpack(observerFaction->effectivePlayerReactionBounds);
        const auto targetBounds = Gameplay::Faction::ReactionBounds::Unpack(targetFaction->effectivePlayerReactionBounds);
        const auto baseForward = runtime.GetRelation(observerFaction->effectiveFaction, targetFaction->effectiveFaction);
        const auto baseReverse = runtime.GetRelation(targetFaction->effectiveFaction, observerFaction->effectiveFaction);
        const auto forward = Util::Faction::GetReaction(world, observer, target, runtime);
        const auto reverse = Util::Faction::GetReaction(world, target, observer, runtime);

        std::string reputationText = "none";
        if (const auto* reputation = world.TryGet<Components::CharacterReputation>(observer))
        {
            if (const auto* state = Util::Faction::FindReputation(*reputation, targetFaction->effectiveFaction))
            {
                const auto& standing = runtime.GetStanding(state->value);
                reputationText = std::format("{} ({}), flags {}", state->value, standing.name, state->flags);
            }
        }

        const auto* observerModifiers = world.TryGet<Components::FactionModifiers>(observer);
        const auto* targetModifiers = world.TryGet<Components::FactionModifiers>(target);
        const size_t observerContributorCount = observerModifiers ? observerModifiers->contributors.size() : 0;
        const size_t targetContributorCount = targetModifiers ? targetModifiers->contributors.size() : 0;
        std::string presentationText = "n/a";
        std::string interactionText = "n/a";
        if (world.TryGet<Components::CharacterReputation>(observer))
        {
            presentationText = std::string(ReactionName(Util::Faction::GetPresentationReaction(world, observer, target, runtime)));
            interactionText = Util::Faction::CanInteract(world, observer, target, runtime) ? "true" : "false";
        }

        const std::string response = std::format(
            "Faction reaction:\n"
            "- Observer: {} assigned {} ({}) effective {} ({}) bounds {}..{} modifiers {}\n"
            "- Target: {} assigned {} ({}) effective {} ({}) bounds {}..{} modifiers {}\n"
            "- Base relation observer -> target: {}\n"
            "- Base relation target -> observer: {}\n"
            "- Observer persistent reputation for target faction: {}\n"
            "- Resolved observer -> target: {}\n"
            "- Resolved target -> observer: {}\n"
            "- Presentation: {}\n"
            "- CanAttack: {}\n"
            "- CanAssist: {}\n"
            "- CanInteract: {}",
            observerGUID.ToString(), observerAssigned.id, observerAssigned.name, observerEffective.id,
            observerEffective.name, ReactionName(observerBounds.minimum), ReactionName(observerBounds.maximum),
            observerContributorCount, targetGUID.ToString(), targetAssigned.id, targetAssigned.name,
            targetEffective.id, targetEffective.name, ReactionName(targetBounds.minimum),
            ReactionName(targetBounds.maximum), targetContributorCount, ReactionName(baseForward),
            ReactionName(baseReverse), reputationText, ReactionName(forward), ReactionName(reverse),
            presentationText, Util::Faction::CanAttack(world, observer, target, runtime),
            Util::Faction::CanAssist(world, observer, target, runtime), interactionText);
        Util::Unit::SendChatMessage(world, networkState, socketID, response);
        return true;
    }

    bool HandleOnCheatFactionReputationInfo(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        if (!gameCache.factionRuntimeData)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction runtime data is unavailable.");
            return true;
        }

        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        if (!TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction))
        {
            return true;
        }

        const auto& runtime = *gameCache.factionRuntimeData;
        if (factionID != Gameplay::Faction::NONE_FACTION_ID)
        {
            Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
            if (TryGetAdministrativeReputationFaction(world, networkState, socketID, runtime, factionID, factionIndex))
            {
                SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, runtime, *characterFaction, *reputation, factionIndex);
            }

            return true;
        }

        const auto& assigned = runtime.GetDefinition(characterFaction->assignedFaction);
        Util::Unit::SendChatMessage(world, networkState, socketID,
            std::format("Reputation for {}: {} active entries, {} pending upserts, {} pending deletes; "
                        "assigned faction {} ({})",
                characterGUID.ToString(), reputation->byFaction.size(), reputation->dirtyByFaction.size(),
                reputation->removedDirtyByFaction.size(), assigned.id, assigned.name));

        std::vector<Gameplay::Faction::FactionIndex> factions;
        factions.reserve(reputation->byFaction.size() + reputation->removedDirtyByFaction.size());
        for (const auto& [faction, state] : reputation->byFaction)
        {
            factions.push_back(faction);
        }

        for (const auto& [faction, source] : reputation->removedDirtyByFaction)
        {
            factions.push_back(faction);
        }

        std::sort(factions.begin(), factions.end(), [&](auto left, auto right)
        {
            return runtime.GetFactionID(left) < runtime.GetFactionID(right);
        });

        for (Gameplay::Faction::FactionIndex faction : factions)
        {
            SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, runtime, *characterFaction, *reputation, faction);
        }

        return true;
    }

    bool HandleOnCheatFactionReputationSet(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        i32 value = 0;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID) || !message.buffer->GetI32(value))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        if (!gameCache.factionRuntimeData || !TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction) || !TryGetAdministrativeReputationFaction(world, networkState, socketID, *gameCache.factionRuntimeData, factionID, factionIndex))
            return true;

        const bool changed = Util::Faction::SetReputation(world, characterEntity, *gameCache.factionRuntimeData, factionID, value, { .type = Gameplay::Faction::ReputationSourceType::Administrative });
        Util::Unit::SendChatMessage(world, networkState, socketID, changed ? "Administrative reputation value updated." : "Administrative reputation set made no change.");
        SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
        return true;
    }

    bool HandleOnCheatFactionReputationModify(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        i32 delta = 0;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID) || !message.buffer->GetI32(delta))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        if (!gameCache.factionRuntimeData || !TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction) || !TryGetAdministrativeReputationFaction(world, networkState, socketID, *gameCache.factionRuntimeData, factionID, factionIndex))
            return true;

        const bool changed = Util::Faction::ModifyReputation(world, characterEntity, *gameCache.factionRuntimeData, factionID, delta, { .type = Gameplay::Faction::ReputationSourceType::Administrative });
        Util::Unit::SendChatMessage(world, networkState, socketID, changed ? "Administrative reputation value modified." : "Administrative reputation modification made no change.");
        SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
        return true;
    }

    bool HandleOnCheatFactionReputationRemove(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        if (!gameCache.factionRuntimeData || !TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction) || !TryGetAdministrativeReputationFaction(world, networkState, socketID, *gameCache.factionRuntimeData, factionID, factionIndex))
            return true;

        const bool changed = Util::Faction::RemoveReputation(world, characterEntity, *gameCache.factionRuntimeData, factionID, { .type = Gameplay::Faction::ReputationSourceType::Administrative });
        Util::Unit::SendChatMessage(world, networkState, socketID, changed ? "Administrative reputation entry removed." : "Administrative reputation remove made no change.");
        SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
        return true;
    }

    bool HandleOnCheatFactionReputationSetFlags(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        u16 flags = 0;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID) || !message.buffer->GetU16(flags))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        if (!gameCache.factionRuntimeData || !TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction) || !TryGetAdministrativeReputationFaction(world, networkState, socketID, *gameCache.factionRuntimeData, factionID, factionIndex))
            return true;

        if ((flags & ~Gameplay::Faction::REPUTATION_FLAG_MASK) != 0)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, std::format("Reputation flags {} contain unknown bits.", flags));
            SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
            return true;
        }

        const auto& definition = gameCache.factionRuntimeData->GetDefinition(factionIndex);
        if ((flags & static_cast<u16>(Gameplay::Faction::ReputationFlags::AtWar)) != 0 && !Gameplay::Faction::HasFlag(definition.flags, Gameplay::Faction::DefinitionFlags::CanSetAtWar))
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, std::format("Faction {} ({}) cannot be set at war.", definition.id, definition.name));
            SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
            return true;
        }

        const bool changed = Util::Faction::SetReputationFlags(world, characterEntity, *gameCache.factionRuntimeData, factionID, flags, { .type = Gameplay::Faction::ReputationSourceType::Administrative });
        Util::Unit::SendChatMessage(world, networkState, socketID, changed ? "Administrative reputation flags updated." : "Administrative reputation flag set made no change.");
        SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
        return true;
    }

    bool HandleOnCheatFactionReputationLock(entt::registry* registry, World& world, Network::SocketID socketID, entt::entity entity, Network::Message& message)
    {
        ObjectGUID characterGUID;
        Gameplay::Faction::FactionID factionID = Gameplay::Faction::NONE_FACTION_ID;
        u8 locked = 0;
        if (!message.buffer->Deserialize(characterGUID) || !message.buffer->GetU16(factionID) || !message.buffer->GetU8(locked))
            return false;

        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();
        entt::entity characterEntity = entt::null;
        Components::CharacterReputation* reputation = nullptr;
        Components::UnitFaction* characterFaction = nullptr;
        Gameplay::Faction::FactionIndex factionIndex = Gameplay::Faction::INVALID_FACTION_INDEX;
        if (locked > 1)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Faction reputation lock expects 0 (unlock) or 1 (lock).");
            return true;
        }

        if (!gameCache.factionRuntimeData || !TryGetAdministrativeReputationTarget(world, networkState, socketID, characterGUID, characterEntity, reputation, characterFaction) || !TryGetAdministrativeReputationFaction(world, networkState, socketID, *gameCache.factionRuntimeData, factionID, factionIndex))
            return true;

        const auto* state = Util::Faction::FindReputation(*reputation, factionIndex);
        if (!state)
        {
            Util::Unit::SendChatMessage(world, networkState, socketID, "Cannot lock an absent reputation entry; discover or set it first.");
            SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
            return true;
        }

        const u16 lockFlag = static_cast<u16>(Gameplay::Faction::ReputationFlags::Locked);
        const u16 flags = locked != 0 ? state->flags | lockFlag : state->flags & ~lockFlag;
        const bool changed = Util::Faction::SetReputationFlags(world, characterEntity, *gameCache.factionRuntimeData, factionID, flags, { .type = Gameplay::Faction::ReputationSourceType::Administrative });
        Util::Unit::SendChatMessage(world, networkState, socketID,
            changed ? (locked != 0 ? "Administrative reputation entry locked."
                                   : "Administrative reputation entry unlocked.")
                    : "Administrative reputation lock made no change.");
        SendAdministrativeReputationEntry(world, networkState, socketID, characterEntity, *gameCache.factionRuntimeData, *characterFaction, *reputation, factionIndex);
        return true;
    }

    bool HandleOnSendCheatCommand(World* world, entt::entity entity, Network::SocketID socketID, Network::Message& message)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();

        auto command = MetaGen::Shared::Cheat::CheatCommandEnum::None;
        if (!message.buffer->Get(command))
            return false;

        MetaGen::Server::Permission::Permission requiredPermission;
        if (!GetCheatCommandPermission(command, requiredPermission))
            return true;
        const auto* permissionInfo = world->TryGet<Components::CharacterPermissionInfo>(entity);
        if (!permissionInfo || !Util::Permission::HasPermission(ctx.get<Singletons::GameCache>(),
                                   permissionInfo->effectivePermissions, requiredPermission))
        {
            Util::Unit::SendChatMessage(*world, networkState, socketID,
                "You do not have permission to use this administrative command.");
            return true;
        }

        switch (command)
        {
            case MetaGen::Shared::Cheat::CheatCommandEnum::CharacterAdd:
            {
                return HandleOnCheatCharacterAdd(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::CharacterRemove:
            {
                return HandleOnCheatCharacterRemove(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureAdd:
            {
                return HandleOnCheatCreatureAdd(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureRemove:
            {
                return HandleOnCheatCreatureRemove(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureInfo:
            {
                return HandleOnCheatCreatureInfo(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::Damage:
            {
                return HandleOnCheatDamage(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::Kill:
            {
                return HandleOnCheatKill(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::Resurrect:
            {
                return HandleOnCheatResurrect(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitMorph:
            {
                return HandleOnCheatMorph(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitDemorph:
            {
                return HandleOnCheatDemorph(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitSetRace:
            {
                return HandleOnCheatUnitSetRace(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitSetGender:
            {
                return HandleOnCheatUnitSetGender(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitSetClass:
            {
                return HandleOnCheatUnitSetClass(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitSetLevel:
            {
                return HandleOnCheatUnitSetLevel(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::UnitSetFaction:
            {
                return HandleOnCheatUnitSetFaction(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemAdd:
            {
                return HandleOnCheatItemAdd(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemRemove:
            {
                return HandleOnCheatItemRemove(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemSetTemplate:
            {
                return HandleOnCheatItemSetTemplate(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemSetStatTemplate:
            {
                return HandleOnCheatSetItemStatTemplate(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemSetArmorTemplate:
            {
                return HandleOnCheatSetItemArmorTemplate(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemSetWeaponTemplate:
            {
                return HandleOnCheatSetItemWeaponTemplate(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::ItemSetShieldTemplate:
            {
                return HandleOnCheatSetItemShieldTemplate(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::MapAdd:
            {
                return HandleOnCheatMapAdd(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::GotoAddHere:
            {
                return HandleOnCheatGotoAddHere(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::GotoRemove:
            {
                return HandleOnCheatGotoRemove(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::GotoMap:
            {
                return HandleOnCheatGotoMap(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::GotoLocation:
            {
                return HandleOnCheatGotoLocation(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::GotoXYZ:
            {
                return HandleOnCheatGotoXYZ(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::TriggerAdd:
            {
                return HandleOnCheatTriggerAdd(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::TriggerRemove:
            {
                return HandleOnCheatTriggerRemove(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::SpellSet:
            {
                return HandleOnCheatSpellSet(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::SpellEffectSet:
            {
                return HandleOnCheatSpellEffectSet(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::SpellProcDataSet:
            {
                return HandleOnCheatSpellProcDataSet(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::SpellProcLinkSet:
            {
                return HandleOnCheatSpellProcLinkSet(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureAddScript:
            {
                return HandleOnCheatCreatureAddScript(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureRemoveScript:
            {
                return HandleOnCheatCreatureRemoveScript(registry, *world, socketID, entity, message);
            }

            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureMove:
            {
                return HandleOnCheatCreatureMove(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureFollow:
            {
                return HandleOnCheatCreatureFollow(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureWander:
            {
                return HandleOnCheatCreatureWander(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::CreatureStop:
            {
                return HandleOnCheatCreatureStop(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReaction:
            {
                return HandleOnCheatFactionReaction(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationInfo:
            {
                return HandleOnCheatFactionReputationInfo(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationSet:
            {
                return HandleOnCheatFactionReputationSet(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationModify:
            {
                return HandleOnCheatFactionReputationModify(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationRemove:
            {
                return HandleOnCheatFactionReputationRemove(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationSetFlags:
            {
                return HandleOnCheatFactionReputationSetFlags(registry, *world, socketID, entity, message);
            }
            case MetaGen::Shared::Cheat::CheatCommandEnum::FactionReputationLock:
            {
                return HandleOnCheatFactionReputationLock(registry, *world, socketID, entity, message);
            }

            default:
                break;
        }

        return true;
    }

    bool HandleOnConnect(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientConnectPacket& packet)
    {
        if (!StringUtils::StringIsAlphaAndAtLeastLength(packet.accountName, 2))
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        entt::registry::context& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();

        AccountLoginRequest accountLoginRequest = {
            .socketID = socketID,
            .name = packet.accountName
        };
        networkState.accountLoginRequest.enqueue(accountLoginRequest);

        return true;
    }
    bool HandleOnAuthChallenge(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientAuthChallengePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        auto& authInfo = registry->get<Components::AuthenticationInfo>(entity);
        if (authInfo.state != AuthenticationState::Step1)
        {
            authInfo.state = AuthenticationState::Failed;
            return false;
        }

        auto& accountInfo = registry->get<Components::AccountInfo>(entity);

        unsigned char response2[crypto_spake_RESPONSE2BYTES];
        i32 result = crypto_spake_step2(&authInfo.serverState, response2, accountInfo.name.c_str(), accountInfo.name.length(), "NovusEngine", 11, authInfo.blob, packet.challenge.data());
        if (result != 0)
        {
            authInfo.state = AuthenticationState::Failed;
            return false;
        }

        authInfo.state = AuthenticationState::Step2;

        MetaGen::Shared::Packet::ServerAuthProofPacket authProofPacket;
        std::memcpy(authProofPacket.proof.data(), response2, crypto_spake_RESPONSE2BYTES);

        Util::Network::SendPacket(networkState, socketID, authProofPacket);

        return true;
    }
    bool HandleOnAuthProof(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientAuthProofPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        auto& authInfo = registry->get<Components::AuthenticationInfo>(entity);
        if (authInfo.state != AuthenticationState::Step2)
        {
            authInfo.state = AuthenticationState::Failed;
            return false;
        }

        auto& sessionKeys = networkState.socketIDToSessionKeys[socketID];
        i32 result = crypto_spake_step4(&authInfo.serverState, &sessionKeys, packet.proof.data());
        if (result != 0)
        {
            authInfo.state = AuthenticationState::Failed;
            return false;
        }

        authInfo.state = AuthenticationState::Completed;

        CharacterListRequest characterListRequest = {
            .socketID = socketID,
            .socketEntity = entity
        };
        networkState.characterListRequest.enqueue(characterListRequest);

        return true;
    }
    bool HandleOnCharacterSelect(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientCharacterSelectPacket& packet)
    {
        if (world || entity == entt::null)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        auto& authInfo = registry->get<Components::AuthenticationInfo>(entity);
        if (authInfo.state != AuthenticationState::Completed)
        {
            authInfo.state = AuthenticationState::Failed;
            return false;
        }

        auto& characterListInfo = registry->get<Components::CharacterListInfo>(entity);

        u32 numCharacters = static_cast<u32>(characterListInfo.list.size());
        if (packet.characterIndex >= numCharacters)
            return false;

        auto& characterEntry = characterListInfo.list[packet.characterIndex];

        CharacterLoginRequest characterLoginRequest = {
            .socketID = socketID,
            .name = characterEntry.name
        };
        networkState.characterLoginRequest.enqueue(characterLoginRequest);

        return true;
    }
    bool HandleOnCharacterLogout(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientCharacterLogoutPacket& packet)
    {
        if (!world || entity == entt::null)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        if (world->ValidEntity(entity))
        {
            world->EmplaceOrReplace<Events::CharacterNeedsDeinitialization>(entity);
        }

        networkState.server->SetSocketIDLane(socketID, Network::DEFAULT_LANE_ID);
        ECS::Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerCharacterLogoutPacket{});
        return true;
    }
    bool HandleOnPing(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientPingPacket& packet)
    {
        if (!world || entity == entt::null)
            return true;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& timeState = ctx.get<Singletons::TimeState>();

        auto& netInfo = world->Get<Components::NetInfo>(entity);
        auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        u64 timeSinceLastPing = currentTime - netInfo.lastPingTime;
        u8 serverDiff = static_cast<u8>(timeState.deltaTime * 1000.0f);

        if (timeSinceLastPing < Components::NetInfo::PING_INTERVAL - 1000)
        {
            netInfo.numEarlyPings++;
        }
        else if (timeSinceLastPing > Components::NetInfo::PING_INTERVAL + 1000)
        {
            netInfo.numLatePings++;
        }

        else
        {
            netInfo.numEarlyPings = 0;
            netInfo.numLatePings = 0;
        }

        if (netInfo.numEarlyPings > Components::NetInfo::MAX_EARLY_PINGS ||
            netInfo.numLatePings > Components::NetInfo::MAX_LATE_PINGS)
        {
            networkState.server->CloseSocketID(socketID);
            return true;
        }

        netInfo.ping = packet.ping;
        netInfo.lastPingTime = currentTime;

        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerUpdateStatsPacket{ .serverTickTime = serverDiff });

        return true;
    }

    bool HandleOnClientUnitTargetUpdate(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientUnitTargetUpdatePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();
        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        auto& targetInfo = world->Get<Components::TargetInfo>(entity);

        entt::entity targetEntity = entt::null;
        if (packet.targetGUID.IsValid())
        {
            const bool targetIsSelf = packet.targetGUID == objectInfo.guid;
            const bool targetIsVisible = targetIsSelf ||
                                         (packet.targetGUID.GetType() == ObjectGUID::Type::Player && visibilityInfo.visiblePlayers.contains(packet.targetGUID)) ||
                                         (packet.targetGUID.GetType() == ObjectGUID::Type::Creature && visibilityInfo.visibleCreatures.contains(packet.targetGUID));
            targetEntity = world->GetEntity(packet.targetGUID);
            if (!targetIsVisible || !world->ValidEntity(targetEntity) || !world->AllOf<Components::UnitFaction>(targetEntity))
                return true;
        }

        targetInfo.target = targetEntity;
        if (targetEntity != entt::null && gameCache.factionRuntimeData)
        {
            const auto& targetFaction = world->Get<Components::UnitFaction>(targetEntity);
            const Gameplay::Faction::FactionID factionID = gameCache.factionRuntimeData->GetFactionID(targetFaction.effectiveFaction);
            Util::Faction::DiscoverReputation(*world, entity, *gameCache.factionRuntimeData, factionID,
                Gameplay::Faction::DiscoverySource::Target);
        }

        ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, false, MetaGen::Shared::Packet::ServerUnitTargetUpdatePacket{ .guid = objectInfo.guid, .targetGUID = packet.targetGUID });

        return true;
    }
    bool HandleOnClientUnitMove(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientUnitMovePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& transform = world->Get<Components::Transform>(entity);
        transform.position = packet.position;
        transform.pitchYaw = packet.pitchYaw;

        world->playerVisData.Update(objectInfo.guid, packet.position.x, packet.position.z);

        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, false, ECS::Util::Network::CreateReplicationSendOptions(MetaGen::Shared::Packet::ServerUnitMovePacket::PACKET_ID, objectInfo.guid), MetaGen::Shared::Packet::ServerUnitMovePacket{ .guid = objectInfo.guid, .movementFlags = packet.movementFlags, .position = packet.position, .pitchYaw = packet.pitchYaw, .verticalVelocity = packet.verticalVelocity });

        return true;
    }
    bool HandleOnClientContainerSwapSlots(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::SharedContainerSwapSlotsPacket& packet)
    {
        if (packet.srcContainer == packet.dstContainer && packet.srcSlot == packet.dstSlot)
            return false;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        auto& playerContainers = world->Get<Components::PlayerContainers>(entity);

        Database::Container* srcContainer = nullptr;
        u64 srcContainerID = 0;

        Database::Container* dstContainer = nullptr;
        u64 dstContainerID = 0;

        bool isSameContainer = packet.srcContainer == packet.dstContainer;
        if (isSameContainer)
        {
            if (packet.srcContainer == 0)
            {
                srcContainer = &playerContainers.equipment;
                srcContainerID = 0;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.srcContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            u32 numContainerSlots = srcContainer->GetTotalSlots();
            if (packet.srcSlot >= numContainerSlots || packet.dstSlot >= numContainerSlots)
                return false;

            auto& srcItem = srcContainer->GetItem(packet.srcSlot);
            if (srcItem.IsEmpty())
                return false;

            dstContainer = srcContainer;
            dstContainerID = srcContainerID;
        }
        else
        {
            if (packet.srcContainer == 0)
            {
                srcContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.srcContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                srcContainer = &playerContainers.bags[bagIndex];
                srcContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            if (packet.dstContainer == 0)
            {
                dstContainer = &playerContainers.equipment;
            }
            else
            {
                u32 bagIndex = PLAYER_BAG_INDEX_START + packet.dstContainer - 1;
                const Database::ContainerItem& bagContainerItem = playerContainers.equipment.GetItem(bagIndex);

                if (bagContainerItem.IsEmpty())
                    return false;

                dstContainer = &playerContainers.bags[bagIndex];
                dstContainerID = bagContainerItem.objectGUID.GetCounter();
            }

            if (packet.srcSlot >= srcContainer->GetTotalSlots() || packet.dstSlot >= dstContainer->GetTotalSlots())
                return false;

            auto& srcItem = srcContainer->GetItem(packet.srcSlot);
            if (srcItem.IsEmpty())
                return false;
        }

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        auto& visualItems = world->Get<Components::UnitVisualItems>(entity);
        u64 characterID = objectInfo.guid.GetCounter();

        Database::OperationFailure databaseFailure = Database::OperationFailure::None;
        if (Util::Persistence::Character::ItemSwap(gameCache, characterID, *srcContainer, srcContainerID,
                packet.srcSlot, *dstContainer, dstContainerID, packet.dstSlot, &databaseFailure) == ECS::Result::Success)
        {
            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::SharedContainerSwapSlotsPacket{ .srcContainer = packet.srcContainer, .dstContainer = packet.dstContainer, .srcSlot = packet.srcSlot, .dstSlot = packet.dstSlot });

            if (srcContainerID == 0)
            {
                auto equippedSlot = static_cast<MetaGen::Shared::Unit::ItemEquipSlotEnum>(packet.srcSlot);
                if (equippedSlot >= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentStart && equippedSlot <= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd)
                {
                    const Database::ContainerItem& containerItem = srcContainer->GetItem(packet.srcSlot);
                    bool isContainerItemEmpty = containerItem.IsEmpty();
                    u32 itemID = 0;

                    if (!isContainerItemEmpty)
                    {
                        Database::ItemInstance* itemInstance = nullptr;
                        if (Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                            itemID = itemInstance->itemID;
                    }

                    ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitEquippedItemUpdatePacket{ .guid = objectInfo.guid, .slot = static_cast<u8>(packet.srcSlot), .itemID = itemID });

                    visualItems.equippedItemIDs[packet.srcSlot] = itemID;
                    visualItems.dirtyItemIDs.insert(packet.srcSlot);
                    world->EmplaceOrReplace<Events::CharacterNeedsVisualItemUpdate>(entity);
                    world->EmplaceOrReplace<Events::CharacterNeedsRecalculateStatsUpdate>(entity);
                }
            }

            if (dstContainerID == 0)
            {
                auto equippedSlot = static_cast<MetaGen::Shared::Unit::ItemEquipSlotEnum>(packet.dstSlot);
                if (equippedSlot >= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentStart && equippedSlot <= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd)
                {
                    const Database::ContainerItem& containerItem = dstContainer->GetItem(packet.dstSlot);
                    bool isContainerItemEmpty = containerItem.IsEmpty();
                    u32 itemID = 0;

                    if (!isContainerItemEmpty)
                    {
                        Database::ItemInstance* itemInstance = nullptr;
                        if (Util::Cache::GetItemInstanceByID(gameCache, containerItem.objectGUID.GetCounter(), itemInstance))
                            itemID = itemInstance->itemID;
                    }

                    ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, true, MetaGen::Shared::Packet::ServerUnitEquippedItemUpdatePacket{ .guid = objectInfo.guid, .slot = static_cast<u8>(packet.dstSlot), .itemID = itemID });

                    visualItems.equippedItemIDs[packet.dstSlot] = itemID;
                    visualItems.dirtyItemIDs.insert(packet.dstSlot);
                    world->EmplaceOrReplace<Events::CharacterNeedsVisualItemUpdate>(entity);
                    world->EmplaceOrReplace<Events::CharacterNeedsRecalculateStatsUpdate>(entity);
                }
            }
        }
        else if (databaseFailure != Database::OperationFailure::None)
        {
            ReportAdministrativeDatabaseFailure(registry, *world, socketID, "swap_character_items", databaseFailure);
        }

        return true;
    }

    bool HandleOnClientTriggerEnter(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientTriggerEnterPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();
        auto& networkState = ctx.get<Singletons::NetworkState>();

        auto& proximityTriggers = world->GetSingleton<Singletons::ProximityTriggers>();

        entt::entity triggerEntity;
        if (!Util::ProximityTrigger::ProximityTriggerGetByID(proximityTriggers, packet.triggerID, triggerEntity))
            return true;

        auto& proximityTrigger = world->Get<Components::ProximityTrigger>(triggerEntity);

        // Only process if the server is authoritative for this trigger
        bool isServerAuthoritative = (proximityTrigger.flags & MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::IsServerAuthorative) != MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum::None;
        if (!isServerAuthoritative)
            return false;

        auto& playerTransform = world->Get<Components::Transform>(entity);
        auto& playerAABB = world->Get<Components::AABB>(entity);
        auto& triggerTransform = world->Get<Components::Transform>(triggerEntity);
        auto& triggerAABB = world->Get<Components::AABB>(triggerEntity);

        if (!Util::Collision::Overlaps(playerTransform, playerAABB, triggerTransform, triggerAABB))
            return true;

        Util::ProximityTrigger::AddPlayerToTriggerEntered(*world, proximityTrigger, triggerEntity, entity);

        return true;
    }
    bool HandleOnClientSendChatMessage(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientSendChatMessagePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& ctx = registry->ctx();

        auto& networkState = ctx.get<Singletons::NetworkState>();

        auto& objectInfo = world->Get<Components::ObjectInfo>(entity);
        auto& visibilityInfo = world->Get<Components::VisibilityInfo>(entity);
        ECS::Util::Network::SendToNearby(networkState, *world, entity, visibilityInfo, true, MetaGen::Shared::Packet::ServerSendChatMessagePacket{ .guid = objectInfo.guid, .message = packet.message });

        return true;
    }
    bool HandleOnClientSpellCast(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientSpellCastPacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& gameCache = registry->ctx().get<Singletons::GameCache>();
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        u8 result = 0;

        GameDefine::Database::Spell* spell = nullptr;
        auto& casterSpellCooldownHistory = world->Get<Components::UnitSpellCooldownHistory>(entity);

        if (!Util::Cache::GetSpellByID(gameCache, packet.spellID, spell))
        {
            result = 0x1;
        }
        else
        {
            f32 cooldown = Util::Unit::GetSpellCooldownRemaining(casterSpellCooldownHistory, packet.spellID);
            if (cooldown > 0.0f)
                result = 0x2;
        }

        if (result != 0x0)
        {
            Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerSpellCastResultPacket{ .result = result });

            return true;
        }

        auto& targetInfo = world->Get<Components::TargetInfo>(entity);
        if (SpellRequiresAttackPermission(gameCache, packet.spellID) && (!gameCache.factionRuntimeData || !world->ValidEntity(targetInfo.target) || !Util::Faction::CanAttack(*world, entity, targetInfo.target, *gameCache.factionRuntimeData)))
        {
            Util::Network::SendPacket(networkState, socketID,
                MetaGen::Shared::Packet::ServerSpellCastResultPacket{
                    .result = 0x3 });
            return true;
        }

        auto& characterSpellCastInfo = world->Get<Components::CharacterSpellCastInfo>(entity);

        entt::entity spellEntity = entt::null;
        if (characterSpellCastInfo.activeSpellEntity == entt::null)
        {
            spellEntity = world->CreateEntity();
            characterSpellCastInfo.activeSpellEntity = spellEntity;

            world->Emplace<Tags::IsUnpreparedSpell>(spellEntity);
        }
        else
        {
            if (characterSpellCastInfo.queuedSpellEntity == entt::null)
            {
                spellEntity = world->CreateEntity();
                characterSpellCastInfo.queuedSpellEntity = spellEntity;
            }
            else
            {
                spellEntity = characterSpellCastInfo.queuedSpellEntity;
            }
        }

        auto& casterObjectInfo = world->Get<Components::ObjectInfo>(entity);

        ObjectGUID targetGUID = ObjectGUID::Empty;
        if (world->ValidEntity(targetInfo.target))
        {
            auto& targetObjectInfo = world->Get<Components::ObjectInfo>(targetInfo.target);
            targetGUID = targetObjectInfo.guid;
        }

        auto& spellInfo = world->EmplaceOrReplace<Components::SpellInfo>(spellEntity);
        auto& spellEffectInfo = world->EmplaceOrReplace<Components::SpellEffectInfo>(spellEntity);

        spellInfo.spellID = packet.spellID;
        spellInfo.caster = casterObjectInfo.guid;
        spellInfo.target = targetGUID;
        spellInfo.castTime = spell->castTime;
        spellInfo.timeToCast = spellInfo.castTime;

        Util::Network::SendPacket(networkState, socketID, MetaGen::Shared::Packet::ServerSpellCastResultPacket{ .result = result });

        return true;
    }

    bool HandleOnClientPathGenerate(World* world, entt::entity entity, Network::SocketID socketID, MetaGen::Shared::Packet::ClientPathGeneratePacket& packet)
    {
        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& networkState = registry->ctx().get<Singletons::NetworkState>();

        // AV : Ally Start -> Horde Base
        //vec3 startPos = vec3(490.59f, 97.50f, -764.04f);
        //vec3 endPos = vec3(289.57f, 90.45f, 1319.79f);

        vec3 startPos = packet.start;
        vec3 endPos = packet.end;
        vec3 polyPickExt = vec3(50.0f, 100.0f, 50.0f);

        vec3 straightPath[1024];
        u32 straightPathCount = 0;
        if (!world->navmeshData.FindPath(startPos, endPos, polyPickExt, straightPath, static_cast<u32>(std::size(straightPath)), straightPathCount))
            return true;

        if (straightPathCount > 0)
        {
            const size_t packetCapacity = sizeof(Network::MessageHeader) + sizeof(u32) + static_cast<size_t>(straightPathCount) * sizeof(vec3);
            PacketWriter writer = world->packetArena.Acquire(packetCapacity);
            if (!writer.IsValid())
                return true;

            Bytebuffer& buffer = writer.GetBuffer();
            if (!ECS::Util::MessageBuilder::CreatePacket(buffer, MetaGen::Shared::Packet::ServerPathVisualizationPacket::PACKET_ID, [&, straightPathCount]() -> bool
                    {
                return buffer.PutU32(straightPathCount) && buffer.PutBytes(straightPath, straightPathCount * sizeof(vec3));
                }))
            {
                return true;
            }

            Util::Network::SendPacket(networkState, socketID, writer.Seal());
        }

        return true;
    }

    void NetworkConnection::Init(entt::registry& registry)
    {
        entt::registry::context& ctx = registry.ctx();

        Singletons::PacketArenaConfig packetArenaConfig = CreatePacketArenaConfig();
        Network::ServerConfig serverConfig = CreateServerConfig();
        auto& networkState = ctx.emplace<Singletons::NetworkState>(packetArenaConfig);
        networkState.inboundMessagesPerLanePerUpdate = static_cast<u32>(std::max(CVAR_NetworkInboundMessagesPerLanePerUpdate.Get(), 1));

        // Setup NetServer
        {
            u16 port = 4000;
            networkState.server = std::make_unique<Network::Server>(port, std::move(serverConfig), networkState.packetArenaBudget);
            networkState.messageRouter = std::make_unique<Network::MessageRouter>();

            networkState.messageRouter->SetMessageHandler(MetaGen::Shared::Packet::ClientSendCheatCommandPacket::PACKET_ID, Network::MessageHandler(Network::ConnectionStatus::Connected, 0u, -1, &HandleOnSendCheatCommand));

            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnConnect);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnAuthChallenge);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnAuthProof);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnCharacterSelect);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnCharacterLogout);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnPing);

            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientUnitTargetUpdate);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientUnitMove);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientContainerSwapSlots);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientTriggerEnter);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientSendChatMessage);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientSpellCast);
            networkState.messageRouter->RegisterPacketHandler(Network::ConnectionStatus::Connected, HandleOnClientPathGenerate);

            // Bind to IP/Port
            std::string ipAddress = "0.0.0.0";

            if (networkState.server->Start())
            {
                NC_LOG_INFO("Network : Listening on ({0}, {1})", ipAddress, port);
            }
        }
    }

    void NetworkConnection::Update(entt::registry& registry, f32 deltaTime)
    {
        entt::registry::context& ctx = registry.ctx();

        auto& gameCache = ctx.get<Singletons::GameCache>();
        auto& networkState = ctx.get<Singletons::NetworkState>();
        auto& worldState = ctx.get<Singletons::WorldState>();
        const bool logConnectionDetail = CVAR_NetworkConnectionLogDetail.Get() != 0;
        if (networkState.connectionInfoCaptureEnabled != logConnectionDetail)
        {
            networkState.server->SetConnectionInfoCapture(logConnectionDetail);
            networkState.connectionInfoCaptureEnabled = logConnectionDetail;
        }

        // Handle 'SocketConnectedEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketConnectedEvent>& connectedEvents = networkState.server->GetConnectedEvents();
            u32 connectedCount = 0;

            Network::SocketConnectedEvent connectedEvent;
            while (connectedEvents.try_dequeue(connectedEvent))
            {
                if (logConnectionDetail)
                {
                    const Network::ConnectionInfo& connectionInfo = connectedEvent.connectionInfo;
                    NC_LOG_INFO("Network : Client connected from (SocketID : {0}, \"{1}:{2}\")", static_cast<u32>(connectedEvent.socketID), connectionInfo.ipAddr, connectionInfo.port);
                }

                Util::Network::ActivateSocket(networkState, connectedEvent.socketID);
                connectedCount++;
            }

            if (!logConnectionDetail && connectedCount > 0)
                NC_LOG_INFO("Network : {} clients connected", connectedCount);
        }

        // Handle 'SocketDisconnectedEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketDisconnectedEvent>& disconnectedEvents = networkState.server->GetDisconnectedEvents();
            u32 disconnectedCount = 0;

            Network::SocketDisconnectedEvent disconnectedEvent;
            while (disconnectedEvents.try_dequeue(disconnectedEvent))
            {
                Network::SocketID socketID = disconnectedEvent.socketID;
                if (logConnectionDetail)
                    NC_LOG_INFO("Network : Client disconnected (SocketID : {0})", static_cast<u32>(socketID));

                Util::Network::DeactivateSocket(networkState, socketID);
                networkState.socketIDToSessionKeys.erase(socketID);
                disconnectedCount++;

                entt::entity characterEntity;
                if (Util::Network::GetCharacterEntity(networkState, socketID, characterEntity))
                {
                    u32 mapID;
                    if (worldState.GetMapIDFromSocket(socketID, mapID))
                    {
                        worldState.ClearMapIDForSocket(socketID);
                        World& world = worldState.GetWorld(mapID);

                        if (world.ValidEntity(characterEntity))
                        {
                            world.EmplaceOrReplace<Events::CharacterNeedsDeinitialization>(characterEntity);
                        }
                    }
                    else
                    {
                        registry.destroy(characterEntity);
                    }
                }

                entt::entity accountEntity;
                if (Util::Network::GetAccountEntity(networkState, socketID, accountEntity))
                {
                    auto& accountInfo = registry.get<Components::AccountInfo>(accountEntity);

                    Util::Network::UnlinkSocketFromAccount(networkState, socketID, accountInfo.id);
                    registry.destroy(accountEntity);
                }
            }

            if (!logConnectionDetail && disconnectedCount > 0)
                NC_LOG_INFO("Network : {} clients disconnected", disconnectedCount);
        }

        // Handle 'SocketMessageEvent'
        {
            moodycamel::ConcurrentQueue<Network::SocketMessageEvent>& messageEvents = networkState.server->GetMessageEvents(Network::DEFAULT_LANE_ID);
            Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();

            auto key = Scripting::ZenithInfoKey::MakeGlobal(0, 0);
            Scripting::Zenith* zenith = luaManager->GetZenithStateManager().Get(key);

            Network::SocketMessageEvent messageEvent;
            u32 processedMessageCount = 0;
            while (processedMessageCount < networkState.inboundMessagesPerLanePerUpdate && messageEvents.try_dequeue(messageEvent))
            {
                processedMessageCount++;
                if (!Util::Network::IsSocketActive(networkState, messageEvent.socketID))
                    continue;

                Network::MessageHeader messageHeader;
                if (networkState.messageRouter->GetMessageHeader(messageEvent.message, messageHeader))
                {
                    entt::entity accountEntity;
                    Util::Network::GetAccountEntity(networkState, messageEvent.socketID, accountEntity);

                    bool hasLuaHandlerForOpcode = zenith && Scripting::Util::Network::HasPacketHandler(zenith, messageHeader.opcode);
                    if (hasLuaHandlerForOpcode)
                    {
                        u32 messageReadOffset = static_cast<u32>(messageEvent.message.buffer->readData);
                        if (Scripting::Util::Network::CallPacketHandler(zenith, messageHeader, messageEvent.message))
                        {
                            if (!networkState.messageRouter->HasValidHandlerForHeader(messageHeader))
                                continue;

                            if (networkState.messageRouter->CallHandler(nullptr, accountEntity, messageEvent.socketID, messageHeader, messageEvent.message))
                                continue;
                        }
                    }
                    else
                    {
                        if (networkState.messageRouter->HasValidHandlerForHeader(messageHeader))
                        {
                            if (networkState.messageRouter->CallHandler(nullptr, accountEntity, messageEvent.socketID, messageHeader, messageEvent.message))
                                continue;
                        }
                    }
                }

                // Failed to Call Handler, Close Socket
                {
                    Util::Network::RequestSocketToClose(networkState, messageEvent.socketID);
                }
            }
        }

        networkState.server->Update();

        networkState.packetArenaTrimTimer -= deltaTime;
        if (networkState.packetArenaTrimTimer <= 0.0f)
        {
            networkState.packetArena.Trim(networkState.packetArenaConfig.warmBlocksPerSizeClass);
            networkState.packetArenaTrimTimer = networkState.packetArenaConfig.trimIntervalSeconds;
        }

        if (CVAR_NetworkTelemetryEnabled.Get() == 0)
        {
            networkState.telemetryTimer = 0.0f;
            return;
        }

        networkState.telemetryTimer -= deltaTime;
        if (networkState.telemetryTimer <= 0.0f)
        {
            LogNetworkTelemetry(networkState, worldState);
            networkState.telemetryTimer = static_cast<f32>(std::max(CVAR_NetworkTelemetryIntervalSeconds.Get(), 1));
        }
    }
}
