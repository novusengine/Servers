#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS
{
    namespace Singletons
    {
        struct GameCommandQueue;
    };

    namespace GameCommands
    {
        enum class GameCommandID : u16
        {
            Invalid,
            CharacterCreate,
            CharacterDelete,
            CharacterCreateItem,
            CharacterDeleteItem,
            CharacterSwapContainerSlots,
        };

        struct GameCommandBase;
        typedef std::function<void(entt::registry&, Singletons::GameCommandQueue&, GameCommandBase*)> GameCommandHandlerFn;
        typedef std::function<void(entt::registry&, GameCommandBase*, bool)> GameCommandCallback;

        struct GameCommandBase
        {
        public:
            GameCommandBase() : id(GameCommandID::Invalid) {}

        public:
            GameCommandID id;
        };
    }
}