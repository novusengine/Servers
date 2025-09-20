#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct SpellEffect
        {
        public:
            static void Register(Zenith* zenith);
            static SpellEffect* Create(Zenith* zenith, entt::entity entity, u8 effectIndex, u8 effectType);

        public:
            entt::entity entity;
            u8 effectIndex;
            u8 effectType;
        };

        namespace SpellEffectMethods
        {
            i32 GetIndex(Zenith* zenith, SpellEffect* spellEffect);
            i32 GetType(Zenith* zenith, SpellEffect* spellEffect);

            i32 GetValues(Zenith* zenith, SpellEffect* spellEffect);
            i32 SetValue(Zenith* zenith, SpellEffect* spellEffect);

            i32 GetMiscValues(Zenith* zenith, SpellEffect* spellEffect);
            i32 SetMiscValue(Zenith* zenith, SpellEffect* spellEffect);
        }

        static LuaRegister<SpellEffect> spellEffectMethods[] =
        {
            { "GetIndex", SpellEffectMethods::GetIndex },
            { "GetType", SpellEffectMethods::GetType },

            { "GetValues", SpellEffectMethods::GetValues },
            { "SetValue", SpellEffectMethods::SetValue },

            { "GetMiscValues", SpellEffectMethods::GetMiscValues },
            { "SetMiscValue", SpellEffectMethods::SetMiscValue  }
        };
    }
}