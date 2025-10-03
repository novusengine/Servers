#include "SpellEffect.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Components/SpellEffectInfo.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    namespace Game
    {
        void SpellEffect::Register(Zenith* zenith)
        {
            LuaMetaTable<SpellEffect>::Register(zenith, "SpellEffectMetaTable");
            LuaMetaTable<SpellEffect>::Set(zenith, spellEffectMethods);
        }

        SpellEffect* SpellEffect::Create(Zenith* zenith, entt::entity entity, u8 effectIndex, u8 effectType)
        {
            SpellEffect* spellEffect = zenith->PushUserData<SpellEffect>([](void* x) {});
            spellEffect->entity = entity;
            spellEffect->effectIndex = effectIndex;
            spellEffect->effectType = effectType;

            luaL_getmetatable(zenith->state, "SpellEffectMetaTable");
            lua_setmetatable(zenith->state, -2);

            return spellEffect;
        }

        namespace SpellEffectMethods
        {
            i32 GetIndex(Zenith* zenith, SpellEffect* spellEffect)
            {
                zenith->Push(spellEffect->effectIndex + 1);
                return 1;
            }
            i32 GetType(Zenith* zenith, SpellEffect* spellEffect)
            {
                zenith->Push(spellEffect->effectType);
                return 1;
            }

            i32 GetValues(Zenith* zenith, SpellEffect* spellEffect)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                i32 effectValue1 = 0;
                i32 effectValue2 = 0;
                i32 effectValue3 = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(spellEffect->entity))
                {
                    auto& spellInfo = world.Get<ECS::Components::SpellEffectInfo>(spellEffect->entity);
                    auto& effect = spellInfo.effects[spellEffect->effectIndex];

                    effectValue1 = effect.effectValue1;
                    effectValue2 = effect.effectValue2;
                    effectValue3 = effect.effectValue3;
                }

                zenith->Push(effectValue1);
                zenith->Push(effectValue2);
                zenith->Push(effectValue3);
                return 3;
            }
            i32 SetValue(Zenith* zenith, SpellEffect* spellEffect)
            {
                i32 valueIndex = zenith->CheckVal<i32>(2);
                i32 value = zenith->CheckVal<i32>(3);

                if (valueIndex < 1 || valueIndex > 3)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spellEffect->entity))
                    return 0;

                auto& spellInfo = world.Get<ECS::Components::SpellEffectInfo>(spellEffect->entity);
                auto& effect = spellInfo.effects[spellEffect->effectIndex];

                ivec3* effectValues = reinterpret_cast<ivec3*>(&effect.effectValue1);
                (*effectValues)[valueIndex - 1] = value;
                
                return 0;
            }

            i32 GetMiscValues(Zenith* zenith, SpellEffect* spellEffect)
            {
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                i32 effectMiscValue1 = 0;
                i32 effectMiscValue2 = 0;
                i32 effectMiscValue3 = 0;

                ECS::World& world = worldState.GetWorld(mapID);
                if (world.ValidEntity(spellEffect->entity))
                {
                    auto& spellInfo = world.Get<ECS::Components::SpellEffectInfo>(spellEffect->entity);
                    auto& effect = spellInfo.effects[spellEffect->effectIndex];

                    effectMiscValue1 = effect.effectMiscValue1;
                    effectMiscValue2 = effect.effectMiscValue2;
                    effectMiscValue3 = effect.effectMiscValue3;
                }

                zenith->Push(effectMiscValue1);
                zenith->Push(effectMiscValue2);
                zenith->Push(effectMiscValue3);
                return 3;
            }
            i32 SetMiscValue(Zenith* zenith, SpellEffect* spellEffect)
            {
                i32 miscValueIndex = zenith->CheckVal<i32>(2);
                i32 miscValue = zenith->CheckVal<i32>(3);

                if (miscValueIndex < 1 || miscValueIndex > 3)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(spellEffect->entity))
                    return 0;

                auto& spellInfo = world.Get<ECS::Components::SpellEffectInfo>(spellEffect->entity);
                auto& effect = spellInfo.effects[spellEffect->effectIndex];

                ivec3* effectValues = reinterpret_cast<ivec3*>(&effect.effectMiscValue1);
                (*effectValues)[miscValueIndex - 1] = miscValue;

                return 0;
            }
        }
    }
}