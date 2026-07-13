#pragma once

#include "DatabaseConfiguration.h"
#include "Definitions.h"
#include "OperationResult.h"

#include <Gameplay/GameDefine.h>
#include <MetaGen/Shared/ClientDB/ClientDB.h>
#include <MetaGen/Postgres/Auth/Tables/Accounts.h>
#include <MetaGen/Postgres/Character/Tables/CharacterItems.h>
#include <MetaGen/Postgres/Character/Tables/Characters.h>
#include <MetaGen/Postgres/Character/Tables/ItemInstances.h>
#include <MetaGen/Postgres/World/Tables/Creatures.h>
#include <MetaGen/Postgres/World/Tables/MapLocations.h>
#include <MetaGen/Postgres/World/Tables/ProximityTriggers.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Database
{
    class DBController;

    enum class DeleteOutcome : u8 { Deleted, AlreadyAbsent };
    enum class UpdateOutcome : u8 { Updated, TargetNotFound };

    struct CreatureCreateRequest
    {
        u32 templateID = 0;
        u32 displayID = 0;
        u32 mapID = 0;
        vec3 position = {};
        f32 orientation = 0.0f;
        std::string scriptName;
    };

    struct ProximityTriggerCreateRequest
    {
        std::string name;
        u16 flags = 0;
        u32 mapID = 0;
        vec3 position = {};
        vec3 extents = {};
    };

    struct WorldCacheSnapshot
    {
        MapTables maps;
        SpellTables spells;
        ItemTables items;
        CreatureTables creatures;
    };

    struct CharacterInitializationSnapshot
    {
        std::optional<MetaGen::Postgres::Character::CharactersRecord> character;
        std::vector<MetaGen::Postgres::Character::ItemInstancesRecord> items;
        std::vector<MetaGen::Postgres::Character::CharacterItemsRecord> placements;
    };

    class DatabaseService
    {
    public:
        DatabaseService();
        ~DatabaseService();
        DatabaseService(const DatabaseService&) = delete;
        DatabaseService& operator=(const DatabaseService&) = delete;

        void InitializeBundle(DBType bundle, const DBEntry& configuration);

        OperationResult<std::optional<MetaGen::Postgres::Auth::AccountsRecord>> GetAccountByName(const std::string& name);
        OperationResult<MetaGen::Postgres::Auth::AccountsRecord> CreateAccount(const std::string& name, const std::string& email,
            u64 registrationTimestamp, unsigned char* blob, u32 blobSize);

        OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> GetCharacterByID(u64 characterID);
        OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> GetCharacterByName(const std::string& name);
        OperationResult<std::vector<MetaGen::Postgres::Character::CharactersRecord>> GetCharactersByAccount(u64 accountID);
        OperationResult<std::vector<MetaGen::Postgres::Character::ItemInstancesRecord>> GetCharacterItems(u64 characterID);
        OperationResult<std::vector<MetaGen::Postgres::Character::CharacterItemsRecord>> GetCharacterItemPlacements(u64 characterID);
        OperationResult<CharacterInitializationSnapshot> GetCharacterInitialization(u64 characterID);
        OperationResult<MetaGen::Postgres::Character::CharactersRecord> CreateCharacter(u64 accountID, const std::string& name, u16 raceGenderClass);
        OperationResult<DeleteOutcome> DeleteCharacter(u64 characterID);
        OperationResult<UpdateOutcome> SetCharacterRaceGenderClass(u64 characterID, u16 raceGenderClass);
        OperationResult<UpdateOutcome> SetCharacterLevel(u64 characterID, u16 level);
        OperationResult<UpdateOutcome> SetCharacterMap(u64 characterID, u32 mapID);
        OperationResult<UpdateOutcome> SetCharacterPosition(u64 characterID, const vec3& position, f32 orientation);
        OperationResult<UpdateOutcome> SetCharacterMapAndPosition(u64 characterID, u32 mapID, const vec3& position, f32 orientation);
        OperationResult<MetaGen::Postgres::Character::ItemInstancesRecord> AddCharacterItem(u64 characterID, u32 itemID,
            u64 containerID, u16 slot, u16 count, u16 durability);
        OperationResult<DeleteOutcome> DeleteCharacterItem(u64 characterID, u64 itemInstanceID);
        OperationResult<UpdateOutcome> SwapCharacterItems(u64 characterID, u64 sourceItemID, u64 destinationItemID,
            bool hasDestination, u64 sourceContainerID, u64 destinationContainerID, u16 sourceSlot, u16 destinationSlot);

        OperationResult<std::vector<MetaGen::Postgres::World::CreaturesRecord>> GetCreaturesByMap(u32 mapID);
        OperationResult<MetaGen::Postgres::World::CreaturesRecord> CreateCreature(const CreatureCreateRequest& request);
        OperationResult<DeleteOutcome> DeleteCreature(u64 creatureID);
        OperationResult<std::vector<MetaGen::Postgres::World::ProximityTriggersRecord>> GetProximityTriggersByMap(u32 mapID);
        OperationResult<MetaGen::Postgres::World::ProximityTriggersRecord> CreateProximityTrigger(const ProximityTriggerCreateRequest& request);
        OperationResult<DeleteOutcome> DeleteProximityTrigger(u32 triggerID);
        OperationResult<MetaGen::Postgres::World::MapLocationsRecord> CreateMapLocation(const std::string& name, u32 mapID, const vec3& position, f32 orientation);
        OperationResult<DeleteOutcome> DeleteMapLocation(u32 locationID);

        OperationResult<UpdateOutcome> SetItemTemplate(const GameDefine::Database::ItemTemplate& value);
        OperationResult<UpdateOutcome> SetItemStatTemplate(const GameDefine::Database::ItemStatTemplate& value);
        OperationResult<UpdateOutcome> SetItemArmorTemplate(const GameDefine::Database::ItemArmorTemplate& value);
        OperationResult<UpdateOutcome> SetItemWeaponTemplate(const GameDefine::Database::ItemWeaponTemplate& value);
        OperationResult<UpdateOutcome> SetItemShieldTemplate(const GameDefine::Database::ItemShieldTemplate& value);
        OperationResult<UpdateOutcome> SetMap(const GameDefine::Database::Map& value);
        OperationResult<UpdateOutcome> SetSpell(const GameDefine::Database::Spell& value);
        OperationResult<UpdateOutcome> SetSpellEffect(const GameDefine::Database::SpellEffect& value);
        OperationResult<UpdateOutcome> SetSpellProcData(u32 id, const MetaGen::Shared::ClientDB::SpellProcDataRecord& value);
        OperationResult<UpdateOutcome> SetSpellProcLink(u32 id, const MetaGen::Shared::ClientDB::SpellProcLinkRecord& value);

        OperationResult<WorldCacheSnapshot> LoadWorldCache();
        OperationResult<SpellTables> LoadSpellCache();

    private:
        std::unique_ptr<DBController> _controller;
    };
}
