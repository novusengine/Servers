#include "DatabaseService.h"

#include "DBController.h"
#include "GeneratedRuntime.h"
#include "MigrationRunner.h"
#include "ReliableExecution.h"
#include "SchemaTraits.h"
#include "Detail/AccountUtils.h"
#include "Detail/CharacterUtils.h"
#include "Detail/CreatureUtils.h"
#include "Detail/FactionUtils.h"
#include "Detail/ItemUtils.h"
#include "Detail/MapUtils.h"
#include "Detail/ProximityTriggerUtils.h"
#include "Detail/SpellUtils.h"

#include <MetaGen/Postgres/Auth/Tables/AccountPermissionGroups.h>
#include <MetaGen/Postgres/Auth/Tables/AccountPermissions.h>
#include <MetaGen/Postgres/Character/Tables/CharacterPermissionGroups.h>
#include <MetaGen/Postgres/Character/Tables/CharacterPermissions.h>
#include <MetaGen/Postgres/World/Tables/ItemArmorTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemShieldTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemStatTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemTemplates.h>
#include <MetaGen/Postgres/World/Tables/ItemWeaponTemplates.h>
#include <MetaGen/Postgres/World/Tables/Maps.h>
#include <MetaGen/Postgres/World/Tables/SpellEffects.h>
#include <MetaGen/Postgres/World/Tables/SpellProcData.h>
#include <MetaGen/Postgres/World/Tables/SpellProcLink.h>
#include <MetaGen/Postgres/World/Tables/Spells.h>

#include <limits>
#include <stdexcept>

#include <robinhood/robinhood.h>

namespace Database
{
    namespace
    {
        UpdateOutcome Updated(bool value)
        {
            return value ? UpdateOutcome::Updated : UpdateOutcome::TargetNotFound;
        }
        DeleteOutcome Deleted(bool value)
        {
            return value ? DeleteOutcome::Deleted : DeleteOutcome::AlreadyAbsent;
        }
        bool ValidMapID(u32 value)
        {
            return value <= static_cast<u32>(std::numeric_limits<i32>::max());
        }

        void UpsertCharacterReputations(pqxx::transaction_base& transaction, u64 characterID, std::span<const CharacterReputationUpdate> updates)
        {
            if (updates.empty())
                return;

            std::string sql =
                "INSERT INTO \"public\".\"character_reputations\" "
                "(\"character_id\", \"faction_id\", \"value\", \"flags\") VALUES ";
            pqxx::params parameters;
            parameters.reserve(1 + updates.size() * 3);
            parameters.append(characterID);

            for (size_t i = 0; i < updates.size(); ++i)
            {
                if (i != 0)
                    sql += ", ";

                const size_t parameter = 2 + i * 3;
                sql += "($1, $" + std::to_string(parameter) + ", $" + std::to_string(parameter + 1) +
                       ", $" + std::to_string(parameter + 2) + ")";
                parameters.append(updates[i].factionID);
                parameters.append(updates[i].value);
                parameters.append(updates[i].flags);
            }

            sql +=
                " ON CONFLICT (\"character_id\", \"faction_id\") DO UPDATE SET "
                "\"value\" = EXCLUDED.\"value\", \"flags\" = EXCLUDED.\"flags\"";
            transaction.exec(sql, parameters);
        }

        template <typename Update>
        std::vector<Update> CoalesceFactionUpdates(std::span<const Update> updates)
        {
            std::vector<Update> coalesced;
            robin_hood::unordered_flat_map<u16, size_t> factionToIndex;
            coalesced.reserve(updates.size());
            factionToIndex.reserve(updates.size());
            for (const Update& update : updates)
            {
                const auto [itr, inserted] = factionToIndex.emplace(update.factionID, coalesced.size());
                if (inserted)
                {
                    coalesced.push_back(update);
                }
                else
                {
                    coalesced[itr->second] = update;
                }
            }

            return coalesced;
        }
    }

    DatabaseService::DatabaseService()
        : _controller(std::make_unique<DBController>())
    {
    }
    DatabaseService::~DatabaseService() = default;

    void DatabaseService::InitializeBundle(DBType bundle, const DBEntry& configuration)
    {
        if (!_controller->SetDBEntry(bundle, configuration))
            throw std::runtime_error("Failed to configure database bundle");

        auto migrationConnection = _controller->OpenDedicatedConnection(bundle);
        if (!migrationConnection)
            throw std::runtime_error("Failed to open database migration connection");
        MigrationRunner::Run(bundle, *migrationConnection, configuration.migrationMode);

        bool prepared = false;
        switch (bundle)
        {
            case DBType::Auth:
                prepared = _controller->SetPrepareCallback(bundle, [](DBConnection& connection)
                {
                    Generated::PrepareStatements<SchemaTraits<DBType::Auth>::Schema>(*connection.connection);
                });
                break;
            case DBType::Character:
                prepared = _controller->SetPrepareCallback(bundle, [](DBConnection& connection)
                {
                    Generated::PrepareStatements<SchemaTraits<DBType::Character>::Schema>(*connection.connection);
                });
                break;
            case DBType::World:
                prepared = _controller->SetPrepareCallback(bundle, [](DBConnection& connection)
                {
                    Generated::PrepareStatements<SchemaTraits<DBType::World>::Schema>(*connection.connection);
                });
                break;
            default:
                throw std::runtime_error("Unsupported database bundle");
        }
        if (!prepared)
            throw std::runtime_error("Failed to configure prepared statements for database bundle");
    }

    OperationResult<std::optional<MetaGen::Postgres::Auth::AccountsRecord>> DatabaseService::GetAccountByName(const std::string& name)
    {
        return Detail::Account::AccountGetInfoByName(*_controller, name);
    }
    OperationResult<PermissionAssignmentSnapshot> DatabaseService::GetAccountPermissions(u64 accountID)
    {
        return Detail::Account::AccountGetPermissions(*_controller, accountID);
    }
    OperationResult<bool> DatabaseService::SetAccountPermission(u64 accountID, u32 permissionID, i64 value)
    {
        return RunTransaction(*_controller, DBType::Auth, "set_account_permission", [&](auto& tx)
        {
            Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionsTable::Set>(tx, accountID, permissionID, value);
            return true;
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteAccountPermission(u64 accountID, u32 permissionID)
    {
        return RunTransaction(*_controller, DBType::Auth, "delete_account_permission", [&](auto& tx)
        {
            return Deleted(Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionsTable::Delete>(
                               tx, accountID, permissionID)
                               .affected_rows() != 0);
        });
    }
    OperationResult<bool> DatabaseService::AddAccountPermissionGroup(u64 accountID, u32 permissionGroupID)
    {
        return RunTransaction(*_controller, DBType::Auth, "add_account_permission_group", [&](auto& tx)
        {
            Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionGroupsTable::Set>(tx, accountID, permissionGroupID);
            return true;
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteAccountPermissionGroup(u64 accountID, u32 permissionGroupID)
    {
        return RunTransaction(*_controller, DBType::Auth, "delete_account_permission_group", [&](auto& tx)
        {
            return Deleted(Generated::Execute<MetaGen::Postgres::Auth::AccountPermissionGroupsTable::Delete>(
                               tx, accountID, permissionGroupID)
                               .affected_rows() != 0);
        });
    }
    OperationResult<PermissionTables> DatabaseService::LoadPermissionTables()
    {
        return Detail::Account::LoadPermissionTables(*_controller);
    }
    OperationResult<bool> DatabaseService::SynchronizePermissionTables()
    {
        return RunTransaction(*_controller, DBType::Auth, "synchronize_permission_tables", [&](auto& tx)
        {
            return Detail::Account::SynchronizePermissionTables(tx);
        });
    }
    OperationResult<MetaGen::Postgres::Auth::AccountsRecord> DatabaseService::CreateAccount(const std::string& name, const std::string& email, u64 timestamp, unsigned char* blob, u32 size)
    {
        return RunTransaction(*_controller, DBType::Auth, "create_account", [&](auto& tx)
        {
            return Detail::Account::AccountCreate(tx, name, email, timestamp, blob, size);
        });
    }

    OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> DatabaseService::GetCharacterByID(u64 id)
    {
        return Detail::Character::CharacterGetInfoByID(*_controller, id);
    }
    OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> DatabaseService::GetCharacterByName(const std::string& name)
    {
        return Detail::Character::CharacterGetInfoByName(*_controller, name);
    }
    OperationResult<std::vector<MetaGen::Postgres::Character::CharactersRecord>> DatabaseService::GetCharactersByAccount(u64 id)
    {
        return Detail::Character::CharacterGetInfosByAccount(*_controller, id);
    }
    OperationResult<PermissionAssignmentSnapshot> DatabaseService::GetCharacterPermissions(u64 id)
    {
        return RunRead(*_controller, DBType::Character, "get_character_permissions", [&](auto& tx)
        {
            PermissionAssignmentSnapshot snapshot;
            auto permissions = Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionsTable::ByCharacter>(tx, id);
            snapshot.permissions.reserve(permissions.size());
            for (const auto& permission : permissions)
            {
                snapshot.permissions.push_back({ permission.permissionId, permission.value });
            }

            auto groups = Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionGroupsTable::ByCharacter>(tx, id);
            snapshot.permissionGroupIDs.reserve(groups.size());
            for (const auto& group : groups)
            {
                snapshot.permissionGroupIDs.push_back(group.permissionGroupId);
            }

            return snapshot;
        });
    }
    OperationResult<bool> DatabaseService::SetCharacterPermission(u64 characterID, u32 permissionID, i64 value)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_permission", [&](auto& tx)
        {
            Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionsTable::Set>(tx, characterID, permissionID, value);
            return true;
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCharacterPermission(u64 characterID, u32 permissionID)
    {
        return RunTransaction(*_controller, DBType::Character, "delete_character_permission", [&](auto& tx)
        {
            return Deleted(Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionsTable::Delete>(
                               tx, characterID, permissionID)
                               .affected_rows() != 0);
        });
    }
    OperationResult<bool> DatabaseService::AddCharacterPermissionGroup(u64 characterID, u32 permissionGroupID)
    {
        return RunTransaction(*_controller, DBType::Character, "add_character_permission_group", [&](auto& tx)
        {
            Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionGroupsTable::Set>(tx, characterID, permissionGroupID);
            return true;
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCharacterPermissionGroup(u64 characterID, u32 permissionGroupID)
    {
        return RunTransaction(*_controller, DBType::Character, "delete_character_permission_group", [&](auto& tx)
        {
            return Deleted(Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionGroupsTable::Delete>(
                               tx, characterID, permissionGroupID)
                               .affected_rows() != 0);
        });
    }
    OperationResult<std::vector<MetaGen::Postgres::Character::ItemInstancesRecord>> DatabaseService::GetCharacterItems(u64 id)
    {
        return Detail::Character::CharacterGetItems(*_controller, id);
    }
    OperationResult<std::vector<MetaGen::Postgres::Character::CharacterItemsRecord>> DatabaseService::GetCharacterItemPlacements(u64 id)
    {
        return Detail::Character::CharacterGetItemPlacements(*_controller, id);
    }
    OperationResult<std::vector<MetaGen::Postgres::Character::CharacterReputationsRecord>> DatabaseService::GetCharacterReputations(u64 id)
    {
        return RunRead(*_controller, DBType::Character, "get_character_reputations", [&](auto& tx)
        {
            return Generated::Execute<MetaGen::Postgres::Character::CharacterReputationsTable::ByCharacter>(tx, id);
        });
    }
    OperationResult<CharacterInitializationSnapshot> DatabaseService::GetCharacterInitialization(u64 id)
    {
        return RunRead(*_controller, DBType::Character, "get_character_initialization", [&](auto& tx)
        {
            CharacterInitializationSnapshot snapshot;
            snapshot.character = Generated::Execute<MetaGen::Postgres::Character::CharactersTable::ByPrimaryKey>(tx, id);
            snapshot.items = Generated::Execute<MetaGen::Postgres::Character::ItemInstancesTable::ByOwner>(tx, id);
            snapshot.placements = Generated::Execute<MetaGen::Postgres::Character::CharacterItemsTable::ByCharacter>(tx, id);
            snapshot.reputations = Generated::Execute<MetaGen::Postgres::Character::CharacterReputationsTable::ByCharacter>(tx, id);

            auto permissions = Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionsTable::ByCharacter>(tx, id);
            snapshot.permissions.permissions.reserve(permissions.size());
            for (const auto& permission : permissions)
            {
                snapshot.permissions.permissions.push_back({ permission.permissionId, permission.value });
            }

            auto groups = Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionGroupsTable::ByCharacter>(tx, id);
            snapshot.permissions.permissionGroupIDs.reserve(groups.size());
            for (const auto& group : groups)
            {
                snapshot.permissions.permissionGroupIDs.push_back(group.permissionGroupId);
            }

            return snapshot;
        });
    }
    OperationResult<MetaGen::Postgres::Character::CharactersRecord> DatabaseService::CreateCharacter(u64 accountID, const std::string& name, u16 raceGenderClass, u16 factionID)
    {
        return RunTransaction(*_controller, DBType::Character, "create_character", [&](auto& tx)
        {
            return Detail::Character::CharacterCreate(tx, accountID, name, raceGenderClass, factionID);
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterFaction(u64 characterID, u16 factionID)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_faction", [&](auto& tx)
        {
            return Updated(Generated::Execute<MetaGen::Postgres::Character::CharactersTable::SetFaction>(tx, factionID, characterID).affected_rows() != 0);
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterReputation(u64 characterID, u16 factionID, i32 value, u16 flags)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_reputation", [&](auto& tx)
        {
            return Updated(Generated::Execute<MetaGen::Postgres::Character::CharacterReputationsTable::Set>(tx, characterID, factionID, value, flags).affected_rows() != 0);
        });
    }
    OperationResult<u32> DatabaseService::SetCharacterReputations(u64 characterID, std::span<const CharacterReputationUpdate> updates)
    {
        if (updates.size() > MAX_CHARACTER_REPUTATION_UPDATES_PER_BATCH)
            return OperationFailure::Rejected;

        if (updates.empty())
            return u32{ 0 };

        return RunTransaction(*_controller, DBType::Character, "set_character_reputations", [&](auto& tx)
        {
            const std::vector<CharacterReputationUpdate> coalesced = CoalesceFactionUpdates(updates);
            UpsertCharacterReputations(tx, characterID, coalesced);
            return static_cast<u32>(updates.size());
        });
    }
    OperationResult<u32> DatabaseService::ApplyCharacterReputationChanges(u64 characterID, std::span<const CharacterReputationChange> changes)
    {
        if (changes.size() > MAX_CHARACTER_REPUTATION_UPDATES_PER_BATCH)
            return OperationFailure::Rejected;

        if (changes.empty())
            return u32{ 0 };

        return RunTransaction(*_controller, DBType::Character, "apply_character_reputation_changes", [&](auto& tx)
        {
            const std::vector<CharacterReputationChange> coalesced = CoalesceFactionUpdates(changes);
            std::vector<CharacterReputationUpdate> updates;
            std::vector<u16> removals;
            updates.reserve(coalesced.size());
            removals.reserve(coalesced.size());
            for (const CharacterReputationChange& change : coalesced)
            {
                if (change.remove)
                {
                    removals.push_back(change.factionID);
                }
                else
                {
                    updates.push_back({
                        .factionID = change.factionID,
                        .value = change.value,
                        .flags = change.flags
                    });
                }
            }

            if (!removals.empty())
            {
                std::string sql =
                    "DELETE FROM \"public\".\"character_reputations\" "
                    "WHERE \"character_id\" = $1 AND \"faction_id\" IN (";
                pqxx::params parameters;
                parameters.reserve(1 + removals.size());
                parameters.append(characterID);
                for (size_t i = 0; i < removals.size(); ++i)
                {
                    if (i != 0)
                        sql += ", ";

                    sql += "$" + std::to_string(i + 2);
                    parameters.append(removals[i]);
                }
                sql += ")";
                tx.exec(sql, parameters);
            }

            UpsertCharacterReputations(tx, characterID, updates);
            return static_cast<u32>(changes.size());
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCharacterReputation(u64 characterID, u16 factionID)
    {
        return RunTransaction(*_controller, DBType::Character, "delete_character_reputation", [&](auto& tx)
        {
            return Deleted(Generated::Execute<MetaGen::Postgres::Character::CharacterReputationsTable::Delete>(tx, characterID, factionID).affected_rows() != 0);
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCharacter(u64 id)
    {
        return RunTransaction(*_controller, DBType::Character, "delete_character", [&](auto& tx)
        {
            return Deleted(Detail::Character::CharacterDelete(tx, id));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterRaceGenderClass(u64 id, u16 value)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_race_gender_class", [&](auto& tx)
        {
            return Updated(Detail::Character::CharacterSetRaceGenderClass(tx, id, value));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterLevel(u64 id, u16 value)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_level", [&](auto& tx)
        {
            return Updated(Detail::Character::CharacterSetLevel(tx, id, value));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterMap(u64 id, u32 value)
    {
        if (!ValidMapID(value))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::Character, "set_character_map", [&](auto& tx)
        {
            return Updated(Detail::Character::CharacterSetMapID(tx, id, value));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterPosition(u64 id, const vec3& position, f32 orientation)
    {
        return RunTransaction(*_controller, DBType::Character, "set_character_position", [&](auto& tx)
        {
            return Updated(Detail::Character::CharacterSetPositionOrientation(tx, id, position, orientation));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetCharacterMapAndPosition(u64 id, u32 mapID, const vec3& position, f32 orientation)
    {
        if (!ValidMapID(mapID))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::Character, "set_character_map_position", [&](auto& tx)
        {
            if (!Detail::Character::CharacterSetMapID(tx, id, mapID))
                return UpdateOutcome::TargetNotFound;
            return Updated(Detail::Character::CharacterSetPositionOrientation(tx, id, position, orientation));
        });
    }
    OperationResult<MetaGen::Postgres::Character::ItemInstancesRecord> DatabaseService::AddCharacterItem(u64 characterID, u32 itemID, u64 containerID, u16 slot, u16 count, u16 durability)
    {
        return RunTransaction(*_controller, DBType::Character, "add_character_item", [&](auto& tx)
        {
            auto item = Detail::Item::ItemInstanceCreate(tx, itemID, characterID, count, durability);
            Detail::Character::CharacterAddItem(tx, characterID, item.id, containerID, slot);
            return item;
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCharacterItem(u64 characterID, u64 itemID)
    {
        return RunTransaction(*_controller, DBType::Character, "delete_character_item", [&](auto& tx)
        {
            if (!Detail::Character::CharacterDeleteItem(tx, characterID, itemID))
                return DeleteOutcome::AlreadyAbsent;
            return Deleted(Detail::Item::ItemInstanceDestroy(tx, itemID));
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SwapCharacterItems(u64 characterID, u64 sourceItemID, u64 destinationItemID,
        bool hasDestination, u64 sourceContainerID, u64 destinationContainerID, u16 sourceSlot, u16 destinationSlot)
    {
        return RunTransaction(*_controller, DBType::Character, "swap_character_items", [&](auto& tx)
        {
            if (!Detail::Character::CharacterLockItemsForSwap(tx, characterID, sourceItemID, destinationItemID,
                    hasDestination, sourceContainerID, destinationContainerID, sourceSlot, destinationSlot))
                return UpdateOutcome::TargetNotFound;
            if (!Detail::Character::CharacterDeleteItem(tx, characterID, sourceItemID))
                return UpdateOutcome::TargetNotFound;
            if (hasDestination && !Detail::Character::CharacterDeleteItem(tx, characterID, destinationItemID))
                return UpdateOutcome::TargetNotFound;
            Detail::Character::CharacterAddItem(tx, characterID, sourceItemID, destinationContainerID, destinationSlot);
            if (hasDestination)
                Detail::Character::CharacterAddItem(tx, characterID, destinationItemID, sourceContainerID, sourceSlot);
            return UpdateOutcome::Updated;
        });
    }

    OperationResult<std::vector<MetaGen::Postgres::World::CreaturesRecord>> DatabaseService::GetCreaturesByMap(u32 id)
    {
        if (!ValidMapID(id))
            return OperationFailure::Rejected;
        return Detail::Creature::CreatureGetInfoByMap(*_controller, id);
    }
    OperationResult<MetaGen::Postgres::World::CreaturesRecord> DatabaseService::CreateCreature(const CreatureCreateRequest& r)
    {
        if (!ValidMapID(r.mapID))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::World, "create_creature", [&](auto& tx)
        {
            return Detail::Creature::CreatureCreate(tx, r.templateID, r.displayID, r.mapID, r.position, r.orientation,
                r.scriptName, r.spawnTimeInSecMin, r.spawnTimeInSecMax, r.wanderDistance, r.movementType);
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteCreature(u64 id)
    {
        return RunTransaction(*_controller, DBType::World, "delete_creature", [&](auto& tx)
        {
            return Deleted(Detail::Creature::CreatureDelete(tx, id));
        });
    }
    OperationResult<std::vector<MetaGen::Postgres::World::ProximityTriggersRecord>> DatabaseService::GetProximityTriggersByMap(u32 id)
    {
        if (!ValidMapID(id))
            return OperationFailure::Rejected;
        return Detail::ProximityTrigger::ProximityTriggerGetInfoByMap(*_controller, id);
    }
    OperationResult<MetaGen::Postgres::World::ProximityTriggersRecord> DatabaseService::CreateProximityTrigger(const ProximityTriggerCreateRequest& r)
    {
        if (!ValidMapID(r.mapID))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::World, "create_proximity_trigger", [&](auto& tx)
        {
            return Detail::ProximityTrigger::ProximityTriggerCreate(tx, r.name, r.flags, r.mapID, r.position, r.extents);
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteProximityTrigger(u32 id)
    {
        return RunTransaction(*_controller, DBType::World, "delete_proximity_trigger", [&](auto& tx)
        {
            return Deleted(Detail::ProximityTrigger::ProximityTriggerDelete(tx, id));
        });
    }
    OperationResult<MetaGen::Postgres::World::MapLocationsRecord> DatabaseService::CreateMapLocation(const std::string& name, u32 mapID, const vec3& position, f32 orientation)
    {
        if (!ValidMapID(mapID))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::World, "create_map_location", [&](auto& tx)
        {
            return Detail::Map::MapLocationCreate(tx, name, mapID, position, orientation);
        });
    }
    OperationResult<DeleteOutcome> DatabaseService::DeleteMapLocation(u32 id)
    {
        return RunTransaction(*_controller, DBType::World, "delete_map_location", [&](auto& tx)
        {
            return Deleted(Detail::Map::MapLocationDelete(tx, id));
        });
    }

#define DB_SET_METHOD(Method, Type, Descriptor, Name, ...)                                        \
    OperationResult<UpdateOutcome> DatabaseService::Method(const Type& value)                     \
    {                                                                                             \
        return RunTransaction(*_controller, DBType::World, Name, [&](auto& tx) {                  \
            return Updated(Generated::Execute<Descriptor>(tx, __VA_ARGS__).affected_rows() != 0); \
        });                                                                                       \
    }

    DB_SET_METHOD(SetItemTemplate, GameDefine::Database::ItemTemplate, MetaGen::Postgres::World::ItemTemplatesTable::Set, "set_item_template", value.id, value.displayID, (u16)value.bind, (u16)value.rarity, (u16)value.category, (u16)value.type, value.virtualLevel, value.requiredLevel, value.durability, value.iconID, value.name, value.description, value.armor, value.statTemplateID, value.armorTemplateID, value.weaponTemplateID, value.shieldTemplateID)
    DB_SET_METHOD(SetItemStatTemplate, GameDefine::Database::ItemStatTemplate, MetaGen::Postgres::World::ItemStatTemplatesTable::Set, "set_item_stat_template", value.id, (u16)value.statTypes[0], (u16)value.statTypes[1], (u16)value.statTypes[2], (u16)value.statTypes[3], (u16)value.statTypes[4], (u16)value.statTypes[5], (u16)value.statTypes[6], (u16)value.statTypes[7], value.statValues[0], value.statValues[1], value.statValues[2], value.statValues[3], value.statValues[4], value.statValues[5], value.statValues[6], value.statValues[7])
    DB_SET_METHOD(SetItemArmorTemplate, GameDefine::Database::ItemArmorTemplate, MetaGen::Postgres::World::ItemArmorTemplatesTable::Set, "set_item_armor_template", value.id, (u16)value.equipType, value.bonusArmor)
    DB_SET_METHOD(SetItemWeaponTemplate, GameDefine::Database::ItemWeaponTemplate, MetaGen::Postgres::World::ItemWeaponTemplatesTable::Set, "set_item_weapon_template", value.id, (u16)value.weaponStyle, value.minDamage, value.maxDamage, value.speed)
    DB_SET_METHOD(SetItemShieldTemplate, GameDefine::Database::ItemShieldTemplate, MetaGen::Postgres::World::ItemShieldTemplatesTable::Set, "set_item_shield_template", value.id, value.bonusArmor, value.block)
    DB_SET_METHOD(SetSpell, GameDefine::Database::Spell, MetaGen::Postgres::World::SpellsTable::Set, "set_spell", value.id, value.name, value.description, value.auraDescription, value.iconID, value.castTime, value.cooldown, value.duration)
    DB_SET_METHOD(SetSpellEffect, GameDefine::Database::SpellEffect, MetaGen::Postgres::World::SpellEffectsTable::Set, "set_spell_effect", value.id, value.spellID, (u16)value.effectPriority, (u16)value.effectType, value.effectValue1, value.effectValue2, value.effectValue3, value.effectMiscValue1, value.effectMiscValue2, value.effectMiscValue3)
#undef DB_SET_METHOD

    OperationResult<UpdateOutcome> DatabaseService::SetMap(const GameDefine::Database::Map& value)
    {
        if (!ValidMapID(value.id))
            return OperationFailure::Rejected;
        return RunTransaction(*_controller, DBType::World, "set_map", [&](auto& tx)
        {
            return Updated(Generated::Execute<MetaGen::Postgres::World::MapsTable::Set>(tx, value.id, value.flags,
                               value.internalName, value.name, value.type, value.maxPlayers)
                               .affected_rows() != 0);
        });
    }

    OperationResult<UpdateOutcome> DatabaseService::SetSpellProcData(u32 id, const MetaGen::Shared::ClientDB::SpellProcDataRecord& v)
    {
        return RunTransaction(*_controller, DBType::World, "set_spell_proc_data", [&](auto& tx)
        {
            return Updated(Generated::Execute<MetaGen::Postgres::World::SpellProcDataTable::Set>(tx, id, (i32)v.phaseMask, (i64)v.typeMask, (i64)v.hitMask, (i64)v.flags, v.procsPerMinute, v.chanceToProc, (i32)v.internalCooldownMS, v.charges).affected_rows() != 0);
        });
    }
    OperationResult<UpdateOutcome> DatabaseService::SetSpellProcLink(u32 id, const MetaGen::Shared::ClientDB::SpellProcLinkRecord& v)
    {
        return RunTransaction(*_controller, DBType::World, "set_spell_proc_link", [&](auto& tx)
        {
            return Updated(Generated::Execute<MetaGen::Postgres::World::SpellProcLinkTable::Set>(tx, id, v.spellID, (i64)v.effectMask, v.procDataID).affected_rows() != 0);
        });
    }

    OperationResult<WorldCacheSnapshot> DatabaseService::LoadWorldCache()
    {
        return RunRead(*_controller, DBType::World, "load_world_cache", [&](auto& tx)
        {
            WorldCacheSnapshot s;
            Detail::Map::Loading::LoadMapTables(tx, s.maps);
            Detail::Spell::Loading::LoadSpellTables(tx, s.spells);
            Detail::Item::Loading::LoadItemTables(tx, s.items);
            Detail::Creature::Loading::LoadCreatureTables(tx, s.creatures);
            Detail::Faction::Loading::LoadFactionTables(tx, s.factions);
            return s;
        });
    }
    OperationResult<SpellTables> DatabaseService::LoadSpellCache()
    {
        return RunRead(*_controller, DBType::World, "load_spell_cache", [&](auto& tx)
        {
            SpellTables tables;
            Detail::Spell::Loading::LoadSpellTables(tx, tables);
            return tables;
        });
    }
}
