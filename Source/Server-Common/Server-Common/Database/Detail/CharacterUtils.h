#pragma once
#include "Server-Common/Database/Definitions.h"
#include "Server-Common/Database/ReliableExecution.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/Character/Tables/Characters.h>
#include <MetaGen/Postgres/Character/Tables/CharacterItems.h>
#include <MetaGen/Postgres/Character/Tables/ItemInstances.h>

#include <pqxx/pqxx>
#include <vector>

namespace Database
{
    namespace Detail::Character
    {
        OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfoByID(DBController& dbController, u64 characterID);
        OperationResult<std::optional<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfoByName(DBController& dbController, const std::string& name);
        OperationResult<std::vector<MetaGen::Postgres::Character::CharactersRecord>> CharacterGetInfosByAccount(DBController& dbController, u64 accountID);
        MetaGen::Postgres::Character::CharactersRecord CharacterCreate(pqxx::work& transaction, u64 accountID, const std::string& name, u16 raceGenderClass, u16 factionID);
        bool CharacterDelete(pqxx::work& transaction, u64 characterID);
        bool CharacterSetRaceGenderClass(pqxx::work& transaction, u64 characterID, u16 raceGenderClass);
        bool CharacterSetLevel(pqxx::work& transaction, u64 characterID, u16 level);
        bool CharacterSetMapID(pqxx::work& transaction, u64 characterID, u32 mapID);
        bool CharacterSetPositionOrientation(pqxx::work& transaction, u64 characterID, const vec3& position, f32 orientation);
        bool CharacterAddItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID, u64 containerID, u16 slot);
        bool CharacterLockItemsForSwap(pqxx::work& transaction, u64 characterID, u64 sourceItemInstanceID, u64 destinationItemInstanceID, bool hasDestination, u64 sourceContainerID, u64 destinationContainerID, u16 sourceSlot, u16 destinationSlot);
        bool CharacterDeleteItem(pqxx::work& transaction, u64 characterID, u64 itemInstanceID);
        OperationResult<std::vector<MetaGen::Postgres::Character::ItemInstancesRecord>> CharacterGetItems(DBController& dbController, u64 characterID);
        OperationResult<std::vector<MetaGen::Postgres::Character::CharacterItemsRecord>> CharacterGetItemPlacements(DBController& dbController, u64 characterID);
    }
}
