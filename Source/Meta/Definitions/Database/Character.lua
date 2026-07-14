local U = require("PostgresDefinitions")
local D, P, Type = U.Definition, U.Postgres, U.Type
local Column, Table, Unique, ForeignKey = U.Column, U.Table, U.Unique, U.ForeignKey
local I, D0 = U.Identity, U.Default

return D.Definitions
{
    Table("character", "characters",
    {
        Column("id", Type.U64, P.BigInt, I), Column("account_id", Type.U64, P.BigInt, D0(0)),
        Column("name", Type.STRING, P.VarChar(12)), Column("total_time", Type.U32, P.Integer, D0(0)),
        Column("level_time", Type.U32, P.Integer, D0(0)), Column("logout_time", Type.U64, P.BigInt, D0(0)),
        Column("flags", Type.U32, P.Integer, D0(0)), Column("race_gender_class", Type.U16, P.SmallInt),
        Column("level", Type.U16, P.SmallInt, D0(1)), Column("experience_points", Type.U64, P.BigInt, D0(0)),
        Column("map_id", Type.U32, P.Integer, D0(0)), Column("position_x", Type.F32, P.Real, D0(0.0)),
        Column("position_y", Type.F32, P.Real, D0(0.0)), Column("position_z", Type.F32, P.Real, D0(0.0)),
        Column("position_o", Type.F32, P.Real, D0(0.0))
    }, { constraints = {
            Unique("characters", "characters_unique_name", { "name" }),
            P.RawCheck("characters_name_length", "char_length(name) >= 2", { persistentId = "postgres.characters.name_length" })
        }, indexes = {
            P.Index("characters_account_id_id_idx", { P.Asc("accountId"), P.Asc("id") },
                { persistentId = "postgres.characters.account_id_id_idx" })
        }, queries = {
            P.Query("ByName", { "name" }, { persistentId = "postgres.characters.by_name", cardinality = "zeroOrOne" }),
            P.Query("ByAccount", { "accountId" }, { persistentId = "postgres.characters.by_account", orderBy = { P.Asc("id") } })
        }, operations = {
            P.Delete("Delete", { "id" }, { persistentId = "postgres.characters.delete" }),
            P.Update("SetRaceGenderClass", { "id" }, { "raceGenderClass" }, { persistentId = "postgres.characters.set_race_gender_class" }),
            P.Update("SetLevel", { "id" }, { "level" }, { persistentId = "postgres.characters.set_level" }),
            P.Update("SetMap", { "id" }, { "mapId" }, { persistentId = "postgres.characters.set_map" }),
            P.Update("SetPosition", { "id" }, { "positionX", "positionY", "positionZ", "positionO" }, { persistentId = "postgres.characters.set_position" })
        } }),

    Table("character", "character_currency", {
        Column("id", Type.U64, P.BigInt, I), Column("character_id", Type.U64, P.BigInt),
        Column("currency_id", Type.U16, P.SmallInt), Column("value", Type.U64, P.BigInt, D0(0))
    }, { constraints = {
            Unique("character_currency", "character_currency_key", { "character_id", "currency_id" }),
            ForeignKey("character_currency", "character_currency_character_fk", { "character_id" }, "characters", { "id" }, "cascade")
        }, operations = {
            P.Upsert("Set", { "characterId", "currencyId", "value" }, { "characterId", "currencyId" }, { "value" }, { persistentId = "postgres.character_currency.set" }),
            P.Delete("Delete", { "characterId", "currencyId" }, { persistentId = "postgres.character_currency.delete" }),
            P.Delete("DeleteByCharacter", { "characterId" }, { persistentId = "postgres.character_currency.delete_by_character", cardinality = "zeroOrMore" })
        } }),

    Table("character", "character_permissions", {
        Column("id", Type.U64, P.BigInt, I), Column("character_id", Type.U64, P.BigInt), Column("permission_id", Type.U32, P.Integer),
        Column("value", Type.I64, P.BigInt, D0(1))
    }, { constraints = {
            Unique("character_permissions", "character_permission_key", { "character_id", "permission_id" }),
            ForeignKey("character_permissions", "character_permissions_character_fk", { "character_id" }, "characters", { "id" }, "cascade")
        }, operations = {
            P.Upsert("Set", { "characterId", "permissionId", "value" }, { "characterId", "permissionId" }, { "value" }, { persistentId = "postgres.character_permissions.set" }),
            P.Delete("Delete", { "characterId", "permissionId" }, { persistentId = "postgres.character_permissions.delete" }),
            P.Delete("DeleteByCharacter", { "characterId" }, { persistentId = "postgres.character_permissions.delete_by_character", cardinality = "zeroOrMore" })
        }, queries = {
            P.Query("ByCharacter", { "characterId" }, { persistentId = "postgres.character_permissions.by_character", orderBy = { P.Asc("id") } })
        } }),

    Table("character", "character_permission_groups", {
        Column("id", Type.U64, P.BigInt, I), Column("character_id", Type.U64, P.BigInt), Column("permission_group_id", Type.U32, P.Integer)
    }, { constraints = {
            Unique("character_permission_groups", "character_permission_group_key", { "character_id", "permission_group_id" }),
            ForeignKey("character_permission_groups", "character_permission_groups_character_fk", { "character_id" }, "characters", { "id" }, "cascade")
        }, queries = {
            P.Query("ByCharacter", { "characterId" }, { persistentId = "postgres.character_permission_groups.by_character", orderBy = { P.Asc("id") } })
        }, operations = {
            P.Upsert("Set", { "characterId", "permissionGroupId" }, { "characterId", "permissionGroupId" }, {}, { persistentId = "postgres.character_permission_groups.set" }),
            P.Delete("Delete", { "characterId", "permissionGroupId" }, { persistentId = "postgres.character_permission_groups.delete" }),
            P.Delete("DeleteByCharacter", { "characterId" }, { persistentId = "postgres.character_permission_groups.delete_by_character", cardinality = "zeroOrMore" })
        } }),

    Table("character", "item_instances", {
        Column("id", Type.U64, P.BigInt, I), Column("item_id", Type.U32, P.Integer), Column("owner_id", Type.U64, P.BigInt),
        Column("count", Type.U16, P.SmallInt), Column("durability", Type.U16, P.SmallInt)
    }, { constraints = {
            Unique("item_instances", "item_instances_owner_id_id_key", { "owner_id", "id" }),
            ForeignKey("item_instances", "item_instances_owner_fk", { "owner_id" }, "characters", { "id" }, "cascade")
        }, indexes = {
            P.Index("item_instances_owner_id_id_idx", { P.Asc("ownerId"), P.Asc("id") },
                { persistentId = "postgres.item_instances.owner_id_id_idx" })
        }, queries = { P.Query("ByOwner", { "ownerId" }, { persistentId = "postgres.item_instances.by_owner", orderBy = { P.Asc("id") } }) },
        operations = {
            P.Delete("Delete", { "id" }, { persistentId = "postgres.item_instances.delete" }),
            P.Delete("DeleteByOwner", { "ownerId" }, { persistentId = "postgres.item_instances.delete_by_owner", cardinality = "zeroOrMore" }),
            P.Update("Update", { "id" }, { "ownerId", "count", "durability" }, { persistentId = "postgres.item_instances.update" }),
            P.Update("SetOwner", { "id" }, { "ownerId" }, { persistentId = "postgres.item_instances.set_owner" }),
            P.Update("SetCount", { "id" }, { "count" }, { persistentId = "postgres.item_instances.set_count" }),
            P.Update("SetDurability", { "id" }, { "durability" }, { persistentId = "postgres.item_instances.set_durability" })
        } }),

    Table("character", "character_items", {
        Column("character_id", Type.U64, P.BigInt), Column("container", Type.U64, P.BigInt),
        Column("slot", Type.U16, P.SmallInt), Column("item_instance_id", Type.U64, P.BigInt)
    }, { noPrimaryKey = true, constraints = {
            Unique("character_items", "unique_item_instance_id", { "item_instance_id" }),
            Unique("character_items", "character_items_character_container_slot_key", { "character_id", "container", "slot" }),
            ForeignKey("character_items", "character_items_character_fk", { "character_id" }, "characters", { "id" }, "cascade"),
            ForeignKey("character_items", "character_items_owner_item_fk", { "character_id", "item_instance_id" },
                "item_instances", { "owner_id", "id" }, "cascade")
        },
        queries = {
            P.Query("ByCharacter", { "characterId" },
                { persistentId = "postgres.character_items.by_character", orderBy = { P.Asc("container"), P.Asc("slot") } })
        },
        operations = {
            P.Delete("DeleteByCharacter", { "characterId" }, { persistentId = "postgres.character_items.delete_by_character", cardinality = "zeroOrMore" })
        } })
}
