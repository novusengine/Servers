#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    struct DBConnection;

    namespace Util::Spell
    {
        namespace Loading
        {
            void InitSpellTablesPreparedStatements(std::shared_ptr<DBConnection>& dbConnection);

            void LoadSpellTables(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables);
            u64 LoadSpells(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables);
            u64 LoadSpellEffects(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables);
        }

        bool SpellLocationCreate(pqxx::work& transaction, const std::string& name, u16 spellID, const vec3& position, f32 orientation, u32& locationID);
        bool SpellLocationDelete(pqxx::work& transaction, u32 locationID);
    }
}