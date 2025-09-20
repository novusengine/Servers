#include "CacheUtil.h"

#include "Server-Game/ECS/Components/CharacterInfo.h"
#include "Server-Game/ECS/Components/DisplayInfo.h"
#include "Server-Game/ECS/Singletons/GameCache.h"
#include "Server-Game/ECS/Util/UnitUtil.h"

#include <Base/Util/StringUtils.h>

#include <Server-Common/Database/Definitions.h>

#include <entt/entt.hpp>

namespace ECS::Util::Cache
{
    bool CharacterExistsByID(Singletons::GameCache& cache, u64 characterID)
    {
        bool exists = cache.characterTables.charIDToEntity.contains(characterID);
        return exists;
    }

    bool CharacterExistsByNameHash(Singletons::GameCache& cache, u32 nameHash)
    {
        bool exists = cache.characterTables.charNameHashToCharID.contains(nameHash);
        return exists;
    }

    bool GetCharacterIDByNameHash(Singletons::GameCache& cache, u32 nameHash, u64& characterID)
    {
        characterID = std::numeric_limits<u64>::max();

        if (!CharacterExistsByNameHash(cache, nameHash))
            return false;

        characterID = cache.characterTables.charNameHashToCharID[nameHash];
        return true;
    }

    bool GetEntityByCharacterID(Singletons::GameCache& cache, u64 characterID, entt::entity entity)
    {
        if (!CharacterExistsByID(cache, characterID))
            return false;

        entity = cache.characterTables.charIDToEntity[characterID];
        return true;
    }

    void CharacterCreate(Singletons::GameCache& cache, u64 characterID, const std::string& name, entt::entity entity)
    {
        u32 charNameHash = StringUtils::fnv1a_32(name.c_str(), name.length());

        cache.characterTables.charIDToEntity[characterID] = entity;
        cache.characterTables.charNameHashToCharID[charNameHash] = characterID;
    }

    void CharacterDelete(Singletons::GameCache& cache, u64 characterID, Components::CharacterInfo& characterInfo)
    {
        u32 charNameHash = StringUtils::fnv1a_32(characterInfo.name.c_str(), characterInfo.name.length());

        cache.characterTables.charIDToEntity.erase(characterID);
        cache.characterTables.charNameHashToCharID.erase(charNameHash);
        cache.characterTables.charIDToPermissions.erase(characterID);
        cache.characterTables.charIDToPermissionGroups.erase(characterID);
        cache.characterTables.charIDToCurrency.erase(characterID);
    }

    void CharacterSetClass(Components::CharacterInfo& characterInfo, GameDefine::UnitClass unitClass)
    {
        characterInfo.unitClass = unitClass;
    }

    void CharacterSetLevel(Components::CharacterInfo& characterInfo, u16 level)
    {
        characterInfo.level = level;
    }

    bool ItemTemplateExistsByID(Singletons::GameCache& cache, u32 itemID)
    {
        bool exists = cache.itemTables.templateIDToTemplateDefinition.contains(itemID);
        return exists;
    }
    bool ItemWeaponTemplateExistsByID(Singletons::GameCache& cache, u32 weaponTemplateID)
    {
        bool exists = cache.itemTables.weaponTemplateIDToTemplateDefinition.contains(weaponTemplateID);
        return exists;
    }
    bool ItemInstanceExistsByID(Singletons::GameCache& cache, u64 itemInstanceID)
    {
        bool exists = cache.itemTables.itemInstanceIDToDefinition.contains(itemInstanceID);
        return exists;
    }
    bool GetItemTemplateByID(Singletons::GameCache& cache, u32 itemID, GameDefine::Database::ItemTemplate*& itemTemplate)
    {
        if (!ItemTemplateExistsByID(cache, itemID))
            return false;

        itemTemplate = &cache.itemTables.templateIDToTemplateDefinition[itemID];
        return true;
    }
    bool GetItemWeaponTemplateByID(Singletons::GameCache& cache, u32 weaponTemplateID, GameDefine::Database::ItemWeaponTemplate*& itemWeaponTemplate)
    {
        if (!ItemWeaponTemplateExistsByID(cache, weaponTemplateID))
            return false;

        itemWeaponTemplate = &cache.itemTables.weaponTemplateIDToTemplateDefinition[weaponTemplateID];
        return true;
    }
    bool GetItemInstanceByID(Singletons::GameCache& cache, u64 itemInstanceID, Database::ItemInstance*& itemInstance)
    {
        if (!ItemInstanceExistsByID(cache, itemInstanceID))
            return false;

        itemInstance = &cache.itemTables.itemInstanceIDToDefinition[itemInstanceID];
        return true;
    }

    bool ItemInstanceCreate(Singletons::GameCache& cache, u64 itemInstanceID, const Database::ItemInstance& itemInstance)
    {
        if (ItemInstanceExistsByID(cache, itemInstanceID))
            return false;

        cache.itemTables.itemInstanceIDToDefinition[itemInstanceID] = itemInstance;
        return true;
    }
    bool ItemInstanceDelete(Singletons::GameCache& cache, u64 itemInstanceID)
    {
        if (!ItemInstanceExistsByID(cache, itemInstanceID))
            return false;

        cache.itemTables.itemInstanceIDToDefinition.erase(itemInstanceID);
        return true;
    }
    bool CreatureTemplateExistsByID(Singletons::GameCache& cache, u32 creatureTemplateID)
    {
        bool exists = cache.creatureTables.templateIDToDefinition.contains(creatureTemplateID);
        return exists;
    }
    bool GetCreatureTemplateByID(Singletons::GameCache& cache, u32 creatureTemplateID, GameDefine::Database::CreatureTemplate*& creatureTemplate)
    {
        if (!CreatureTemplateExistsByID(cache, creatureTemplateID))
            return false;

        creatureTemplate = &cache.creatureTables.templateIDToDefinition[creatureTemplateID];
        return true;
    }

    bool MapExistsByID(Singletons::GameCache& cache, u32 mapID)
    {
        bool exists = cache.mapTables.idToDefinition.contains(mapID);
        return exists;
    }
    bool GetMapByID(Singletons::GameCache& cache, u32 mapID, GameDefine::Database::Map*& map)
    {
        if (!MapExistsByID(cache, mapID))
            return false;

        map = &cache.mapTables.idToDefinition[mapID];
        return true;
    }

    bool LocationExistsByID(Singletons::GameCache& cache, u32 locationID)
    {
        bool exists = cache.mapTables.locationIDToDefinition.contains(locationID);
        return exists;
    }
    bool LocationExistsByHash(Singletons::GameCache& cache, u32 locationHash)
    {
        bool exists = cache.mapTables.locationNameHashToID.contains(locationHash);
        return exists;
    }
    bool GetLocationByID(Singletons::GameCache& cache, u32 locationID, GameDefine::Database::MapLocation*& location)
    {
        if (!LocationExistsByID(cache, locationID))
            return false;

        location = &cache.mapTables.locationIDToDefinition[locationID];
        return true;
    }
    bool GetLocationByHash(Singletons::GameCache& cache, u32 locationHash, GameDefine::Database::MapLocation*& location)
    {
        if (!LocationExistsByHash(cache, locationHash))
            return false;

        u32 locationID = cache.mapTables.locationNameHashToID[locationHash];
        return GetLocationByID(cache, locationID, location);
    }

    bool SpellExistsByID(Singletons::GameCache& cache, u32 spellID)
    {
        bool exists = cache.spellTables.idToDefinition.contains(spellID);
        return exists;
    }

    bool GetSpellByID(Singletons::GameCache& cache, u32 spellID, GameDefine::Database::Spell*& spell)
    {
        if (!SpellExistsByID(cache, spellID))
            return false;

        spell = &cache.spellTables.idToDefinition[spellID];
        return true;
    }
    bool GetSpellEffectsBySpellID(Singletons::GameCache& cache, u32 spellID, std::vector<GameDefine::Database::SpellEffect>*& spellEffectList)
    {
        spellEffectList = nullptr;

        if (!SpellExistsByID(cache, spellID))
            return false;

        if (!cache.spellTables.spellIDToEffects.contains(spellID))
            return false;

        spellEffectList = &cache.spellTables.spellIDToEffects[spellID];
        return true;
    }
}