#pragma once
#include "GameCommandBase.h"

#include <entt/fwd.hpp>

#include <string>

namespace ECS
{
    namespace Singletons
    {
        struct GameCommandQueue;
    };

    // Struct Defines
    namespace GameCommands::Character
    {
        struct CharacterCreate : public GameCommandBase
        {
        public:
            static CharacterCreate* Create(const std::string& name, u16 racegenderclass, GameCommandCallback&& callback = nullptr)
            {
                CharacterCreate* command = new CharacterCreate();
                command->id = GameCommandID::CharacterCreate;
                command->name = name;
                command->racegenderclass = racegenderclass;
                command->callback = callback;

                return command;
            }

        private:
            CharacterCreate() = default;

        public:
            std::string name;
            u16 racegenderclass;
            GameCommandCallback callback;
        };

        struct CharacterDelete : public GameCommandBase
        {
        public:
            static CharacterDelete* Create(u64 characterID, GameCommandCallback&& callback = nullptr)
            {
                CharacterDelete* command = new CharacterDelete();
                command->id = GameCommandID::CharacterDelete;
                command->characterID = characterID;
                command->callback = callback;

                return command;
            }

        private:
            CharacterDelete() = default;

        public:
            u64 characterID;
            GameCommandCallback callback;
        };

        typedef std::function<void(entt::registry&, GameCommandBase*, bool, u64)> GameCommandCharacterCreateItemCallback;
        struct CharacterCreateItem : public GameCommandBase
        {
        public:
            static CharacterCreateItem* Create(u64 characterID, u32 itemID, u16 count, u64 containerID, u8 slot, GameCommandCharacterCreateItemCallback&& callback = nullptr)
            {
                CharacterCreateItem* command = new CharacterCreateItem();
                command->id = GameCommandID::CharacterCreateItem;
                command->characterID = characterID;
                command->itemID = itemID;
                command->count = count;
                command->containerID = containerID;
                command->slot = slot;
                command->callback = callback;

                return command;
            }

        private:
            CharacterCreateItem() = default;

        public:
            u64 characterID;
            u32 itemID;
            u16 count;
            u64 containerID;
            u8 slot;
            GameCommandCharacterCreateItemCallback callback;
        };

        typedef std::function<void(entt::registry&, GameCommandBase*, bool, u64)> GameCommandCharacterDeleteItemCallback;
        struct CharacterDeleteItem : public GameCommandBase
        {
        public:
            static CharacterDeleteItem* Create(u64 characterID, u32 itemID, u16 count, u64 containerID, u8 slot, GameCommandCharacterDeleteItemCallback&& callback = nullptr)
            {
                CharacterDeleteItem* command = new CharacterDeleteItem();
                command->id = GameCommandID::CharacterDeleteItem;
                command->characterID = characterID;
                command->itemID = itemID;
                command->count = count;
                command->containerID = containerID;
                command->slot = slot;
                command->callback = callback;

                return command;
            }

        private:
            CharacterDeleteItem() = default;

        public:
            u64 characterID;
            u32 itemID;
            u16 count;
            u64 containerID;
            u8 slot;
            GameCommandCharacterDeleteItemCallback callback;
        };

        struct CharacterSwapContainerSlots : public GameCommandBase
        {
        public:
            static CharacterSwapContainerSlots* Create(u64 characterID, u8 srcContainerIndex, u8 destContainerIndex, u8 srcSlotIndex, u8 destSlotIndex, GameCommandCallback&& callback = nullptr)
            {
                CharacterSwapContainerSlots* command = new CharacterSwapContainerSlots();
                command->id = GameCommandID::CharacterSwapContainerSlots;
                command->characterID = characterID;
                command->srcContainerIndex = srcContainerIndex;
                command->destContainerIndex = destContainerIndex;
                command->srcSlotIndex = srcSlotIndex;
                command->destSlotIndex = destSlotIndex;
                command->callback = callback;

                return command;
            }

        private:
            CharacterSwapContainerSlots() = default;

        public:
            u64 characterID;
            u8 srcContainerIndex;
            u8 destContainerIndex;
            u8 srcSlotIndex;
            u8 destSlotIndex;
            GameCommandCallback callback;
        };
    }

    // Handler Defines
    namespace GameCommands::Character
    {
        void InstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue);
        void UninstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue);

        void CharacterCreateHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase);
        void CharacterDeleteHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase);
        void CharacterCreateItemHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase);
        void CharacterDeleteItemHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase);
        void CharacterSwapContainerSlotsHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase);
    }
}