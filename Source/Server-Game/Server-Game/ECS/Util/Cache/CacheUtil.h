#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <entt/fwd.hpp>

namespace Database
{
    struct Container;
    struct ItemInstance;
    struct SpellEffectInfo;
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
    void CharacterDelete(Singletons::GameCache& cache, u64 characterID, Components::CharacterInfo& characterInfo);

    void CharacterSetClass(Components::CharacterInfo& character, GameDefine::UnitClass unitClass);
    void CharacterSetLevel(Components::CharacterInfo& character, u16 level);

    bool ItemTemplateExistsByID(Singletons::GameCache& cache, u32 itemID);
    bool ItemStatTemplateExistsByID(Singletons::GameCache& cache, u32 statTemplateID);
    bool ItemArmorTemplateExistsByID(Singletons::GameCache& cache, u32 armorTemplateID);
    bool ItemWeaponTemplateExistsByID(Singletons::GameCache& cache, u32 weaponTemplateID);
    bool ItemShieldTemplateExistsByID(Singletons::GameCache& cache, u32 shieldTemplateID);
    bool ItemInstanceExistsByID(Singletons::GameCache& cache, u64 itemInstanceID);
    bool GetItemTemplateByID(Singletons::GameCache& cache, u32 itemID, GameDefine::Database::ItemTemplate*& itemTemplate);
    bool GetItemStatTemplateByID(Singletons::GameCache& cache, u32 statTemplateID, GameDefine::Database::ItemStatTemplate*& itemStatTemplate);
    bool GetItemArmorTemplateByID(Singletons::GameCache& cache, u32 armorTemplateID, GameDefine::Database::ItemArmorTemplate*& itemArmorTemplate);
    bool GetItemWeaponTemplateByID(Singletons::GameCache& cache, u32 weaponTemplateID, GameDefine::Database::ItemWeaponTemplate*& itemWeaponTemplate);
    bool GetItemShieldTemplateByID(Singletons::GameCache& cache, u32 shieldTemplateID, GameDefine::Database::ItemShieldTemplate*& itemShieldTemplate);
    bool GetItemInstanceByID(Singletons::GameCache& cache, u64 itemInstanceID, Database::ItemInstance*& itemInstance);

    bool ItemInstanceCreate(Singletons::GameCache& cache, u64 itemInstanceID, const Database::ItemInstance& itemInstance);
    bool ItemInstanceDelete(Singletons::GameCache& cache, u64 itemInstanceID);

    bool CreatureTemplateExistsByID(Singletons::GameCache& cache, u32 creatureTemplateID);
    bool GetCreatureTemplateByID(Singletons::GameCache& cache, u32 creatureTemplateID, GameDefine::Database::CreatureTemplate*& creatureTemplate);

    bool MapExistsByID(Singletons::GameCache& cache, u32 mapID);
    bool GetMapByID(Singletons::GameCache& cache, u32 mapID, GameDefine::Database::Map*& map);

    bool LocationExistsByID(Singletons::GameCache& cache, u32 locationID);
    bool LocationExistsByHash(Singletons::GameCache& cache, u32 locationHash);
    bool GetLocationByID(Singletons::GameCache& cache, u32 locationID, GameDefine::Database::MapLocation*& location);
    bool GetLocationByHash(Singletons::GameCache& cache, u32 locationHash, GameDefine::Database::MapLocation*& location);

    bool SpellExistsByID(Singletons::GameCache& cache, u32 spellID);
    bool GetSpellByID(Singletons::GameCache& cache, u32 spellID, GameDefine::Database::Spell*& spell);
    bool GetSpellEffectsBySpellID(Singletons::GameCache& cache, u32 spellID, Database::SpellEffectInfo*& dbSpellEffectInfo);
}