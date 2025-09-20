#include "UnitHandler.h"
#include "Server-Game/Scripting/Game/Unit.h"

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>

namespace Scripting
{
    void UnitHandler::Register(Zenith* zenith)
    {
        Game::Unit::Register(zenith);
    }

    void UnitHandler::PostLoad(Zenith* zenith)
    {
    }
}
