#include "Aura.h"
#include "AuraEffect.h"
#include "Unit.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/AuraInfo.h"
#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Singletons/CombatEventState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/CombatEventUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void Aura::Register(Zenith* zenith)
        {
            LuaMetaTable<Aura>::Register(zenith, "AuraMetaTable");
            LuaMetaTable<Aura>::Set(zenith, auraMethods);
        }

        Aura* Aura::Create(Zenith* zenith, entt::entity entity, u32 spellID)
        {
            Aura* aura = zenith->PushUserData<Aura>([](void* x) {});
            aura->entity = entity;
            aura->spellID = spellID;

            luaL_getmetatable(zenith->state, "AuraMetaTable");
            lua_setmetatable(zenith->state, -2);

            return aura;
        }

        namespace AuraMethods
        {
            i32 GetID(Zenith* zenith, Aura* aura)
            {
                zenith->Push(aura->spellID);
                return 1;
            }

            i32 GetEffect(Zenith* zenith, Aura* aura)
            {
                u8 effectIndex = zenith->CheckVal<u8>(2) - 1;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(aura->entity))
                    return 0;

                auto& auraEffectInfo = world.Get<ECS::Components::AuraEffectInfo>(aura->entity);

                u32 numEffects = static_cast<u32>(auraEffectInfo.effects.size());
                if (effectIndex >= numEffects)
                    return 0;

                auto& auraEffect = auraEffectInfo.effects[effectIndex];
                AuraEffect::Create(zenith, aura->entity, effectIndex, auraEffect.type);
                return 1;
            }

            i32 GetCaster(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(aura->entity))
                    return 0;

                auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);

                entt::entity casterEntity = world.GetEntity(auraInfo.caster);
                if (!world.ValidEntity(casterEntity))
                    return 0;

                Unit::Create(zenith, casterEntity);
                return 1;
            }
            i32 GetTarget(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(aura->entity))
                    return 0;

                auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);

                entt::entity targetEntity = world.GetEntity(auraInfo.target);
                if (!world.ValidEntity(targetEntity))
                    return 0;

                Unit::Create(zenith, targetEntity);
                return 1;
            }

            i32 GetStacks(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                u16 stacks = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(aura->entity))
                {
                    auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);
                    stacks = auraInfo.stacks;
                }

                zenith->Push(stacks);
                return 1;
            }

            i32 GetDuration(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                f32 duration = 0.0f;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(aura->entity))
                {
                    auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);
                    duration = auraInfo.duration;
                }

                zenith->Push(duration);
                return 1;
            }

            i32 GetTimeRemaining(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                f32 timeRemaining = 0.0f;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(aura->entity))
                {
                    auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);
                    timeRemaining = auraInfo.timeRemaining;
                }

                zenith->Push(timeRemaining);
                return 1;
            }

            i32 Damage(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u32 amount = zenith->CheckVal<u32>(2);

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(aura->entity))
                    return 0;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);

                ECS::Util::CombatEvent::AddDamageEvent(combatEventState, auraInfo.caster, auraInfo.target, amount, 0u, 0u, 0u, auraInfo.spellID);
                return 0;
            }
            i32 Heal(Zenith* zenith, Aura* aura)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u32 amount = zenith->CheckVal<u32>(2);

                u16 mapID = zenith->key.GetMapID();
                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(aura->entity))
                    return 0;

                auto& combatEventState = world.GetSingleton<ECS::Singletons::CombatEventState>();
                auto& auraInfo = world.Get<ECS::Components::AuraInfo>(aura->entity);

                ECS::Util::CombatEvent::AddHealEvent(combatEventState, auraInfo.caster, auraInfo.target, amount, 0u, auraInfo.spellID);
                return 0;
            }
        }
    }
}