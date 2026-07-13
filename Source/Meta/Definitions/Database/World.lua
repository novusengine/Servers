local U = require("PostgresDefinitions")
local D, P, Type = U.Definition, U.Postgres, U.Type
local Column, Table, Unique, ForeignKey, Upsert = U.Column, U.Table, U.Unique, U.ForeignKey, U.Upsert
local I, D0 = U.Identity, U.Default

return D.Definitions
{

    Table("world", "currency",
    {
        Column("id", Type.U16, P.SmallInt, I),
        Column("name", Type.STRING, P.Text)
    }, { constraints = { Unique("currency", "currency_unique_name", { "name" }) } }),

    Table("world", "permissions",
    {
        Column("id", Type.U16, P.SmallInt, I),
        Column("name", Type.STRING, P.Text)
    }, { constraints = { Unique("permissions", "permissions_unique_name", { "name" }) } }),

    Table("world", "permission_groups",
    {
        Column("id", Type.U16, P.SmallInt, I),
        Column("name", Type.STRING, P.Text)
    }, { constraints = { Unique("permission_groups", "permission_groups_unique_name", { "name" }) } }),

    Table("world", "permission_group_data",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("group_id", Type.U16, P.SmallInt),
        Column("permission_id", Type.U16, P.SmallInt)
    }, { constraints =
        {
            Unique("permission_group_data", "permission_group_data_key", { "group_id", "permission_id" }),
            ForeignKey("permission_group_data", "permission_group_data_group_fk", { "group_id" }, "permission_groups", { "id" }, "cascade"),
            ForeignKey("permission_group_data", "permission_group_data_permission_fk", { "permission_id" }, "permissions", { "id" }, "cascade")
        }
    }),

    Table("world", "creature_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("flags", Type.U32, P.Integer, D0(0)),
        Column("name", Type.STRING, P.Text, D0("Unknown")),
        Column("subname", Type.STRING, P.Text, D0("")),
        Column("display_id", Type.U32, P.Integer, D0(0)),
        Column("scale", Type.F32, P.Real, D0(1.0)),
        Column("min_level", Type.U16, P.SmallInt, D0(1)),
        Column("max_level", Type.U16, P.SmallInt, D0(1)),
        Column("armor_mod", Type.F32, P.Real, D0(1.0)),
        Column("health_mod", Type.F32, P.Real, D0(1.0)),
        Column("resource_mod", Type.F32, P.Real, D0(1.0)),
        Column("damage_mod", Type.F32, P.Real, D0(1.0)),
        Column("script_name", Type.STRING, P.Text, D0(""))
    }),

    Table("world", "creatures",
    {
        Column("id", Type.U64, P.BigInt, I),
        Column("template_id", Type.U32, P.Integer),
        Column("display_id", Type.U32, P.Integer),
        Column("map_id", Type.U32, P.Integer),
        Column("position_x", Type.F32, P.Real),
        Column("position_y", Type.F32, P.Real),
        Column("position_z", Type.F32, P.Real),
        Column("position_o", Type.F32, P.Real),
        Column("script_name", Type.STRING, P.Text, D0(""))
    }, { indexes = { P.Index("creatures_map_id_idx", { P.Asc("mapId") }, { persistentId = "postgres.creatures.map_id_idx" }) },
        queries = { P.Query("ByMap", { "mapId" }, { persistentId = "postgres.creatures.by_map" }) },
        operations = { P.Delete("Delete", { "id" }, { persistentId = "postgres.creatures.delete" }) } }),

    Table("world", "item_stat_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("stat_type_1", Type.U16, P.SmallInt, D0(0)), Column("stat_type_2", Type.U16, P.SmallInt, D0(0)),
        Column("stat_type_3", Type.U16, P.SmallInt, D0(0)), Column("stat_type_4", Type.U16, P.SmallInt, D0(0)),
        Column("stat_type_5", Type.U16, P.SmallInt, D0(0)), Column("stat_type_6", Type.U16, P.SmallInt, D0(0)),
        Column("stat_type_7", Type.U16, P.SmallInt, D0(0)), Column("stat_type_8", Type.U16, P.SmallInt, D0(0)),
        Column("stat_value_1", Type.I32, P.Integer, D0(0)), Column("stat_value_2", Type.I32, P.Integer, D0(0)),
        Column("stat_value_3", Type.I32, P.Integer, D0(0)), Column("stat_value_4", Type.I32, P.Integer, D0(0)),
        Column("stat_value_5", Type.I32, P.Integer, D0(0)), Column("stat_value_6", Type.I32, P.Integer, D0(0)),
        Column("stat_value_7", Type.I32, P.Integer, D0(0)), Column("stat_value_8", Type.I32, P.Integer, D0(0))
    }, { operations = { Upsert("item_stat_templates",
        { "id", "statType1", "statType2", "statType3", "statType4", "statType5", "statType6", "statType7", "statType8",
          "statValue1", "statValue2", "statValue3", "statValue4", "statValue5", "statValue6", "statValue7", "statValue8" },
        { "statType1", "statType2", "statType3", "statType4", "statType5", "statType6", "statType7", "statType8",
          "statValue1", "statValue2", "statValue3", "statValue4", "statValue5", "statValue6", "statValue7", "statValue8" }) } }),

    Table("world", "item_armor_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("equip_type", Type.U16, P.SmallInt, D0(0)),
        Column("bonus_armor", Type.U32, P.Integer, D0(0))
    }, { operations = { Upsert("item_armor_templates", { "id", "equipType", "bonusArmor" }, { "equipType", "bonusArmor" }) } }),

    Table("world", "item_weapon_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("weapon_style", Type.U16, P.SmallInt, D0(0)),
        Column("min_damage", Type.U32, P.Integer, D0(0)),
        Column("max_damage", Type.U32, P.Integer, D0(0)),
        Column("speed", Type.F32, P.Real, D0(1.0))
    }, { operations = { Upsert("item_weapon_templates", { "id", "weaponStyle", "minDamage", "maxDamage", "speed" },
        { "weaponStyle", "minDamage", "maxDamage", "speed" }) } }),

    Table("world", "item_shield_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("bonus_armor", Type.U32, P.Integer, D0(0)),
        Column("block", Type.U32, P.Integer, D0(0))
    }, { operations = { Upsert("item_shield_templates", { "id", "bonusArmor", "block" }, { "bonusArmor", "block" }) } }),

    Table("world", "item_templates",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("display_id", Type.U32, P.Integer, D0(0)),
        Column("bind", Type.U16, P.SmallInt, D0(0)),
        Column("rarity", Type.U16, P.SmallInt, D0(0)),
        Column("category", Type.U16, P.SmallInt, D0(1)),
        Column("type", Type.U16, P.SmallInt, D0(1)),
        Column("virtual_level", Type.U16, P.SmallInt, D0(0)),
        Column("required_level", Type.U16, P.SmallInt, D0(0)),
        Column("durability", Type.U32, P.Integer, D0(0)),
        Column("icon_id", Type.U32, P.Integer, D0(0)),
        Column("name", Type.STRING, P.Text),
        Column("description", Type.STRING, P.Text, D0("")),
        Column("armor", Type.U32, P.Integer, D0(0)),
        Column("stat_template_id", Type.U32, P.Integer, D0(0)),
        Column("armor_template_id", Type.U32, P.Integer, D0(0)),
        Column("weapon_template_id", Type.U32, P.Integer, D0(0)),
        Column("shield_template_id", Type.U32, P.Integer, D0(0))
    }, { operations = { Upsert("item_templates",
        { "id", "displayId", "bind", "rarity", "category", "type", "virtualLevel", "requiredLevel", "durability", "iconId",
          "name", "description", "armor", "statTemplateId", "armorTemplateId", "weaponTemplateId", "shieldTemplateId" },
        { "displayId", "bind", "rarity", "category", "type", "virtualLevel", "requiredLevel", "durability", "iconId",
          "name", "description", "armor", "statTemplateId", "armorTemplateId", "weaponTemplateId", "shieldTemplateId" }) } }),

    Table("world", "proximity_triggers",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("name", Type.STRING, P.Text, D0("")),
        Column("flags", Type.U16, P.SmallInt, D0(0)),
        Column("map_id", Type.U32, P.Integer, D0(0)),
        Column("position_x", Type.F32, P.Real, D0(0.0)), Column("position_y", Type.F32, P.Real, D0(0.0)),
        Column("position_z", Type.F32, P.Real, D0(0.0)), Column("extents_x", Type.F32, P.Real, D0(1.0)),
        Column("extents_y", Type.F32, P.Real, D0(1.0)), Column("extents_z", Type.F32, P.Real, D0(1.0))
    }, { indexes = { P.Index("proximity_triggers_map_id_idx", { P.Asc("mapId") }, { persistentId = "postgres.proximity_triggers.map_id_idx" }) },
        queries = { P.Query("ByMap", { "mapId" }, { persistentId = "postgres.proximity_triggers.by_map" }) },
        operations = { P.Delete("Delete", { "id" }, { persistentId = "postgres.proximity_triggers.delete" }) } }),

    Table("world", "maps",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("flags", Type.U32, P.Integer, D0(0)),
        Column("internal_name", Type.STRING, P.Text, D0("")),
        Column("name", Type.STRING, P.Text, D0("")),
        Column("type", Type.U16, P.SmallInt, D0(0)),
        Column("max_players", Type.U16, P.SmallInt, D0(0))
    }, { operations = { Upsert("maps", { "id", "flags", "internalName", "name", "type", "maxPlayers" },
        { "flags", "internalName", "name", "type", "maxPlayers" }) } }),

    Table("world", "map_locations",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("name", Type.STRING, P.Text, D0("")),
        Column("map_id", Type.U32, P.Integer),
        Column("position_x", Type.F32, P.Real, D0(0.0)), Column("position_y", Type.F32, P.Real, D0(0.0)),
        Column("position_z", Type.F32, P.Real, D0(0.0)), Column("position_o", Type.F32, P.Real, D0(0.0))
    }, { constraints = { ForeignKey("map_locations", "map_locations_map_fk", { "map_id" }, "maps", { "id" }) },
        operations = {
            Upsert("map_locations", { "id", "name", "mapId", "positionX", "positionY", "positionZ", "positionO" },
                { "name", "mapId", "positionX", "positionY", "positionZ", "positionO" }),
            P.Delete("Delete", { "id" }, { persistentId = "postgres.map_locations.delete" })
        } }),

    Table("world", "spells",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("name", Type.STRING, P.Text, D0("")),
        Column("description", Type.STRING, P.Text, D0("")),
        Column("aura_description", Type.STRING, P.Text, D0("")),
        Column("icon_id", Type.U32, P.Integer, D0(0)),
        Column("cast_time", Type.F32, P.Real, D0(0.0)),
        Column("cooldown", Type.F32, P.Real, D0(0.0)),
        Column("duration", Type.F32, P.Real, D0(0.0))
    }, { operations = { Upsert("spells", { "id", "name", "description", "auraDescription", "iconId", "castTime", "cooldown", "duration" },
        { "name", "description", "auraDescription", "iconId", "castTime", "cooldown", "duration" }) } }),

    Table("world", "spell_effects",
    {
        Column("id", Type.U32, P.Integer, I), Column("spell_id", Type.U32, P.Integer),
        Column("effect_priority", Type.U16, P.SmallInt, D0(0)), Column("effect_type", Type.U16, P.SmallInt),
        Column("effect_value_1", Type.I32, P.Integer, D0(0)), Column("effect_value_2", Type.I32, P.Integer, D0(0)),
        Column("effect_value_3", Type.I32, P.Integer, D0(0)), Column("effect_misc_value_1", Type.I32, P.Integer, D0(0)),
        Column("effect_misc_value_2", Type.I32, P.Integer, D0(0)), Column("effect_misc_value_3", Type.I32, P.Integer, D0(0))
    }, { orderBy = { P.Asc("spellId"), P.Desc("effectPriority") }, operations = { Upsert("spell_effects",
        { "id", "spellId", "effectPriority", "effectType", "effectValue1", "effectValue2", "effectValue3", "effectMiscValue1", "effectMiscValue2", "effectMiscValue3" },
        { "spellId", "effectPriority", "effectType", "effectValue1", "effectValue2", "effectValue3", "effectMiscValue1", "effectMiscValue2", "effectMiscValue3" }) } }),

    Table("world", "spell_proc_data",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("phase_mask", Type.I32, P.Integer, D0(-1)),
        Column("type_mask", Type.I64, P.BigInt, D0(-1)),
        Column("hit_mask", Type.I64, P.BigInt, D0(-1)),
        Column("flags", Type.I64, P.BigInt, D0(0)),
        Column("procs_per_min", Type.F32, P.Real, D0(1.0)),
        Column("chance_to_proc", Type.F32, P.Real, D0(0.0)),
        Column("internal_cooldown_ms", Type.I32, P.Integer, D0(0)),
        Column("charges", Type.I32, P.Integer, D0(-1))
    }, { operations = { Upsert("spell_proc_data", { "id", "phaseMask", "typeMask", "hitMask", "flags", "procsPerMin", "chanceToProc", "internalCooldownMs", "charges" },
        { "phaseMask", "typeMask", "hitMask", "flags", "procsPerMin", "chanceToProc", "internalCooldownMs", "charges" }) } }),

    Table("world", "spell_proc_link",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("spell_id", Type.U32, P.Integer),
        Column("effect_mask", Type.I64, P.BigInt, D0(-1)),
        Column("proc_data_id", Type.U32, P.Integer)
    }, { orderBy = { P.Asc("spellId"), P.Asc("procDataId") }, operations = { Upsert("spell_proc_link",
        { "id", "spellId", "effectMask", "procDataId" }, { "spellId", "effectMask", "procDataId" }) } })
}
