#include "Unit.h"
#include "Aura.h"
#include "Character.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/UnitAuraInfo.h"
#include "Server-Game/ECS/Components/UnitSpellCooldownHistory.h"
#include "Server-Game/ECS/Components/UnitPowersComponent.h"
#include "Server-Game/ECS/Components/UnitResistancesComponent.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/VisibilityInfo.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <MetaGen/Shared/Unit/Unit.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void Unit::Register(Zenith* zenith)
        {
            LuaMetaTable<Unit>::Register(zenith, "UnitMetaTable");
            LuaMetaTable<Unit>::Set(zenith, unitMethods);
        }

        Unit* Unit::Create(Zenith* zenith, entt::entity entity)
        {
            Unit* unit = zenith->PushUserData<Unit>([](void* x) {});
            unit->entity = entity;

            luaL_getmetatable(zenith->state, "UnitMetaTable");
            lua_setmetatable(zenith->state, -2);

            return unit;
        }

        namespace UnitMethods
        {
            i32 GetID(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                u64 characterID = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(unit->entity))
                {
                    auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(unit->entity);
                    characterID = objectInfo.guid.GetCounter();
                }

                zenith->Push(characterID);
                return 1;
            }
            i32 ToCharacter(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(unit->entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(unit->entity);
                if (objectInfo.guid.GetType() != ObjectGUID::Type::Player)
                    return 0;

                Character::Create(zenith, unit->entity);
                return 1;
            }

            i32 IsAlive(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                bool isAlive = false;
                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(unit->entity))
                {
                    auto& unitPowersComponent = world.Get<ECS::Components::UnitPowersComponent>(unit->entity);
                    auto& healthPower = ECS::Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);
                    isAlive = healthPower.current > 0.0f;
                }

                zenith->Push(isAlive);
                return 1;
            }
            i32 GetHealth(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                f64 currentHealth = 0.0f;
                f64 maxHealth = 0.0f;
                f64 baseHealth = 0.0f;
                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(unit->entity))
                {
                    auto& unitPowersComponent = world.Get<ECS::Components::UnitPowersComponent>(unit->entity);
                    auto& healthPower = ECS::Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);

                    currentHealth = healthPower.current;
                    maxHealth = healthPower.max;
                    baseHealth = healthPower.base;
                }

                zenith->Push(currentHealth);
                zenith->Push(maxHealth);
                zenith->Push(baseHealth);
                return 3;
            }

            i32 Kill(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(unit->entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);

                ObjectGUID::Type objectType = objectInfo.guid.GetType();
                if (objectType != ObjectGUID::Type::Creature && objectType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                auto& unitPowersComponent = world.Get<ECS::Components::UnitPowersComponent>(unit->entity);
                auto& healthPower = ECS::Util::Unit::GetPower(unitPowersComponent, MetaGen::Shared::Unit::PowerTypeEnum::Health);

                u32 amount = static_cast<u32>(healthPower.max);
                ECS::Util::CombatEvent::AddDamageEvent(combatEventState, objectInfo.guid, objectInfo.guid, amount);

                return 0;
            }
            i32 Resurrect(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(unit->entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);

                ObjectGUID::Type objectType = objectInfo.guid.GetType();
                if (objectType != ObjectGUID::Type::Creature && objectType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                ECS::Util::CombatEvent::AddResurrectEvent(combatEventState, objectInfo.guid, objectInfo.guid);

                return 0;
            }
            i32 DealDamage(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                auto amount = zenith->CheckVal<u32>(2);
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(unit->entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);

                ObjectGUID::Type objectType = objectInfo.guid.GetType();
                if (objectType != ObjectGUID::Type::Creature && objectType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                ECS::Util::CombatEvent::AddDamageEvent(combatEventState, objectInfo.guid, objectInfo.guid, amount);

                return 0;
            }
            i32 DealDamageTo(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity srcUnitEntity = unit->entity;
                entt::entity dstUnitEntity = zenith->IsNumber(2) ? static_cast<entt::entity>(zenith->Get<u32>(1)) : entt::null;
                auto amount = zenith->CheckVal<u32>(3);

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(dstUnitEntity))
                    return 0;

                auto& srcObjectInfo = world.Get<ECS::Components::ObjectInfo>(srcUnitEntity);
                auto& targetObjectInfo = world.Get<ECS::Components::ObjectInfo>(dstUnitEntity);

                ObjectGUID::Type targetType = targetObjectInfo.guid.GetType();
                if (targetType != ObjectGUID::Type::Creature && targetType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                ECS::Util::CombatEvent::AddDamageEvent(combatEventState, srcObjectInfo.guid, targetObjectInfo.guid, amount);

                return 0;
            }
            i32 Heal(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                auto amount = zenith->CheckVal<u32>(2);
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);

                ObjectGUID::Type objectType = objectInfo.guid.GetType();
                if (objectType != ObjectGUID::Type::Creature && objectType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                ECS::Util::CombatEvent::AddHealEvent(combatEventState, objectInfo.guid, objectInfo.guid, amount);

                return 0;
            }
            i32 HealTo(Zenith* zenith, Unit* unit)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity srcUnitEntity = unit->entity;
                entt::entity dstUnitEntity = zenith->IsNumber(2) ? static_cast<entt::entity>(zenith->Get<u32>(1)) : entt::null;
                auto amount = zenith->CheckVal<u32>(3);

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(dstUnitEntity))
                    return 0;

                auto& srcObjectInfo = world.Get<ECS::Components::ObjectInfo>(srcUnitEntity);
                auto& targetObjectInfo = world.Get<ECS::Components::ObjectInfo>(dstUnitEntity);

                ObjectGUID::Type targetType = targetObjectInfo.guid.GetType();
                if (targetType != ObjectGUID::Type::Creature && targetType != ObjectGUID::Type::Player)
                    return true;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                ECS::Util::CombatEvent::AddHealEvent(combatEventState, srcObjectInfo.guid, targetObjectInfo.guid, amount);

                return 0;
            }

            i32 TeleportToLocation(Zenith* zenith, Unit* unit)
            {
                auto locationName = zenith->CheckVal<const char*>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& networkState = registry->ctx().get<ECS::Singletons::NetworkState>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                u32 locationNameLength = static_cast<u32>(strlen(locationName));
                u32 locationNameHash = StringUtils::fnv1a_32(locationName, locationNameLength);

                GameDefine::Database::MapLocation* location = nullptr;
                if (!ECS::Util::Cache::GetLocationByHash(gameCache, locationNameHash, location))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);
                auto& transform = world.Get<ECS::Components::Transform>(entity);
                auto& visibilityInfo = world.Get<ECS::Components::VisibilityInfo>(entity);

                bool result = ECS::Util::Unit::TeleportToLocation(worldState, world, gameCache, networkState, entity, objectInfo, transform, visibilityInfo, location->mapID, vec3(location->positionX, location->positionY, location->positionZ), location->orientation);
                zenith->Push(result);
                return 1;
            }
            i32 TeleportToMap(Zenith* zenith, Unit* unit)
            {
                auto mapID = zenith->CheckVal<u32>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& networkState = registry->ctx().get<ECS::Singletons::NetworkState>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                GameDefine::Database::Map* mapDefinition = nullptr;
                if (!ECS::Util::Cache::GetMapByID(gameCache, mapID, mapDefinition))
                    return 0;

                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                std::string locationName = mapDefinition->name;
                StringUtils::ToLower(locationName);
                u32 locationNameHash = StringUtils::fnv1a_32(locationName.c_str(), locationName.length());

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);
                auto& transform = world.Get<ECS::Components::Transform>(entity);
                auto& visibilityInfo = world.Get<ECS::Components::VisibilityInfo>(entity);

                vec3 position = transform.position;
                f32 orientation = transform.pitchYaw.y;

                GameDefine::Database::MapLocation* location = nullptr;
                if (ECS::Util::Cache::GetLocationByHash(gameCache, locationNameHash, location))
                {
                    position = vec3(location->positionX, location->positionY, location->positionZ);
                    orientation = location->orientation;
                }

                bool result = ECS::Util::Unit::TeleportToLocation(worldState, world, gameCache, networkState, entity, objectInfo, transform, visibilityInfo, mapID, position, orientation);
                zenith->Push(result);
                return 1;
            }
            i32 TeleportToXYZ(Zenith* zenith, Unit* unit)
            {
                auto position = zenith->CheckVal<vec3>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& networkState = registry->ctx().get<ECS::Singletons::NetworkState>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                auto& objectInfo = world.Get<ECS::Components::ObjectInfo>(entity);
                auto& transform = world.Get<ECS::Components::Transform>(entity);
                auto& visibilityInfo = world.Get<ECS::Components::VisibilityInfo>(entity);

                auto orientation = zenith->IsNumber(3) ? zenith->Get<f32>(3) : transform.pitchYaw.y;
                ECS::Util::Unit::TeleportToXYZ(world, networkState, entity, objectInfo, transform, visibilityInfo, position, orientation);

                zenith->Push(true);
                return 1;
            }
            
            i32 HasSpellCooldown(Zenith* zenith, Unit* unit)
            {
                auto spellID = zenith->CheckVal<u32>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                auto& unitSpellCooldownHistory = world.Get<ECS::Components::UnitSpellCooldownHistory>(entity);

                bool hasCooldown = ECS::Util::Unit::HasSpellCooldown(unitSpellCooldownHistory, spellID);
                zenith->Push(hasCooldown);
                return 1;
            }
            i32 GetSpellCooldown(Zenith* zenith, Unit* unit)
            {
                auto spellID = zenith->CheckVal<u32>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                auto& unitSpellCooldownHistory = world.Get<ECS::Components::UnitSpellCooldownHistory>(entity);

                f32 cooldown = ECS::Util::Unit::GetSpellCooldownRemaining(unitSpellCooldownHistory, spellID);
                zenith->Push(cooldown);
                return 1;
            }
            i32 SetSpellCooldown(Zenith* zenith, Unit* unit)
            {
                auto spellID = zenith->CheckVal<u32>(2);
                auto cooldown = zenith->CheckVal<f32>(3);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(entity))
                    return 0;

                auto& unitSpellCooldownHistory = world.Get<ECS::Components::UnitSpellCooldownHistory>(entity);
                ECS::Util::Unit::SetSpellCooldown(unitSpellCooldownHistory, spellID, cooldown);
                return 0;
            }

            i32 AddAura(Zenith* zenith, Unit* unit)
            {
                auto target = zenith->GetUserData<Unit>(2);
                auto spellID = zenith->CheckVal<u32>(3);
                auto stacks = zenith->IsNumber(4) ? zenith->CheckVal<u16>(4) : 1;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity casterEntity = unit->entity;
                entt::entity targetEntity = target ? target->entity : entt::null;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(casterEntity) || !world.ValidEntity(targetEntity))
                    return 0;

                auto& unitAuraInfo = world.Get<ECS::Components::UnitAuraInfo>(targetEntity);

                entt::entity auraEntity = entt::null;
                if (!ECS::Util::Unit::AddAura(world, gameCache, casterEntity, targetEntity, unitAuraInfo, spellID, stacks, auraEntity))
                    return 0;

                Aura::Create(zenith, auraEntity, spellID);
                return 1;
            }

            i32 RemoveAura(Zenith* zenith, Unit* unit)
            {
                return 0;
            }

            i32 HasAura(Zenith* zenith, Unit* unit)
            {
                return 0;
            }

            i32 GetAura(Zenith* zenith, Unit* unit)
            {
                return 0;
            }
            i32 GetPower(Zenith* zenith, Unit* unit)
            {
                auto resourceType = static_cast<MetaGen::Shared::Unit::PowerTypeEnum>(zenith->CheckVal<u8>(2));
                if (resourceType <= MetaGen::Shared::Unit::PowerTypeEnum::Invalid || resourceType >= MetaGen::Shared::Unit::PowerTypeEnum::Count)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(entity))
                    return 0;

                auto& unitPowersComponent = world.Get<ECS::Components::UnitPowersComponent>(entity);
                ECS::UnitPower* power = ECS::Util::Unit::TryGetPower(unitPowersComponent, resourceType);
                if (!power)
                    return 0;

                zenith->Push(power->current);
                zenith->Push(power->max);
                zenith->Push(power->base);

                return 3;
            }
            i32 GetStat(Zenith* zenith, Unit* unit)
            {
                auto statType = static_cast<MetaGen::Shared::Unit::StatTypeEnum>(zenith->CheckVal<u8>(2));
                if (statType <= MetaGen::Shared::Unit::StatTypeEnum::Invalid || statType >= MetaGen::Shared::Unit::StatTypeEnum::Count)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = unit->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(entity))
                    return 0;

                auto& unitStatsComponent = world.Get<ECS::Components::UnitStatsComponent>(entity);
                ECS::UnitStat& stat = ECS::Util::Unit::GetStat(unitStatsComponent, statType);

                zenith->Push(stat.current);
                zenith->Push(stat.base);

                return 3;
            }
        }
    }
}