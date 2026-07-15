#include "Character.h"
#include "Item.h"
#include "Server-Game/Application/Application.h"
#include "Server-Game/ECS/Components/CharacterReputation.h"
#include "Server-Game/ECS/Components/PlayerContainers.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/ECS/Util/FactionUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Server-Common/Database/Definitions.h>

#include <MetaGen/Shared/Unit/Unit.h>

#include <Scripting/Zenith.h>

#include <entt/entt.hpp>
#include <Server-Game/ECS/Util/Cache/CacheUtil.h>

namespace Scripting
{
    namespace Game
    {
        void Character::Register(Zenith* zenith)
        {
            LuaMetaTable<Character>::Register(zenith, "CharacterMetaTable");
            LuaMetaTable<Character>::Set(zenith, unitMethods);
            LuaMetaTable<Character>::Set(zenith, characterMethods);
        }

        Character* Character::Create(Zenith* zenith, entt::entity entity)
        {
            Character* character = zenith->PushUserData<Character>([](void* x)
            {
            });
            character->entity = entity;

            luaL_getmetatable(zenith->state, "CharacterMetaTable");
            lua_setmetatable(zenith->state, -2);

            return character;
        }

        namespace CharacterMethods
        {
            i32 GetEquippedItem(Zenith* zenith, Character* character)
            {
                auto slot = static_cast<MetaGen::Shared::Unit::ItemEquipSlotEnum>(zenith->CheckVal<u16>(2));
                if (slot < MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentStart || slot >= MetaGen::Shared::Unit::ItemEquipSlotEnum::EquipmentEnd)
                    return 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                u16 mapID = zenith->key.GetMapID();
                entt::entity entity = character->entity;

                ECS::World& world = worldState.GetWorld(mapID);
                if (!world.ValidEntity(entity))
                    return 0;

                auto& playerContainers = world.Get<ECS::Components::PlayerContainers>(entity);
                auto& containerItem = playerContainers.equipment.GetItem((u16)slot);
                if (containerItem.IsEmpty())
                    return 0;

                ObjectGUID itemGUID = containerItem.objectGUID;

                Database::ItemInstance* itemInstance = nullptr;
                if (!ECS::Util::Cache::GetItemInstanceByID(gameCache, itemGUID.GetCounter(), itemInstance))
                    return 0;

                Item::Create(zenith, itemGUID, itemInstance->itemID);
                return 1;
            }

            i32 GetReputation(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();
                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                if (!gameCache.factionRuntimeData || !world.ValidEntity(character->entity))
                    return 0;

                Gameplay::Faction::FactionIndex faction = Gameplay::Faction::INVALID_FACTION_INDEX;
                if (!gameCache.factionRuntimeData->TryGetFactionIndex(factionID, faction) || !Gameplay::Faction::HasFlag(gameCache.factionRuntimeData->GetDefinition(faction).flags, Gameplay::Faction::DefinitionFlags::AllowsReputation))
                    return 0;

                const auto& reputation = world.Get<ECS::Components::CharacterReputation>(character->entity);
                const ECS::Components::ReputationState* state =
                    ECS::Util::Faction::FindReputation(reputation, faction);
                const i32 value = ECS::Util::Faction::GetPersistentReputationValue(world, character->entity,
                    *gameCache.factionRuntimeData, factionID);
                zenith->Push(value);
                zenith->Push(state ? state->flags : static_cast<u16>(0));
                return 2;
            }

            i32 SetReputation(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                const i32 value = zenith->CheckVal<i32>(3);
                const u32 contentID = zenith->IsNumber(4) ? zenith->Get<u32>(4) : 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                const bool changed = gameCache.factionRuntimeData && world.ValidEntity(character->entity) && ECS::Util::Faction::SetReputation(world, character->entity, *gameCache.factionRuntimeData, factionID, value, { .type = Gameplay::Faction::ReputationSourceType::Script, .contentID = contentID });
                zenith->Push(changed);
                return 1;
            }

            i32 ModifyReputation(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                const i32 delta = zenith->CheckVal<i32>(3);
                const u32 contentID = zenith->IsNumber(4) ? zenith->Get<u32>(4) : 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                const bool changed = gameCache.factionRuntimeData && world.ValidEntity(character->entity) && ECS::Util::Faction::ModifyReputation(world, character->entity, *gameCache.factionRuntimeData, factionID, delta, { .type = Gameplay::Faction::ReputationSourceType::Script, .contentID = contentID });
                zenith->Push(changed);
                return 1;
            }

            i32 RemoveReputation(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                const u32 contentID = zenith->IsNumber(3) ? zenith->Get<u32>(3) : 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                const bool changed = gameCache.factionRuntimeData && world.ValidEntity(character->entity) && ECS::Util::Faction::RemoveReputation(world, character->entity, *gameCache.factionRuntimeData, factionID, { .type = Gameplay::Faction::ReputationSourceType::Script, .contentID = contentID });
                zenith->Push(changed);
                return 1;
            }

            i32 DiscoverReputation(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                const bool discovered = gameCache.factionRuntimeData && world.ValidEntity(character->entity) && ECS::Util::Faction::DiscoverReputation(world, character->entity, *gameCache.factionRuntimeData, factionID, Gameplay::Faction::DiscoverySource::Interaction);
                zenith->Push(discovered);
                return 1;
            }

            i32 SetReputationFlags(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                const u16 flags = zenith->CheckVal<u16>(3);
                const u32 contentID = zenith->IsNumber(4) ? zenith->Get<u32>(4) : 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                const bool changed = gameCache.factionRuntimeData && world.ValidEntity(character->entity) && ECS::Util::Faction::SetReputationFlags(world, character->entity, *gameCache.factionRuntimeData, factionID, flags, { .type = Gameplay::Faction::ReputationSourceType::Script, .contentID = contentID });
                zenith->Push(changed);
                return 1;
            }

            i32 SetReputationLocked(Zenith* zenith, Character* character)
            {
                const auto factionID = zenith->CheckVal<Gameplay::Faction::FactionID>(2);
                const bool locked = zenith->CheckVal<bool>(3);
                const u32 contentID = zenith->IsNumber(4) ? zenith->Get<u32>(4) : 0;

                entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
                auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
                auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

                ECS::World& world = worldState.GetWorld(zenith->key.GetMapID());
                if (!gameCache.factionRuntimeData || !world.ValidEntity(character->entity))
                {
                    zenith->Push(false);
                    return 1;
                }

                Gameplay::Faction::FactionIndex faction = Gameplay::Faction::INVALID_FACTION_INDEX;
                auto* reputation = world.TryGet<ECS::Components::CharacterReputation>(character->entity);
                if (!reputation || !gameCache.factionRuntimeData->TryGetFactionIndex(factionID, faction))
                {
                    zenith->Push(false);
                    return 1;
                }

                const auto* state = ECS::Util::Faction::FindReputation(*reputation, faction);
                if (!state)
                {
                    zenith->Push(false);
                    return 1;
                }

                const u16 lockFlag = static_cast<u16>(Gameplay::Faction::ReputationFlags::Locked);
                const u16 flags = locked ? state->flags | lockFlag : state->flags & ~lockFlag;
                const bool changed = ECS::Util::Faction::SetReputationFlags(world, character->entity, *gameCache.factionRuntimeData, factionID, flags, { .type = Gameplay::Faction::ReputationSourceType::Script, .contentID = contentID });
                zenith->Push(changed);
                return 1;
            }
        }
    }
}
