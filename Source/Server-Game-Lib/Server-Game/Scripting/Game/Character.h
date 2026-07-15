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
            i32 GetReputation(Zenith* zenith, Character* character);
            i32 SetReputation(Zenith* zenith, Character* character);
            i32 ModifyReputation(Zenith* zenith, Character* character);
            i32 RemoveReputation(Zenith* zenith, Character* character);
            i32 DiscoverReputation(Zenith* zenith, Character* character);
            i32 SetReputationFlags(Zenith* zenith, Character* character);
            i32 SetReputationLocked(Zenith* zenith, Character* character);
        }

        static LuaRegister<Character> characterMethods[] = {
            { "GetEquippedItem", CharacterMethods::GetEquippedItem },
            { "GetReputation", CharacterMethods::GetReputation },
            { "SetReputation", CharacterMethods::SetReputation },
            { "ModifyReputation", CharacterMethods::ModifyReputation },
            { "RemoveReputation", CharacterMethods::RemoveReputation },
            { "DiscoverReputation", CharacterMethods::DiscoverReputation },
            { "SetReputationFlags", CharacterMethods::SetReputationFlags },
            { "SetReputationLocked", CharacterMethods::SetReputationLocked }
        };
    }
}
