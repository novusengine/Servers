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

    namespace Util::Persistence::Character
    {
        ECS::Result CharacterCreate(entt::registry& registry, const std::string& name, u16 raceGenderClass, u64& characterID);
        ECS::Result CharacterDelete(entt::registry& registry, u64 characterID);
        ECS::Result CharacterSetRace(entt::registry& registry, u64 characterID, GameDefine::UnitRace unitRace);
        ECS::Result CharacterSetGender(entt::registry& registry, u64 characterID, GameDefine::UnitGender unitGender);
        ECS::Result CharacterSetClass(entt::registry& registry, u64 characterID, GameDefine::UnitClass unitClass);
        ECS::Result CharacterSetLevel(entt::registry& registry, u64 characterID, u16 level);

        ECS::Result ItemAssign(pqxx::work& transaction, entt::registry& registry, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot);
        ECS::Result ItemAdd(entt::registry& registry, entt::entity characterEntity, u64 characterID, u32 itemID, Database::Container& container, u64 containerID, u16 slotIndex, u64& itemInstanceID);
        ECS::Result ItemDelete(entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& container, u64 containerID, u16 slotIndex);
        ECS::Result ItemSwap(entt::registry& registry, entt::entity characterEntity, u64 characterID, Database::Container& srcContainer, u64 srcContainerID, u16 srcSlotIndex, Database::Container& dstContainer, u64 dstContainerID, u16 dstSlotIndex);
    }
}