#include "Spell.h"
#include "SpellEffect.h"
#include "Unit.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/ObjectInfo.h"
#include "Server-Game/ECS/Components/Transform.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Components/SpellInfo.h"
#include "Server-Game/ECS/Components/UnitCombatInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/ECS/Util/Cache/CacheUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void Spell::Register(Zenith* zenith)
        {
            LuaMetaTable<Spell>::Register(zenith, "SpellMetaTable");
            LuaMetaTable<Spell>::Set(zenith, spellMethods);
        }

        Spell* Spell::Create(Zenith* zenith, entt::entity entity, u32 spellID)
        {
            Spell* spell = zenith->PushUserData<Spell>([](void* x) {});
            spell->entity = entity;
            spell->spellID = spellID;

            luaL_getmetatable(zenith->state, "SpellMetaTable");
            lua_setmetatable(zenith->state, -2);

            return spell;
        }

        namespace SpellMethods
        {
            i32 GetID(Zenith* zenith, Spell* spell)
            {
                zenith->Push(spell->spellID);
                return 1;
            }

            i32 GetEffect(Zenith* zenith, Spell* spell)
            {
                u8 effectIndex = zenith->CheckVal<u8>(2) - 1;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spell->entity))
                    return 0;

                auto& spellEffectInfo = world.Get<ECS::Components::SpellEffectInfo>(spell->entity);

                u32 numEffects = static_cast<u32>(spellEffectInfo.effects.size());
                if (effectIndex >= numEffects)
                    return 0;

                auto& spellEffect = spellEffectInfo.effects[effectIndex];
                SpellEffect::Create(zenith, spell->entity, effectIndex, spellEffect.effectType);
                return 1;
            }

            i32 GetCaster(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spell->entity))
                    return 0;

                auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);

                entt::entity casterEntity = world.GetEntity(spellInfo.caster);
                if (!world.ValidEntity(casterEntity))
                    return 0;

                Unit::Create(zenith, casterEntity);
                return 1;
            }
            i32 GetTarget(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spell->entity))
                    return 0;

                auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);

                entt::entity targetEntity = world.GetEntity(spellInfo.target);
                if (!world.ValidEntity(targetEntity))
                    return 0;

                Unit::Create(zenith, targetEntity);
                return 1;
            }

            i32 GetCastTime(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                f32 castTime = 0.0f;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(spell->entity))
                {
                    auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);
                    castTime = spellInfo.castTime;
                }

                zenith->Push(castTime);
                return 1;
            }

            i32 GetTimeToCast(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                f32 timeToCast = 0.0f;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(spell->entity))
                {
                    auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);
                    timeToCast = spellInfo.timeToCast;
                }

                zenith->Push(timeToCast);
                return 1;
            }

            i32 Damage(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u32 amount = zenith->CheckVal<u32>(2);

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spell->entity))
                    return 0;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);

                ECS::Util::CombatEvent::AddDamageEvent(combatEventState, spellInfo.caster, spellInfo.target, amount, 0u, 0u, 0u, spellInfo.spellID);
                return 0;
            }
            i32 Heal(Zenith* zenith, Spell* spell)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u32 amount = zenith->CheckVal<u32>(2);

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spell->entity))
                    return 0;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                auto& spellInfo = world.Get<ECS::Components::SpellInfo>(spell->entity);

                ECS::Util::CombatEvent::AddHealEvent(combatEventState, spellInfo.caster, spellInfo.target, amount, 0u, spellInfo.spellID);
                return 0;
            }
        }
    }
}