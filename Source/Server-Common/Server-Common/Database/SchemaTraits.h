#pragma once

#include "DBController.h"

#include <MetaGen/Postgres/Auth/Schema.h>
#include <MetaGen/Postgres/Character/Schema.h>
#include <MetaGen/Postgres/World/Schema.h>

namespace Database
{
    template <DBType> struct SchemaTraits;
    template <> struct SchemaTraits<DBType::Auth> { using Schema = MetaGen::Postgres::Auth::AuthSchema; static constexpr std::string_view Name = "auth"; };
    template <> struct SchemaTraits<DBType::Character> { using Schema = MetaGen::Postgres::Character::CharacterSchema; static constexpr std::string_view Name = "character"; };
    template <> struct SchemaTraits<DBType::World> { using Schema = MetaGen::Postgres::World::WorldSchema; static constexpr std::string_view Name = "world"; };
}
