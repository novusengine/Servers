#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct Unit
        {
        public:
            static void Register(Zenith* zenith);
            static Unit* Create(Zenith* zenith, entt::entity entity);

        public:
            entt::entity entity;
        };

        namespace UnitMethods
        {
            i32 GetID(Zenith* zenith, Unit* unit);
            i32 ToCharacter(Zenith* zenith, Unit* unit);

            i32 IsAlive(Zenith* zenith, Unit* unit);
            i32 GetHealth(Zenith* zenith, Unit* unit);

            i32 Kill(Zenith* zenith, Unit* unit);
            i32 Resurrect(Zenith* zenith, Unit* unit);
            i32 DealDamage(Zenith* zenith, Unit* unit);
            i32 DealDamageTo(Zenith* zenith, Unit* unit);
            i32 Heal(Zenith* zenith, Unit* unit);
            i32 HealTo(Zenith* zenith, Unit* unit);

            i32 TeleportToLocation(Zenith* zenith, Unit* unit);
            i32 TeleportToMap(Zenith* zenith, Unit* unit);
            i32 TeleportToXYZ(Zenith* zenith, Unit* unit);

            i32 HasSpellCooldown(Zenith* zenith, Unit* unit);
            i32 GetSpellCooldown(Zenith* zenith, Unit* unit);
            i32 SetSpellCooldown(Zenith* zenith, Unit* unit);

            i32 AddAura(Zenith* zenith, Unit* unit);
            i32 RemoveAura(Zenith* zenith, Unit* unit);
            i32 HasAura(Zenith* zenith, Unit* unit);
            i32 GetAura(Zenith* zenith, Unit* unit);

            i32 GetPower(Zenith* zenith, Unit* unit);
            i32 GetStat(Zenith* zenith, Unit* unit);
        }

        static LuaRegister<Unit> unitMethods[] =
        {
            { "GetID", UnitMethods::GetID },
            { "ToCharacter", UnitMethods::ToCharacter },

            { "IsAlive", UnitMethods::IsAlive },
            { "GetHealth", UnitMethods::GetHealth },

            { "Kill", UnitMethods::Kill },
            { "Resurrect", UnitMethods::Resurrect },
            { "DealDamage", UnitMethods::DealDamage },
            { "DealDamageTo", UnitMethods::DealDamageTo },
            { "Heal", UnitMethods::Heal },
            { "HealTo", UnitMethods::HealTo },

            { "TeleportToLocation", UnitMethods::TeleportToLocation },
            { "TeleportToMap", UnitMethods::TeleportToMap },
            { "TeleportToXYZ", UnitMethods::TeleportToXYZ },

            { "HasSpellCooldown", UnitMethods::HasSpellCooldown },
            { "GetSpellCooldown", UnitMethods::GetSpellCooldown },
            { "SetSpellCooldown", UnitMethods::SetSpellCooldown },

            { "AddAura", UnitMethods::AddAura },
            { "RemoveAura", UnitMethods::RemoveAura },
            { "HasAura", UnitMethods::HasAura },
            { "GetAura", UnitMethods::GetAura },

            { "GetPower", UnitMethods::GetPower },
            { "GetStat", UnitMethods::GetStat }
        };
    }
}