-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "character",
    format = 3,
    hash = "7906646833f1a5815ae1ee1f03ae61c117723fb7125d37a30c92d627885bc25f",
    schemas = {
        public = true,
    },
    tables = {
        ["postgres.character_currency"] = {
            constraints = {
                ["postgres.character_currency.character_currency_character_fk"] = {
                    kind = "foreignKey",
                    name = "character_currency_character_fk",
                    persistentId = "postgres.character_currency.character_currency_character_fk",
                    signature = "foreignKey|character_currency_character_fk|postgres.character_currency.character_id:asc||nil:nil||postgres.characters|postgres.characters.id:asc|noAction|cascade",
                },
                ["postgres.character_currency.character_currency_key"] = {
                    kind = "unique",
                    name = "character_currency_key",
                    persistentId = "postgres.character_currency.character_currency_key",
                    signature = "unique|character_currency_key|postgres.character_currency.character_id:asc,postgres.character_currency.currency_id:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.character_currency.character_id"] = {
                    columnName = "character_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.character_currency.character_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_currency.currency_id"] = {
                    columnName = "currency_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.character_currency.currency_id",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
                ["postgres.character_currency.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.character_currency.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_currency.value"] = {
                    columnName = "value",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.character_currency.value",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.character_currency",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.character_currency.id",
                    },
                },
                name = "character_currency_pkey",
                persistentId = "postgres.character_currency.pk",
                signature = "character_currency_pkey|postgres.character_currency.id:asc",
            },
            schema = "public",
            tableName = "character_currency",
        },
        ["postgres.character_items"] = {
            constraints = {
                ["postgres.character_items.character_items_character_container_slot_key"] = {
                    kind = "unique",
                    name = "character_items_character_container_slot_key",
                    persistentId = "postgres.character_items.character_items_character_container_slot_key",
                    signature = "unique|character_items_character_container_slot_key|postgres.character_items.character_id:asc,postgres.character_items.container:asc,postgres.character_items.slot:asc||nil:nil|||||",
                },
                ["postgres.character_items.character_items_character_fk"] = {
                    kind = "foreignKey",
                    name = "character_items_character_fk",
                    persistentId = "postgres.character_items.character_items_character_fk",
                    signature = "foreignKey|character_items_character_fk|postgres.character_items.character_id:asc||nil:nil||postgres.characters|postgres.characters.id:asc|noAction|cascade",
                },
                ["postgres.character_items.character_items_owner_item_fk"] = {
                    kind = "foreignKey",
                    name = "character_items_owner_item_fk",
                    persistentId = "postgres.character_items.character_items_owner_item_fk",
                    signature = "foreignKey|character_items_owner_item_fk|postgres.character_items.character_id:asc,postgres.character_items.item_instance_id:asc||nil:nil||postgres.item_instances|postgres.item_instances.owner_id:asc,postgres.item_instances.id:asc|noAction|cascade",
                },
                ["postgres.character_items.unique_item_instance_id"] = {
                    kind = "unique",
                    name = "unique_item_instance_id",
                    persistentId = "postgres.character_items.unique_item_instance_id",
                    signature = "unique|unique_item_instance_id|postgres.character_items.item_instance_id:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.character_items.character_id"] = {
                    columnName = "character_id",
                    hasDefault = false,
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.character_items.character_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_items.container"] = {
                    columnName = "container",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.character_items.container",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_items.item_instance_id"] = {
                    columnName = "item_instance_id",
                    hasDefault = false,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.character_items.item_instance_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_items.slot"] = {
                    columnName = "slot",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.character_items.slot",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.character_items",
            schema = "public",
            tableName = "character_items",
        },
        ["postgres.character_permission_groups"] = {
            constraints = {
                ["postgres.character_permission_groups.character_permission_group_key"] = {
                    kind = "unique",
                    name = "character_permission_group_key",
                    persistentId = "postgres.character_permission_groups.character_permission_group_key",
                    signature = "unique|character_permission_group_key|postgres.character_permission_groups.character_id:asc,postgres.character_permission_groups.permission_group_id:asc||nil:nil|||||",
                },
                ["postgres.character_permission_groups.character_permission_groups_character_fk"] = {
                    kind = "foreignKey",
                    name = "character_permission_groups_character_fk",
                    persistentId = "postgres.character_permission_groups.character_permission_groups_character_fk",
                    signature = "foreignKey|character_permission_groups_character_fk|postgres.character_permission_groups.character_id:asc||nil:nil||postgres.characters|postgres.characters.id:asc|noAction|cascade",
                },
            },
            fields = {
                ["postgres.character_permission_groups.character_id"] = {
                    columnName = "character_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.character_permission_groups.character_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_permission_groups.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.character_permission_groups.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_permission_groups.permission_group_id"] = {
                    columnName = "permission_group_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.character_permission_groups.permission_group_id",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.character_permission_groups",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.character_permission_groups.id",
                    },
                },
                name = "character_permission_groups_pkey",
                persistentId = "postgres.character_permission_groups.pk",
                signature = "character_permission_groups_pkey|postgres.character_permission_groups.id:asc",
            },
            schema = "public",
            tableName = "character_permission_groups",
        },
        ["postgres.character_permissions"] = {
            constraints = {
                ["postgres.character_permissions.character_permission_key"] = {
                    kind = "unique",
                    name = "character_permission_key",
                    persistentId = "postgres.character_permissions.character_permission_key",
                    signature = "unique|character_permission_key|postgres.character_permissions.character_id:asc,postgres.character_permissions.permission_id:asc||nil:nil|||||",
                },
                ["postgres.character_permissions.character_permissions_character_fk"] = {
                    kind = "foreignKey",
                    name = "character_permissions_character_fk",
                    persistentId = "postgres.character_permissions.character_permissions_character_fk",
                    signature = "foreignKey|character_permissions_character_fk|postgres.character_permissions.character_id:asc||nil:nil||postgres.characters|postgres.characters.id:asc|noAction|cascade",
                },
            },
            fields = {
                ["postgres.character_permissions.character_id"] = {
                    columnName = "character_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.character_permissions.character_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_permissions.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.character_permissions.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.character_permissions.permission_id"] = {
                    columnName = "permission_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.character_permissions.permission_id",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.character_permissions",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.character_permissions.id",
                    },
                },
                name = "character_permissions_pkey",
                persistentId = "postgres.character_permissions.pk",
                signature = "character_permissions_pkey|postgres.character_permissions.id:asc",
            },
            schema = "public",
            tableName = "character_permissions",
        },
        ["postgres.characters"] = {
            constraints = {
                ["postgres.characters.characters_unique_name"] = {
                    kind = "unique",
                    name = "characters_unique_name",
                    persistentId = "postgres.characters.characters_unique_name",
                    signature = "unique|characters_unique_name|postgres.characters.name:asc||nil:nil|||||",
                },
                ["postgres.characters.name_length"] = {
                    kind = "rawCheck",
                    name = "characters_name_length",
                    persistentId = "postgres.characters.name_length",
                    signature = "rawCheck|characters_name_length|||nil:nil|char_length(name) >= 2||||",
                },
            },
            fields = {
                ["postgres.characters.account_id"] = {
                    columnName = "account_id",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.characters.account_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.characters.experience_points"] = {
                    columnName = "experience_points",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 10,
                    persistentId = "postgres.characters.experience_points",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.characters.flags"] = {
                    columnName = "flags",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 7,
                    persistentId = "postgres.characters.flags",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.characters.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.characters.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.characters.level"] = {
                    columnName = "level",
                    defaultValue = 1,
                    hasDefault = true,
                    nullable = false,
                    order = 9,
                    persistentId = "postgres.characters.level",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
                ["postgres.characters.level_time"] = {
                    columnName = "level_time",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 5,
                    persistentId = "postgres.characters.level_time",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.characters.logout_time"] = {
                    columnName = "logout_time",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 6,
                    persistentId = "postgres.characters.logout_time",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.characters.map_id"] = {
                    columnName = "map_id",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 11,
                    persistentId = "postgres.characters.map_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.characters.name"] = {
                    columnName = "name",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.characters.name",
                    postgresType = {
                        name = "varchar(12)",
                        sql = "varchar(12)",
                    },
                },
                ["postgres.characters.position_o"] = {
                    columnName = "position_o",
                    defaultValue = 0.0,
                    hasDefault = true,
                    nullable = false,
                    order = 15,
                    persistentId = "postgres.characters.position_o",
                    postgresType = {
                        name = "real",
                        sql = "real",
                    },
                },
                ["postgres.characters.position_x"] = {
                    columnName = "position_x",
                    defaultValue = 0.0,
                    hasDefault = true,
                    nullable = false,
                    order = 12,
                    persistentId = "postgres.characters.position_x",
                    postgresType = {
                        name = "real",
                        sql = "real",
                    },
                },
                ["postgres.characters.position_y"] = {
                    columnName = "position_y",
                    defaultValue = 0.0,
                    hasDefault = true,
                    nullable = false,
                    order = 13,
                    persistentId = "postgres.characters.position_y",
                    postgresType = {
                        name = "real",
                        sql = "real",
                    },
                },
                ["postgres.characters.position_z"] = {
                    columnName = "position_z",
                    defaultValue = 0.0,
                    hasDefault = true,
                    nullable = false,
                    order = 14,
                    persistentId = "postgres.characters.position_z",
                    postgresType = {
                        name = "real",
                        sql = "real",
                    },
                },
                ["postgres.characters.race_gender_class"] = {
                    columnName = "race_gender_class",
                    hasDefault = false,
                    nullable = false,
                    order = 8,
                    persistentId = "postgres.characters.race_gender_class",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
                ["postgres.characters.total_time"] = {
                    columnName = "total_time",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.characters.total_time",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
            },
            indexes = {
                ["postgres.characters.account_id_id_idx"] = {
                    fields = {
                        [1] = {
                            columnName = "account_id",
                            direction = "asc",
                            persistentId = "postgres.characters.account_id",
                        },
                        [2] = {
                            columnName = "id",
                            direction = "asc",
                            persistentId = "postgres.characters.id",
                        },
                    },
                    name = "characters_account_id_id_idx",
                    persistentId = "postgres.characters.account_id_id_idx",
                    signature = "characters_account_id_id_idx|false|postgres.characters.account_id:asc,postgres.characters.id:asc",
                    unique = false,
                },
            },
            persistentId = "postgres.characters",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.characters.id",
                    },
                },
                name = "characters_pkey",
                persistentId = "postgres.characters.pk",
                signature = "characters_pkey|postgres.characters.id:asc",
            },
            schema = "public",
            tableName = "characters",
        },
        ["postgres.item_instances"] = {
            constraints = {
                ["postgres.item_instances.item_instances_owner_fk"] = {
                    kind = "foreignKey",
                    name = "item_instances_owner_fk",
                    persistentId = "postgres.item_instances.item_instances_owner_fk",
                    signature = "foreignKey|item_instances_owner_fk|postgres.item_instances.owner_id:asc||nil:nil||postgres.characters|postgres.characters.id:asc|noAction|cascade",
                },
                ["postgres.item_instances.item_instances_owner_id_id_key"] = {
                    kind = "unique",
                    name = "item_instances_owner_id_id_key",
                    persistentId = "postgres.item_instances.item_instances_owner_id_id_key",
                    signature = "unique|item_instances_owner_id_id_key|postgres.item_instances.owner_id:asc,postgres.item_instances.id:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.item_instances.count"] = {
                    columnName = "count",
                    hasDefault = false,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.item_instances.count",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
                ["postgres.item_instances.durability"] = {
                    columnName = "durability",
                    hasDefault = false,
                    nullable = false,
                    order = 5,
                    persistentId = "postgres.item_instances.durability",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
                ["postgres.item_instances.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.item_instances.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.item_instances.item_id"] = {
                    columnName = "item_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.item_instances.item_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.item_instances.owner_id"] = {
                    columnName = "owner_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.item_instances.owner_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
            },
            indexes = {
                ["postgres.item_instances.owner_id_id_idx"] = {
                    fields = {
                        [1] = {
                            columnName = "owner_id",
                            direction = "asc",
                            persistentId = "postgres.item_instances.owner_id",
                        },
                        [2] = {
                            columnName = "id",
                            direction = "asc",
                            persistentId = "postgres.item_instances.id",
                        },
                    },
                    name = "item_instances_owner_id_id_idx",
                    persistentId = "postgres.item_instances.owner_id_id_idx",
                    signature = "item_instances_owner_id_id_idx|false|postgres.item_instances.owner_id:asc,postgres.item_instances.id:asc",
                    unique = false,
                },
            },
            persistentId = "postgres.item_instances",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.item_instances.id",
                    },
                },
                name = "item_instances_pkey",
                persistentId = "postgres.item_instances.pk",
                signature = "item_instances_pkey|postgres.item_instances.id:asc",
            },
            schema = "public",
            tableName = "item_instances",
        },
    },
}
