#include "CharacterUtils.h"
#include "Server-Common/Database/DBController.h"
#include "Server-Common/Database/GeneratedRuntime.h"

#include <Base/Util/DebugHandler.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/Postgres/Character/Tables/CharacterCurrency.h>
#include <MetaGen/Postgres/Character/Tables/CharacterPermissionGroups.h>
#include <MetaGen/Postgres/Character/Tables/CharacterPermissions.h>
#include <MetaGen/Postgres/Character/Tables/Characters.h>
#include <MetaGen/Postgres/Character/Tables/CharacterItems.h>
#include <MetaGen/Postgres/Character/Tables/ItemInstances.h>

namespace Database::Detail::Character
{
    OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfoByID(DBController& dbController, u64 characterID)
    {
        return RunRead(dbController, DBType::Character, "get_character_by_id", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::ByPrimaryKey>(transaction, characterID);
        });
    }

    OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfoByName(DBController& dbController, const std::string& name)
    {
        return RunRead(dbController, DBType::Character, "get_character_by_name", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::ByName>(transaction, name);
        });
    }

    OperationResult<std::vector<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfosByAccount(DBController& dbController, u64 accountID)
    {
        return RunRead(dbController, DBType::Character, "get_characters_by_account", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::ByAccount>(transaction, accountID);
        });
    }

    MetaGen::Postgres::Character::CharactersRecord CharacterCreate(pqxx::work& transaction, u64 accountID, const std::string& name, u16 raceGenderClass, u16 factionID)
    {
        return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::Insert>(transaction,
            accountID, name, u32{ 0 }, u32{ 0 }, u64{ 0 }, u32{ 0 }, raceGenderClass,
            u16{ 1 }, u64{ 0 }, u16{ 0 }, f32{ 0.0f }, f32{ 0.0f }, f32{ 0.0f }, f32{ 0.0f }, factionID);
    }

    bool CharacterDelete(pqxx::work& transaction, u64 characterID)
    {
        if (Generated::Execute<MetaGen::Postgres::Character::CharactersTable::Delete>(transaction, characterID).affected_rows() == 0)
            return false;

        Generated::Execute<MetaGen::Postgres::Character::CharacterCurrencyTable::DeleteByCharacter>(transaction, characterID);
        Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionsTable::DeleteByCharacter>(transaction, characterID);
        Generated::Execute<MetaGen::Postgres::Character::CharacterPermissionGroupsTable::DeleteByCharacter>(transaction, characterID);
        Generated::Execute<MetaGen::Postgres::Character::CharacterItemsTable::DeleteByCharacter>(transaction, characterID);
        Generated::Execute<MetaGen::Postgres::Character::ItemInstancesTable::DeleteByOwner>(transaction, characterID);
        return true;
    }

    bool CharacterSetRaceGenderClass(pqxx::work& transaction, u64 characterID, u16 raceGenderClass)
    {
        return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::SetRaceGenderClass>(transaction, raceGenderClass, characterID).affected_rows() != 0;
    }

    bool CharacterSetLevel(pqxx::work& transaction, u64 characterID, u16 level)
    {
        return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::SetLevel>(transaction, level, characterID).affected_rows() != 0;
    }

    bool CharacterSetMapID(pqxx::work& transaction, u64 characterID, u32 mapID)
    {
        return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::SetMap>(transaction, mapID, characterID).affected_rows() != 0;
    }

    bool CharacterSetPositionOrientation(pqxx::work& transaction, u64 characterID, const vec3& position, f32 orientation)
    {
        return Generated::Execute<MetaGen::Postgres::Character::CharactersTable::SetPosition>(transaction, position.x, position.y, position.z, orientation, characterID).affected_rows() != 0;
    }

    bool CharacterAddItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot)
    {
        Generated::Execute<MetaGen::Postgres::Character::CharacterItemsTable::Insert>(transaction, characterID, containerID, slot, itemInstanceID);
        return true;
    }

    bool CharacterLockItemsForSwap(pqxx::work& transaction, u64 characterID, u64 sourceItemInstanceID, u64 destinationItemInstanceID, bool hasDestination, u64 sourceContainerID, u64 destinationContainerID, u16 sourceSlot, u16 destinationSlot)
    {
        const auto result = transaction.exec(
            "SELECT ci.item_instance_id, ci.container, ci.slot "
            "FROM public.character_items ci "
            "INNER JOIN public.item_instances ii ON ii.id = ci.item_instance_id "
            "WHERE ci.character_id = $1 AND ii.owner_id = $1 "
            "AND (ci.item_instance_id = $2 OR ($3 AND ci.item_instance_id = $4)) "
            "ORDER BY ci.item_instance_id FOR UPDATE OF ci, ii",
            Generated::MakeParameters(characterID, sourceItemInstanceID, hasDestination, destinationItemInstanceID));

        if (result.size() != (hasDestination ? 2u : 1u))
            return false;

        bool foundSource = false;
        bool foundDestination = !hasDestination;
        for (const auto& row : result)
        {
            const u64 itemInstanceID = row[0].as<u64>();
            const u64 containerID = row[1].as<u64>();
            const u16 slot = row[2].as<u16>();
            if (itemInstanceID == sourceItemInstanceID)
            {
                foundSource = containerID == sourceContainerID && slot == sourceSlot;
            }
            else if (hasDestination && itemInstanceID == destinationItemInstanceID)
            {
                foundDestination = containerID == destinationContainerID && slot == destinationSlot;
            }
        }

        return foundSource && foundDestination;
    }

    bool CharacterDeleteItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID)
    {
        return transaction.exec(
                              "DELETE FROM public.character_items ci USING public.item_instances ii "
                              "WHERE ci.character_id = $1 AND ci.item_instance_id = $2 "
                              "AND ii.id = ci.item_instance_id AND ii.owner_id = $1",
                              Generated::MakeParameters(characterID, itemInstanceID))
                   .affected_rows() != 0;
    }

    OperationResult<std::vector<MetaGen::Postgres::Character::ItemInstancesRecord>> CharacterGetItems(DBController& dbController, u64 characterID)
    {
        return RunRead(dbController, DBType::Character, "get_character_items", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Character::ItemInstancesTable::ByOwner>(transaction, characterID);
        });
    }
    OperationResult<std::vector<MetaGen::Postgres::Character::CharacterItemsRecord>> CharacterGetItemPlacements(DBController& dbController, u64 characterID)
    {
        return RunRead(dbController, DBType::Character, "get_character_item_placements", [&](pqxx::read_transaction& transaction)
        {
            return Generated::Execute<MetaGen::Postgres::Character::CharacterItemsTable::ByCharacter>(transaction, characterID);
        });
    }
}
