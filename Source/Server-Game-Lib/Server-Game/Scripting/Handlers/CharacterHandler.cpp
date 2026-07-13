#include "CharacterHandler.h"
#include "Server-Game/Scripting/Game/Character.h"

#include <Scripting/Zenith.h>

namespace Scripting
{
    void CharacterHandler::Register(Zenith* zenith)
    {
        Game::Character::Register(zenith);
    }

    void CharacterHandler::PostLoad(Zenith* zenith)
    {
    }
}
