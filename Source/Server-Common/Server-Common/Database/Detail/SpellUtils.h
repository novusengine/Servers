#pragma once
#include "Server-Common/Database/Definitions.h"

#include <Base/Types.h>

#include <pqxx/pqxx>

namespace Database
{
    namespace Detail::Spell
    {
        namespace Loading
        {
            void LoadSpellTables(pqxx::read_transaction& transaction, Database::SpellTables& spellTables);
            u64 LoadSpells(pqxx::read_transaction& transaction, Database::SpellTables& spellTables);
            u64 LoadSpellEffects(pqxx::read_transaction& transaction, Database::SpellTables& spellTables);
            u64 LoadSpellProcData(pqxx::read_transaction& transaction, Database::SpellTables& spellTables);
            u64 LoadSpellProcLink(pqxx::read_transaction& transaction, Database::SpellTables& spellTables);
        }
    }
}
