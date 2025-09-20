#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct ItemWeaponTemplate
        {
        public:
            static void Register(Zenith* zenith);
            static ItemWeaponTemplate* Create(Zenith* zenith, u32 id);

        public:
            u32 id;
            GameDefine::Database::ItemWeaponTemplate* definition = nullptr;
        };
        namespace ItemWeaponTemplateMethods
        {
            i32 GetID(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate);
            i32 GetWeaponStyle(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate);
            i32 GetDamageRange(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate);
            i32 GetSpeed(Zenith* zenith, ItemWeaponTemplate* itemWeaponTemplate);
        }

        static LuaRegister<ItemWeaponTemplate> itemWeaponTemplateMethods[] =
        {
            { "GetID", ItemWeaponTemplateMethods::GetID },
            { "GetWeaponStyle", ItemWeaponTemplateMethods::GetWeaponStyle },
            { "GetDamageRange", ItemWeaponTemplateMethods::GetDamageRange },
            { "GetSpeed", ItemWeaponTemplateMethods::GetSpeed }
        };
    }
}