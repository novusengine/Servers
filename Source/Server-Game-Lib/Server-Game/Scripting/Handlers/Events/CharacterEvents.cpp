#include "CharacterEvents.h"
#include "Server-Game/Scripting/Game/Character.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    namespace Event::Character
    {
        void OnCharacterLogin(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnLogin& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
        void OnCharacterLogout(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnLogout& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
        void OnCharacterWorldChanged(Zenith* zenith, MetaGen::Server::Lua::CharacterEventDataOnWorldChanged& data)
        {
            zenith->CreateTable();

            auto* character = Game::Character::Create(zenith, static_cast<entt::entity>(data.characterEntity));
            zenith->SetTableKey("character");
        }
    }
}