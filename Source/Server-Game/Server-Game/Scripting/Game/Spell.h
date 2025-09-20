#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct Spell
        {
        public:
            static void Register(Zenith* zenith);
            static Spell* Create(Zenith* zenith, entt::entity entity, u32 spellID);

        public:
            entt::entity entity;
            u32 spellID;
        };

        namespace SpellMethods
        {
            i32 GetID(Zenith* zenith, Spell* spell);
            i32 GetEffect(Zenith* zenith, Spell* spell);

            i32 GetCaster(Zenith* zenith, Spell* spell);
            i32 GetTarget(Zenith* zenith, Spell* spell);

            i32 GetCastTime(Zenith* zenith, Spell* spell);
            i32 GetTimeToCast(Zenith* zenith, Spell* spell);

            i32 Damage(Zenith* zenith, Spell* spell);
            i32 Heal(Zenith* zenith, Spell* spell);
        }

        static LuaRegister<Spell> spellMethods[] =
        {
            { "GetID", SpellMethods::GetID },
            { "GetEffect", SpellMethods::GetEffect },

            { "GetCaster", SpellMethods::GetCaster },
            { "GetTarget", SpellMethods::GetTarget },

            { "GetCastTime", SpellMethods::GetCastTime },
            { "GetTimeToCast", SpellMethods::GetTimeToCast },

            { "Damage", SpellMethods::Damage },
            { "Heal", SpellMethods::Heal }
        };
    }
}