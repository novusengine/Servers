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
        struct ItemTemplate
        {
        public:
            static void Register(Zenith* zenith);
            static ItemTemplate* Create(Zenith* zenith, u32 id);

        public:
            u32 id;
            GameDefine::Database::ItemTemplate* definition = nullptr;
        };
        namespace ItemTemplateMethods
        {
            i32 GetID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetDisplayID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetBind(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetRarity(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetCategory(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetType(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetVirtualLevel(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetRequiredLevel(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetDurability(Zenith* zenith, ItemTemplate* itemTemplate);

            i32 GetIconID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetName(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetDescription(Zenith* zenith, ItemTemplate* itemTemplate);

            i32 GetArmor(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetStatTemplateID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetArmorTemplateID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetWeaponTemplateID(Zenith* zenith, ItemTemplate* itemTemplate);
            i32 GetShieldTemplateID(Zenith* zenith, ItemTemplate* itemTemplate);

            i32 GetWeaponTemplate(Zenith* zenith, ItemTemplate* itemTemplate);
        }

        static LuaRegister<ItemTemplate> itemTemplateMethods[] =
        {
            { "GetID", ItemTemplateMethods::GetID },
            { "GetDisplayID", ItemTemplateMethods::GetDisplayID },
            { "GetBind", ItemTemplateMethods::GetBind },
            { "GetRarity", ItemTemplateMethods::GetRarity },
            { "GetCategory", ItemTemplateMethods::GetCategory },
            { "GetType", ItemTemplateMethods::GetType },
            { "GetVirtualLevel", ItemTemplateMethods::GetVirtualLevel },
            { "GetRequiredLevel", ItemTemplateMethods::GetRequiredLevel },
            { "GetDurability", ItemTemplateMethods::GetDurability },

            { "GetIconID", ItemTemplateMethods::GetIconID },
            { "GetName", ItemTemplateMethods::GetName },
            { "GetDescription", ItemTemplateMethods::GetDescription },

            { "GetArmor", ItemTemplateMethods::GetArmor },
            { "GetStatTemplateID", ItemTemplateMethods::GetStatTemplateID },
            { "GetArmorTemplateID", ItemTemplateMethods::GetArmorTemplateID },
            { "GetWeaponTemplateID", ItemTemplateMethods::GetWeaponTemplateID },
            { "GetShieldTemplateID", ItemTemplateMethods::GetShieldTemplateID },

            { "GetWeaponTemplate", ItemTemplateMethods::GetWeaponTemplate }
        };
    }
}