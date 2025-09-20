#pragma once
#include <Base/Types.h>

#include <Meta/Generated/Server/LuaEvent.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Character
    {
        void OnCharacterLogin(Zenith* zenith, Generated::LuaCharacterEventDataOnLogin& data);
        void OnCharacterLogout(Zenith* zenith, Generated::LuaCharacterEventDataOnLogout& data);
        void OnCharacterWorldChanged(Zenith* zenith, Generated::LuaCharacterEventDataOnWorldChanged& data);
    }
}