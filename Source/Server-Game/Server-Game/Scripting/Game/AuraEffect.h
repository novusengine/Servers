#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct AuraEffect
        {
        public:
            static void Register(Zenith* zenith);
            static AuraEffect* Create(Zenith* zenith, entt::entity entity, u8 effectIndex, u8 effectType);

        public:
            entt::entity entity;
            u8 effectIndex;
            u8 effectType;
        };

        namespace AuraEffectMethods
        {
            i32 GetIndex(Zenith* zenith, AuraEffect* auraEffect);
            i32 GetType(Zenith* zenith, AuraEffect* auraEffect);

            i32 GetValues(Zenith* zenith, AuraEffect* auraEffect);
            i32 SetValue(Zenith* zenith, AuraEffect* auraEffect);

            i32 GetMiscValues(Zenith* zenith, AuraEffect* auraEffect);
            i32 SetMiscValue(Zenith* zenith, AuraEffect* auraEffect);
        }

        static LuaRegister<AuraEffect> auraEffectMethods[] =
        {
            { "GetIndex", AuraEffectMethods::GetIndex },
            { "GetType", AuraEffectMethods::GetType },

            { "GetValues", AuraEffectMethods::GetValues },
            { "SetValue", AuraEffectMethods::SetValue },

            { "GetMiscValues", AuraEffectMethods::GetMiscValues },
            { "SetMiscValue", AuraEffectMethods::SetMiscValue  }
        };
    }
}