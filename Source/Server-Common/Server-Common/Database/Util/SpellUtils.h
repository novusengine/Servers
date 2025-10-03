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
            u64 LoadSpellProcData(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables);
            u64 LoadSpellProcLink(std::shared_ptr<DBConnection>& dbConnection, Database::SpellTables& spellTables);
        }
    }
}