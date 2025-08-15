#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>

namespace Database
{
    struct Container;
    struct ItemInstance;
}

namespace ECS
{
    namespace Components
    {
        struct CharacterInfo;
        struct DisplayInfo;
    }

    namespace Singletons
    {
        struct GameCache;
    }
}

namespace ECS::Util::Cache
{
    bool CharacterExistsByID(Singletons::GameCache& cache, u64 characterID);
    bool CharacterExistsByNameHash(Singletons::GameCache& cache, u32 nameHash);
    bool GetCharacterIDByNameHash(Singletons::GameCache& cache, u32 nameHash, u64& characterID);
    bool GetEntityByCharacterID(Singletons::GameCache& cache, u64 characterID, entt::entity entity);

    void CharacterCreate(Singletons::GameCache& cache, u64 characterID, const std::string& name, entt::entity entity);
    void CharacterDelete(entt::registry& registry, Singletons::GameCache& cache, u64 characterID, Components::CharacterInfo& characterInfo);

    void CharacterSetClass(Components::CharacterInfo& character, GameDefine::UnitClass unitClass);
    void CharacterSetLevel(Components::CharacterInfo& character, u16 level);

    bool ItemTemplateExistsByID(Singletons::GameCache& cache, u32 itemID);
    bool ItemInstanceExistsByID(Singletons::GameCache& cache, u64 itemInstanceID);
    bool GetItemTemplateByID(Singletons::GameCache& cache, u32 itemID, GameDefine::Database::ItemTemplate*& itemTemplate);
    bool GetItemInstanceByID(Singletons::GameCache& cache, u64 itemInstanceID, Database::ItemInstance*& itemInstance);

    bool ItemInstanceCreate(Singletons::GameCache& cache, u64 itemInstanceID, const Database::ItemInstance& itemInstance);
    bool ItemInstanceDelete(Singletons::GameCache& cache, u64 itemInstanceID);
}