#pragma once
#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct Item
        {
        public:
            static void Register(Zenith* zenith);
            static Item* Create(Zenith* zenith, ObjectGUID guid, u32 templateID);

        public:
            ObjectGUID guid;
            u32 templateID;
        };

        namespace ItemMethods
        {
            i32 GetID(Zenith* zenith, Item* item);
            i32 GetTemplateID(Zenith* zenith, Item* item);
            i32 GetTemplate(Zenith* zenith, Item* item);
        }

        static LuaRegister<Item> itemMethods[] =
        {
            { "GetID", ItemMethods::GetID },
            { "GetTemplateID", ItemMethods::GetTemplateID },
            { "GetTemplate", ItemMethods::GetTemplate },
        };
    }
}