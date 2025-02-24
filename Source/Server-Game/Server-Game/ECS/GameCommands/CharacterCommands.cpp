#include "CharacterCommands.h"
#include "Server-Game/ECS/Singletons/DatabaseState.h"
#include "Server-Game/ECS/Singletons/GameCommandQueue.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"

#include <Server-Common/Database/DBController.h>
#include <Server-Common/Database/Util/CharacterUtils.h>
#include <Server-Common/Database/Util/ItemUtils.h>

#include <Base/Util/StringUtils.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS
{
    namespace GameCommands::Character
    {
        void InstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue)
        {
            auto& gameCommandQueue = registry.ctx().get<Singletons::GameCommandQueue>();

            gameCommandQueue.RegisterHandler(GameCommandID::CharacterCreate, CharacterCreateHandler);
            gameCommandQueue.RegisterHandler(GameCommandID::CharacterDelete, CharacterDeleteHandler);
            gameCommandQueue.RegisterHandler(GameCommandID::CharacterCreateItem, CharacterCreateItemHandler);
            gameCommandQueue.RegisterHandler(GameCommandID::CharacterDeleteItem, CharacterDeleteItemHandler);
            gameCommandQueue.RegisterHandler(GameCommandID::CharacterSwapContainerSlots, CharacterSwapContainerSlotsHandler);
        }

        void UninstallHandlers(entt::registry& registry, Singletons::GameCommandQueue& commandQueue)
        {
            auto& gameCommandQueue = registry.ctx().get<Singletons::GameCommandQueue>();

            gameCommandQueue.UnregisterHandler(GameCommandID::CharacterCreate);
            gameCommandQueue.UnregisterHandler(GameCommandID::CharacterDelete);
            gameCommandQueue.UnregisterHandler(GameCommandID::CharacterCreateItem);
            gameCommandQueue.UnregisterHandler(GameCommandID::CharacterDeleteItem);
            gameCommandQueue.UnregisterHandler(GameCommandID::CharacterSwapContainerSlots);
        }

        void CharacterCreateHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase)
        {
            CharacterCreate* characterCreate = static_cast<CharacterCreate*>(gameCommandBase);
            if (!characterCreate)
                return;

            Singletons::DatabaseState& databaseState = registry.ctx().get<Singletons::DatabaseState>();

            bool result = false;
            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                auto transaction = conn->NewTransaction();

                u64 characterID = std::numeric_limits<u64>().max();
                if (Database::Util::Character::CharacterCreate(transaction, characterCreate->name, characterCreate->racegenderclass, characterID))
                {
                    u32 charNameHash = StringUtils::fnv1a_32(characterCreate->name.c_str(), characterCreate->name.length());
                    u16 raceGenderClass = characterCreate->racegenderclass;

                    databaseState.characterTables.charIDToDefinition[characterID] = { .id = characterID, .name = std::move(characterCreate->name), .raceGenderClass = 1 };
                    databaseState.characterTables.charNameHashToCharID[charNameHash] = characterID;
                    databaseState.characterTables.charIDToBaseContainer[characterID] = Database::Container(Database::CHARACTER_BASE_CONTAINER_SIZE);

                    static constexpr u32 defaultBagItemID = 21;
                    static constexpr u32 defaultContainerID = 0;
                    static constexpr u32 defaultBagSlot = 19;

                    auto* createItemCommand = CharacterCreateItem::Create(characterID, defaultBagItemID, 1, defaultContainerID, defaultBagSlot);
                    commandQueue.Push(createItemCommand);

                    result = true;
                    transaction.commit();
                }
                else
                {
                    transaction.abort();
                }
            }

            if (characterCreate->callback)
                characterCreate->callback(registry, gameCommandBase, result);
        }
        void CharacterDeleteHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase)
        {
            CharacterDelete* characterDelete = static_cast<CharacterDelete*>(gameCommandBase);
            if (!characterDelete)
                return;

            auto& databaseState = registry.ctx().get<Singletons::DatabaseState>();
            auto& networkState = registry.ctx().get<Singletons::NetworkState>();

            bool result = false;
            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                if (networkState.characterIDToSocketID.contains(characterDelete->characterID))
                {
                    Network::SocketID existingSocketID = networkState.characterIDToSocketID[characterDelete->characterID];
                    networkState.server->CloseSocketID(existingSocketID);
                }

                auto transaction = conn->NewTransaction();
                if (Database::Util::Character::CharacterDelete(transaction, characterDelete->characterID))
                {
                    auto characterDefinition = databaseState.characterTables.charIDToDefinition[characterDelete->characterID];
                    u32 charNameHash = StringUtils::fnv1a_32(characterDefinition.name.c_str(), characterDefinition.name.length());

                    databaseState.characterTables.charIDToDefinition.erase(characterDefinition.id);
                    databaseState.characterTables.charNameHashToCharID.erase(charNameHash);
                    databaseState.characterTables.charIDToPermissions.erase(characterDefinition.id);
                    databaseState.characterTables.charIDToPermissionGroups.erase(characterDefinition.id);
                    databaseState.characterTables.charIDToCurrency.erase(characterDefinition.id);
                    databaseState.characterTables.charIDToBaseContainer.erase(characterDefinition.id);

                    result = true;
                    transaction.commit();
                }
                else
                {
                    transaction.abort();
                }
            }

            if (characterDelete->callback)
                characterDelete->callback(registry, gameCommandBase, result);
        }

        void CharacterCreateItemHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase)
        {
            CharacterCreateItem* characterCreateItem = static_cast<CharacterCreateItem*>(gameCommandBase);
            if (!characterCreateItem)
                return;

            Singletons::DatabaseState& databaseState = registry.ctx().get<Singletons::DatabaseState>();

            bool result = false;
            u64 itemInstanceID = std::numeric_limits<u64>().max();
            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                if (databaseState.itemTables.templateIDToTemplateDefinition.contains(characterCreateItem->itemID))
                {
                    auto transaction = conn->NewTransaction();
                    const auto& itemTemplateDefinition = databaseState.itemTables.templateIDToTemplateDefinition[characterCreateItem->itemID];

                    if (Database::Util::Item::ItemInstanceCreate(transaction, characterCreateItem->itemID, characterCreateItem->characterID, characterCreateItem->count, itemTemplateDefinition.durability, itemInstanceID))
                    {
                        if (Database::Util::Character::CharacterAddItem(transaction,  characterCreateItem->characterID, itemInstanceID, characterCreateItem->containerID, characterCreateItem->slot))
                        {
                            u64 containerID = characterCreateItem->containerID;
                            if (containerID == 0)
                            {
                                Database::Container& itemContainer = databaseState.characterTables.charIDToBaseContainer[characterCreateItem->characterID];

                                GameDefine::ObjectGuid itemObjectGuid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Item, itemInstanceID);
                                itemContainer.AddItemToSlot(itemObjectGuid, (u8)characterCreateItem->slot);
                            }
                            else
                            {
                                Database::Container& itemContainer = databaseState.itemTables.itemInstanceIDToContainer[containerID];

                                GameDefine::ObjectGuid itemObjectGuid = GameDefine::ObjectGuid(GameDefine::ObjectGuid::Type::Item, itemInstanceID);
                                itemContainer.AddItemToSlot(itemObjectGuid, (u8)characterCreateItem->slot);
                            }

                            auto& itemInstance = databaseState.itemTables.itemInstanceIDToDefinition[itemInstanceID];
                            itemInstance.id = itemInstanceID;
                            itemInstance.ownerID = characterCreateItem->characterID;
                            itemInstance.itemID = characterCreateItem->itemID;
                            itemInstance.count = characterCreateItem->count;
                            itemInstance.durability = itemTemplateDefinition.durability;

                            result = true;
                            transaction.commit();
                        }
                    }

                    if (!result)
                    {
                        transaction.abort();
                    }
                }
            }

            if (characterCreateItem->callback)
                characterCreateItem->callback(registry, gameCommandBase, result, itemInstanceID);
        }

        void CharacterDeleteItemHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase)
        {
            CharacterDeleteItem* characterDeleteItem = static_cast<CharacterDeleteItem*>(gameCommandBase);
            if (!characterDeleteItem)
                return;

            Singletons::DatabaseState& databaseState = registry.ctx().get<Singletons::DatabaseState>();

            bool result = false;
            u64 itemInstanceID = std::numeric_limits<u64>().max();
            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                if (databaseState.itemTables.templateIDToTemplateDefinition.contains(characterDeleteItem->itemID))
                {
                    const auto& itemTemplateDefinition = databaseState.itemTables.templateIDToTemplateDefinition[characterDeleteItem->itemID];

                    u64 containerID = characterDeleteItem->containerID;
                    if (containerID == 0)
                    {
                        Database::Container& itemContainer = databaseState.characterTables.charIDToBaseContainer[characterDeleteItem->characterID];
                        itemInstanceID = itemContainer.GetItem(characterDeleteItem->slot).objectGuid.GetCounter();
                    }
                    else
                    {
                        Database::Container& itemContainer = databaseState.itemTables.itemInstanceIDToContainer[containerID];
                        itemInstanceID = itemContainer.GetItem(characterDeleteItem->slot).objectGuid.GetCounter();
                    }

                    auto transaction = conn->NewTransaction();
                    if (Database::Util::Item::ItemInstanceDestroy(transaction, itemInstanceID))
                    {
                        if (Database::Util::Character::CharacterDeleteItem(transaction, characterDeleteItem->characterID, itemInstanceID))
                        {
                            if (containerID == 0)
                            {
                                Database::Container& itemContainer = databaseState.characterTables.charIDToBaseContainer[characterDeleteItem->characterID];
                                itemContainer.RemoveItem(characterDeleteItem->slot);
                            }
                            else
                            {
                                Database::Container& itemContainer = databaseState.itemTables.itemInstanceIDToContainer[containerID];
                                itemContainer.RemoveItem(characterDeleteItem->slot);
                            }

                            databaseState.itemTables.itemInstanceIDToDefinition.erase(itemInstanceID);
                            databaseState.itemTables.itemInstanceIDToContainer.erase(itemInstanceID); // TODO : Handle container items

                            result = true;
                            transaction.commit();
                        }
                    }

                    if (!result)
                    {
                        transaction.abort();
                    }
                }
            }

            if (characterDeleteItem->callback)
                characterDeleteItem->callback(registry, gameCommandBase, result, itemInstanceID);
        }

        void CharacterSwapContainerSlotsHandler(entt::registry& registry, Singletons::GameCommandQueue& commandQueue, GameCommandBase* gameCommandBase)
        {
            CharacterSwapContainerSlots* characterSwapContainerSlots = static_cast<CharacterSwapContainerSlots*>(gameCommandBase);
            if (!characterSwapContainerSlots)
                return;

            Singletons::DatabaseState& databaseState = registry.ctx().get<Singletons::DatabaseState>();

            bool result = false;
            if (auto conn = databaseState.controller->GetConnection(Database::DBType::Character))
            {
                bool charHasBaseContainer = databaseState.characterTables.charIDToBaseContainer.contains(characterSwapContainerSlots->characterID);
                if (charHasBaseContainer)
                {
                    auto& baseContainer = databaseState.characterTables.charIDToBaseContainer[characterSwapContainerSlots->characterID];
                    Database::Container* srcContainer = nullptr;
                    Database::Container* destContainer = nullptr;
                    u64 srcContainerID = 0;
                    u64 destContainerID = 0;

                    if (characterSwapContainerSlots->srcContainerIndex == 0)
                    {
                        srcContainer = &baseContainer;
                    }
                    else
                    {
                        u32 firstBagIndex = 19;
                        u32 bagIndex = firstBagIndex + (characterSwapContainerSlots->srcContainerIndex - 1);

                        const auto& baseContainerItem = baseContainer.GetItem(bagIndex);
                        if (!baseContainerItem.IsEmpty())
                        {
                            srcContainerID = baseContainerItem.objectGuid.GetCounter();
                            if (databaseState.itemTables.itemInstanceIDToContainer.contains(srcContainerID))
                                srcContainer = &databaseState.itemTables.itemInstanceIDToContainer[srcContainerID];
                        }
                    }

                    bool isSameContainer = characterSwapContainerSlots->srcContainerIndex == characterSwapContainerSlots->destContainerIndex;
                    if (isSameContainer)
                    {
                        destContainer = srcContainer;
                        destContainerID = srcContainerID;
                    }
                    else
                    {
                        if (characterSwapContainerSlots->destContainerIndex == 0)
                        {
                            destContainer = &baseContainer;
                        }
                        else
                        {
                            u32 firstBagIndex = 19;
                            u32 bagIndex = firstBagIndex + (characterSwapContainerSlots->destContainerIndex - 1);

                            const auto& baseContainerItem = baseContainer.GetItem(bagIndex);
                            if (!baseContainerItem.IsEmpty())
                            {
                                destContainerID = baseContainerItem.objectGuid.GetCounter();
                                if (databaseState.itemTables.itemInstanceIDToContainer.contains(destContainerID))
                                    destContainer = &databaseState.itemTables.itemInstanceIDToContainer[destContainerID];
                            }
                        }
                    }

                    if (srcContainer && destContainer)
                    {
                        auto transaction = conn->NewTransaction();
                        if (Database::Util::Character::CharacterSwapContainerSlots(transaction, characterSwapContainerSlots->characterID, srcContainerID, destContainerID, characterSwapContainerSlots->srcSlotIndex, characterSwapContainerSlots->destSlotIndex))
                        {
                            srcContainer->SwapItems(*destContainer, characterSwapContainerSlots->srcSlotIndex, characterSwapContainerSlots->destSlotIndex);

                            result = true;
                            transaction.commit();
                        }
                        else
                        {
                            transaction.abort();
                        }
                    }
                }
            }

            if (characterSwapContainerSlots->callback)
                characterSwapContainerSlots->callback(registry, gameCommandBase, result);
        }
    }
}
