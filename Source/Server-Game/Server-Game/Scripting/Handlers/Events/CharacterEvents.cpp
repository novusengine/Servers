#include "CharacterEvents.h"
#include "Server-Game/Scripting/Game/Character.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    namespace Event::Character
    {
        void OnCharacterLogin(Zenith* zenith, Generated::LuaCharacterEventDataOnLogin& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
        void OnCharacterLogout(Zenith* zenith, Generated::LuaCharacterEventDataOnLogout& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
        void OnCharacterWorldChanged(Zenith* zenith, Generated::LuaCharacterEventDataOnWorldChanged& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
    }
}