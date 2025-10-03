#include "AuraEffect.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/AuraEffectInfo.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void AuraEffect::Register(Zenith* zenith)
        {
            LuaMetaTable<AuraEffect>::Register(zenith, "AuraEffectMetaTable");
            LuaMetaTable<AuraEffect>::Set(zenith, auraEffectMethods);
        }

        AuraEffect* AuraEffect::Create(Zenith* zenith, entt::entity entity, u8 effectIndex, u8 effectType)
        {
            AuraEffect* auraEffect = zenith->PushUserData<AuraEffect>([](void* x) {});
            auraEffect->entity = entity;
            auraEffect->effectIndex = effectIndex;
            auraEffect->effectType = effectType;

            luaL_getmetatable(zenith->state, "AuraEffectMetaTable");
            lua_setmetatable(zenith->state, -2);

            return auraEffect;
        }

        namespace AuraEffectMethods
        {
            i32 GetIndex(Zenith* zenith, AuraEffect* auraEffect)
            {
                zenith->Push(auraEffect->effectIndex + 1);
                return 1;
            }
            i32 GetType(Zenith* zenith, AuraEffect* auraEffect)
            {
                zenith->Push(auraEffect->effectType);
                return 1;
            }

            i32 GetValues(Zenith* zenith, AuraEffect* auraEffect)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                i32 value1 = 0;
                i32 value2 = 0;
                i32 value3 = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(auraEffect->entity))
                {
                    auto& auraEffectInfo = world.Get<ECS::Components::AuraEffectInfo>(auraEffect->entity);
                    auto& effect = auraEffectInfo.effects[auraEffect->effectIndex];

                    value1 = effect.value1;
                    value2 = effect.value2;
                    value3 = effect.value3;
                }

                zenith->Push(value1);
                zenith->Push(value2);
                zenith->Push(value3);
                return 3;
            }
            i32 SetValue(Zenith* zenith, AuraEffect* auraEffect)
            {
                i32 valueIndex = zenith->CheckVal<i32>(2);
                i32 value = zenith->CheckVal<i32>(3);

                if (valueIndex < 1 || valueIndex > 3)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(auraEffect->entity))
                    return 0;

                auto& auraEffectInfo = world.Get<ECS::Components::AuraEffectInfo>(auraEffect->entity);
                auto& effect = auraEffectInfo.effects[auraEffect->effectIndex];

                ivec3* values = reinterpret_cast<ivec3*>(&effect.value1);
                (*values)[valueIndex - 1] = value;
                
                return 0;
            }

            i32 GetMiscValues(Zenith* zenith, AuraEffect* auraEffect)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                i32 miscValue1 = 0;
                i32 miscValue2 = 0;
                i32 miscValue3 = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(auraEffect->entity))
                {
                    auto& auraEffectInfo = world.Get<ECS::Components::AuraEffectInfo>(auraEffect->entity);
                    auto& effect = auraEffectInfo.effects[auraEffect->effectIndex];

                    miscValue1 = effect.miscValue1;
                    miscValue2 = effect.miscValue2;
                    miscValue3 = effect.miscValue3;
                }

                zenith->Push(miscValue1);
                zenith->Push(miscValue2);
                zenith->Push(miscValue3);
                return 3;
            }
            i32 SetMiscValue(Zenith* zenith, AuraEffect* auraEffect)
            {
                i32 miscValueIndex = zenith->CheckVal<i32>(2);
                i32 miscValue = zenith->CheckVal<i32>(3);

                if (miscValueIndex < 1 || miscValueIndex > 3)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(auraEffect->entity))
                    return 0;

                auto& auraEffectInfo = world.Get<ECS::Components::AuraEffectInfo>(auraEffect->entity);
                auto& effect = auraEffectInfo.effects[auraEffect->effectIndex];

                ivec3* miscValues = reinterpret_cast<ivec3*>(&effect.miscValue1);
                (*miscValues)[miscValueIndex - 1] = miscValue;

                return 0;
            }
        }
    }
}