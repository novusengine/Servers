#pragma once
#include "Server-Common/Database/Definitions.h"
#include "Server-Common/Database/ReliableExecution.h"

#include <Base/Types.h>
#include <MetaGen/Postgres/World/Tables/Creatures.h>

#include <pqxx/pqxx>
#include <vector>

namespace Database
{
    namespace Detail::Creature
    {
        namespace Loading
        {
            void LoadCreatureTables(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables);
            u64 LoadCreatureTemplates(pqxx::read_transaction& transaction, Database::CreatureTables& creatureTables);
        }

        MetaGen::Postgres::World::CreaturesRecord CreatureCreate(pqxx::work& transaction, u32 templateID, u32 displayID, u32 mapID, const vec3& position, f32 orientation, const std::string& scriptName);
        bool CreatureDelete(pqxx::work& transaction, u64 creatureID);
        OperationResult<std::vector<MetaGen::Postgres::World::CreaturesRecord>> CreatureGetInfoByMap(DBController& dbController, u32 mapID);
    }
}
