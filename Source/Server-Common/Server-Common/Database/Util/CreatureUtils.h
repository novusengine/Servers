#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::Creature
    {
        namespace Loading
        {
            void InitCreatureTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

            void LoadCreatureTables(std::shared_ptr<DBConnection>& dbConnection, Database::CreatureTables& creatureTables);
            u64 LoadCreatureTemplates(std::shared_ptr<DBConnection>& dbConnection, Database::CreatureTables& creatureTables);
        }

        bool CreatureCreate(pqxx::work& transaction, u32 templateID, u32 displayID, u16 mapID, const vec3& position, f32 orientation, u64& creatureID);
        bool CreatureDelete(pqxx::work& transaction, u64 creatureID);
        bool CreatureGetInfoByMap(std::shared_ptr<DBConnection>& dbConnection, u16 mapID, pqxx::result& result);
    }
}