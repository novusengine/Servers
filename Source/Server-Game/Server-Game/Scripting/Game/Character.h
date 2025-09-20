#pragma once
#include "Unit.h"

#include <Base/Types.h>

#include <Scripting/Defines.h>
#include <Scripting/LuaMethodTable.h>

#include <entt/fwd.hpp>

namespace Scripting
{
    namespace Game
    {
        struct Character : Unit
        {
        public:
            static void Register(Zenith* zenith);
            static Character* Create(Zenith* zenith, entt::entity entity);
        };

        namespace CharacterMethods
        {
            i32 GetEquippedItem(Zenith* zenith, Character* character);
        }

        static LuaRegister<Character> characterMethods[] =
        {
            { "GetEquippedItem", CharacterMethods::GetEquippedItem }
        };
    }
}