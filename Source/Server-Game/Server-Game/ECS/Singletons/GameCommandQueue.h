#pragma once
#include "Server-Game/ECS/GameCommands/GameCommandBase.h"

#include <Base/Types.h>

#include <robinhood/robinhood.h>

#include <functional>
#include <mutex>
#include <queue>

namespace ECS
{
    namespace Singletons
    {
        struct GameCommandQueue
        {
        public:
            void Push(GameCommands::GameCommandBase* gameCommandBase)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _queue.push(gameCommandBase);
            }

            bool Pop(GameCommands::GameCommandBase*& command)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_queue.empty())
                    return false;

                command = _queue.front();
                _queue.pop();
                return true;
            }

            void RegisterHandler(GameCommands::GameCommandID id, GameCommands::GameCommandHandlerFn handler)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _handlers[id] = handler;
            }

            void UnregisterHandler(GameCommands::GameCommandID id)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _handlers.erase(id);
            }

            void CallHandler(entt::registry& registry, GameCommands::GameCommandBase* gameCommandBase)
            {
                GameCommands::GameCommandHandlerFn* handler = nullptr;

                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    if (!gameCommandBase || !_handlers.contains(gameCommandBase->id))
                        return;

                    handler = &_handlers[gameCommandBase->id];
                }

                handler->operator()(registry, *this, gameCommandBase);
            }

            u32 Size()
            {
                std::lock_guard<std::mutex> lock(_mutex);
                return static_cast<u32>(_queue.size());
            }

        private:
            robin_hood::unordered_map<GameCommands::GameCommandID, GameCommands::GameCommandHandlerFn> _handlers;
            std::queue<GameCommands::GameCommandBase*> _queue;
            std::mutex _mutex;
        };
    }
}