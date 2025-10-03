#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct Aura
        {
        public:
            static void Register(Zenith* zenith);
            static Aura* Create(Zenith* zenith, entt::entity entity, u32 spellID);

        public:
            entt::entity entity;
            u32 spellID;
        };

        namespace AuraMethods
        {
            i32 GetID(Zenith* zenith, Aura* aura);
            i32 GetEffect(Zenith* zenith, Aura* aura);

            i32 GetCaster(Zenith* zenith, Aura* aura);
            i32 GetTarget(Zenith* zenith, Aura* aura);
            i32 GetStacks(Zenith* zenith, Aura* aura);
            i32 GetDuration(Zenith* zenith, Aura* aura);
            i32 GetTimeRemaining(Zenith* zenith, Aura* aura);

            i32 Damage(Zenith* zenith, Aura* aura);
            i32 Heal(Zenith* zenith, Aura* aura);
        }

        static LuaRegister<Aura> auraMethods[] =
        {
            { "GetID", AuraMethods::GetID },
            { "GetEffect", AuraMethods::GetEffect },

            { "GetCaster", AuraMethods::GetCaster },
            { "GetTarget", AuraMethods::GetTarget },
            { "GetStacks", AuraMethods::GetStacks },
            { "GetDuration", AuraMethods::GetDuration },
            { "GetTimeRemaining", AuraMethods::GetTimeRemaining },

            { "Damage", AuraMethods::Damage },
            { "Heal", AuraMethods::Heal }
        };
    }
}