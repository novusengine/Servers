#pragma once
#include <Base/Types.h>

#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/Defines.h>

namespace Scripting
{
    namespace Event::Character
    {
        void OnCharacterLogin(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnLogin& data);
        void OnCharacterLogout(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnLogout& data);
        void OnCharacterWorldChanged(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnWorldChanged& data);
    }
}