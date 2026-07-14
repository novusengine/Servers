-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "auth",
    format = 3,
    hash = "48dcc31dc0a6c27688d72eae18bf0d09bc703e4337d6cc6711e91e0040a6676b",
    schemas = {
        public = true,
    },
    tables = {
        ["postgres.account_permission_groups"] = {
            constraints = {
                ["postgres.account_permission_groups.account_permission_group_key"] = {
                    kind = "unique",
                    name = "account_permission_group_key",
                    persistentId = "postgres.account_permission_groups.account_permission_group_key",
                    signature = "unique|account_permission_group_key|postgres.account_permission_groups.account_id:asc,postgres.account_permission_groups.permission_group_id:asc||nil:nil|||||",
                },
                ["postgres.account_permission_groups.account_permission_groups_account_fk"] = {
                    kind = "foreignKey",
                    name = "account_permission_groups_account_fk",
                    persistentId = "postgres.account_permission_groups.account_permission_groups_account_fk",
                    signature = "foreignKey|account_permission_groups_account_fk|postgres.account_permission_groups.account_id:asc||nil:nil||postgres.accounts|postgres.accounts.id:asc|noAction|cascade",
                },
                ["postgres.account_permission_groups.account_permission_groups_group_fk"] = {
                    kind = "foreignKey",
                    name = "account_permission_groups_group_fk",
                    persistentId = "postgres.account_permission_groups.account_permission_groups_group_fk",
                    signature = "foreignKey|account_permission_groups_group_fk|postgres.account_permission_groups.permission_group_id:asc||nil:nil||postgres.permission_groups|postgres.permission_groups.id:asc|noAction|cascade",
                },
            },
            fields = {
                ["postgres.account_permission_groups.account_id"] = {
                    columnName = "account_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.account_permission_groups.account_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.account_permission_groups.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.account_permission_groups.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.account_permission_groups.permission_group_id"] = {
                    columnName = "permission_group_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.account_permission_groups.permission_group_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.account_permission_groups",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.account_permission_groups.id",
                    },
                },
                name = "account_permission_groups_pkey",
                persistentId = "postgres.account_permission_groups.pk",
                signature = "account_permission_groups_pkey|postgres.account_permission_groups.id:asc",
            },
            schema = "public",
            tableName = "account_permission_groups",
        },
        ["postgres.account_permissions"] = {
            constraints = {
                ["postgres.account_permissions.account_permission_key"] = {
                    kind = "unique",
                    name = "account_permission_key",
                    persistentId = "postgres.account_permissions.account_permission_key",
                    signature = "unique|account_permission_key|postgres.account_permissions.account_id:asc,postgres.account_permissions.permission_id:asc||nil:nil|||||",
                },
                ["postgres.account_permissions.account_permissions_account_fk"] = {
                    kind = "foreignKey",
                    name = "account_permissions_account_fk",
                    persistentId = "postgres.account_permissions.account_permissions_account_fk",
                    signature = "foreignKey|account_permissions_account_fk|postgres.account_permissions.account_id:asc||nil:nil||postgres.accounts|postgres.accounts.id:asc|noAction|cascade",
                },
                ["postgres.account_permissions.account_permissions_permission_fk"] = {
                    kind = "foreignKey",
                    name = "account_permissions_permission_fk",
                    persistentId = "postgres.account_permissions.account_permissions_permission_fk",
                    signature = "foreignKey|account_permissions_permission_fk|postgres.account_permissions.permission_id:asc||nil:nil||postgres.permissions|postgres.permissions.id:asc|noAction|cascade",
                },
            },
            fields = {
                ["postgres.account_permissions.account_id"] = {
                    columnName = "account_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.account_permissions.account_id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.account_permissions.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.account_permissions.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.account_permissions.permission_id"] = {
                    columnName = "permission_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.account_permissions.permission_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.account_permissions.value"] = {
                    columnName = "value",
                    defaultValue = 1,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.account_permissions.value",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.account_permissions",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.account_permissions.id",
                    },
                },
                name = "account_permissions_pkey",
                persistentId = "postgres.account_permissions.pk",
                signature = "account_permissions_pkey|postgres.account_permissions.id:asc",
            },
            schema = "public",
            tableName = "account_permissions",
        },
        ["postgres.accounts"] = {
            constraints = {
                ["postgres.accounts.unique_name"] = {
                    kind = "unique",
                    name = "unique_name",
                    persistentId = "postgres.accounts.unique_name",
                    signature = "unique|unique_name|postgres.accounts.name:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.accounts.blob"] = {
                    columnName = "blob",
                    hasDefault = false,
                    nullable = true,
                    order = 7,
                    persistentId = "postgres.accounts.blob",
                    postgresType = {
                        name = "bytea",
                        sql = "bytea",
                    },
                },
                ["postgres.accounts.email"] = {
                    columnName = "email",
                    defaultValue = "",
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.accounts.email",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.accounts.flags"] = {
                    columnName = "flags",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.accounts.flags",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.accounts.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.accounts.id",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.accounts.last_login_timestamp"] = {
                    columnName = "last_login_timestamp",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 6,
                    persistentId = "postgres.accounts.last_login_timestamp",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.accounts.name"] = {
                    columnName = "name",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.accounts.name",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.accounts.registration_timestamp"] = {
                    columnName = "registration_timestamp",
                    hasDefault = false,
                    nullable = false,
                    order = 5,
                    persistentId = "postgres.accounts.registration_timestamp",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.accounts",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.accounts.id",
                    },
                },
                name = "accounts_pkey",
                persistentId = "postgres.accounts.pk",
                signature = "accounts_pkey|postgres.accounts.id:asc",
            },
            schema = "public",
            tableName = "accounts",
        },
        ["postgres.permission_group_data"] = {
            constraints = {
                ["postgres.permission_group_data.permission_group_data_group_fk"] = {
                    kind = "foreignKey",
                    name = "permission_group_data_group_fk",
                    persistentId = "postgres.permission_group_data.permission_group_data_group_fk",
                    signature = "foreignKey|permission_group_data_group_fk|postgres.permission_group_data.group_id:asc||nil:nil||postgres.permission_groups|postgres.permission_groups.id:asc|noAction|cascade",
                },
                ["postgres.permission_group_data.permission_group_data_key"] = {
                    kind = "unique",
                    name = "permission_group_data_key",
                    persistentId = "postgres.permission_group_data.permission_group_data_key",
                    signature = "unique|permission_group_data_key|postgres.permission_group_data.group_id:asc,postgres.permission_group_data.permission_id:asc||nil:nil|||||",
                },
                ["postgres.permission_group_data.permission_group_data_permission_fk"] = {
                    kind = "foreignKey",
                    name = "permission_group_data_permission_fk",
                    persistentId = "postgres.permission_group_data.permission_group_data_permission_fk",
                    signature = "foreignKey|permission_group_data_permission_fk|postgres.permission_group_data.permission_id:asc||nil:nil||postgres.permissions|postgres.permissions.id:asc|noAction|cascade",
                },
            },
            fields = {
                ["postgres.permission_group_data.group_id"] = {
                    columnName = "group_id",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.permission_group_data.group_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.permission_group_data.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.permission_group_data.id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.permission_group_data.permission_id"] = {
                    columnName = "permission_id",
                    hasDefault = false,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.permission_group_data.permission_id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.permission_group_data.value"] = {
                    columnName = "value",
                    defaultValue = 1,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.permission_group_data.value",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.permission_group_data",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.permission_group_data.id",
                    },
                },
                name = "permission_group_data_pkey",
                persistentId = "postgres.permission_group_data.pk",
                signature = "permission_group_data_pkey|postgres.permission_group_data.id:asc",
            },
            schema = "public",
            tableName = "permission_group_data",
        },
        ["postgres.permission_groups"] = {
            constraints = {
                ["postgres.permission_groups.permission_groups_unique_name"] = {
                    kind = "unique",
                    name = "permission_groups_unique_name",
                    persistentId = "postgres.permission_groups.permission_groups_unique_name",
                    signature = "unique|permission_groups_unique_name|postgres.permission_groups.name:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.permission_groups.description"] = {
                    columnName = "description",
                    defaultValue = "",
                    hasDefault = true,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.permission_groups.description",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.permission_groups.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.permission_groups.id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.permission_groups.is_system"] = {
                    columnName = "is_system",
                    defaultValue = false,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.permission_groups.is_system",
                    postgresType = {
                        name = "boolean",
                        sql = "boolean",
                    },
                },
                ["postgres.permission_groups.name"] = {
                    columnName = "name",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.permission_groups.name",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.permission_groups",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.permission_groups.id",
                    },
                },
                name = "permission_groups_pkey",
                persistentId = "postgres.permission_groups.pk",
                signature = "permission_groups_pkey|postgres.permission_groups.id:asc",
            },
            schema = "public",
            tableName = "permission_groups",
        },
        ["postgres.permissions"] = {
            constraints = {
                ["postgres.permissions.permissions_unique_name"] = {
                    kind = "unique",
                    name = "permissions_unique_name",
                    persistentId = "postgres.permissions.permissions_unique_name",
                    signature = "unique|permissions_unique_name|postgres.permissions.name:asc||nil:nil|||||",
                },
            },
            fields = {
                ["postgres.permissions.category"] = {
                    columnName = "category",
                    defaultValue = "",
                    hasDefault = true,
                    nullable = false,
                    order = 3,
                    persistentId = "postgres.permissions.category",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.permissions.default_value"] = {
                    columnName = "default_value",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 6,
                    persistentId = "postgres.permissions.default_value",
                    postgresType = {
                        name = "bigint",
                        sql = "bigint",
                    },
                },
                ["postgres.permissions.description"] = {
                    columnName = "description",
                    defaultValue = "",
                    hasDefault = true,
                    nullable = false,
                    order = 5,
                    persistentId = "postgres.permissions.description",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.permissions.id"] = {
                    columnName = "id",
                    hasDefault = false,
                    identity = "byDefault",
                    nullable = false,
                    order = 1,
                    persistentId = "postgres.permissions.id",
                    postgresType = {
                        name = "integer",
                        sql = "integer",
                    },
                },
                ["postgres.permissions.name"] = {
                    columnName = "name",
                    hasDefault = false,
                    nullable = false,
                    order = 2,
                    persistentId = "postgres.permissions.name",
                    postgresType = {
                        name = "text",
                        sql = "text",
                    },
                },
                ["postgres.permissions.value_kind"] = {
                    columnName = "value_kind",
                    defaultValue = 0,
                    hasDefault = true,
                    nullable = false,
                    order = 4,
                    persistentId = "postgres.permissions.value_kind",
                    postgresType = {
                        name = "smallint",
                        sql = "smallint",
                    },
                },
            },
            indexes = {
            },
            persistentId = "postgres.permissions",
            primaryKey = {
                fields = {
                    [1] = {
                        columnName = "id",
                        direction = "asc",
                        persistentId = "postgres.permissions.id",
                    },
                },
                name = "permissions_pkey",
                persistentId = "postgres.permissions.pk",
                signature = "permissions_pkey|postgres.permissions.id:asc",
            },
            schema = "public",
            tableName = "permissions",
        },
    },
}
