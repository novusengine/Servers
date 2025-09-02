#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>
#include <pqxx/transaction>

namespace Database
{
    struct Container;
}

namespace ECS
{
    enum class Result;

    namespace Singletons
    {
        struct GameCache;
    }

    namespace Util::Persistence::Character
    {
        ECS::Result CharacterCreate(Singletons::GameCache& gameCache, const std::string& name, u16 raceGenderClass, u64& characterID);
        ECS::Result CharacterDelete(Singletons::GameCache& gameCache, u64 characterID);
        ECS::Result CharacterSetRace(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitRace unitRace);
        ECS::Result CharacterSetGender(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitGender unitGender);
        ECS::Result CharacterSetClass(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, GameDefine::UnitClass unitClass);
        ECS::Result CharacterSetLevel(Singletons::GameCache& gameCache, entt::registry& registry, u64 characterID, u16 level);
        ECS::Result CharacterSetMapID(Singletons::GameCache& gameCache, u64 characterID, u32 mapID);
        ECS::Result CharacterSetPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, const vec3& position, f32 orientation);
        ECS::Result CharacterSetMapIDPositionOrientation(Singletons::GameCache& gameCache, u64 characterID, u32 mapID, const vec3& position, f32 orientation);

        ECS::Result ItemAssign(pqxx::work& transaction, Singletons::GameCache& gameCache, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot);
        ECS::Result ItemAdd(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity, u64 characterID, u32 itemID, Database::Container& container, u64 containerID, u16 slotIndex, u64& itemInstanceID);
        ECS::Result ItemDelete(Singletons::GameCache& gameCache, entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& container, u64 containerID, u16 slotIndex);
        ECS::Result ItemSwap(Singletons::GameCache& gameCache, u64 characterID, Database::Container& srcContainer, u64 srcContainerID, u16 srcSlotIndex, Database::Container& dstContainer, u64 dstContainerID, u16 dstSlotIndex);
    }
}