-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "auth",
    format = 3,
    hash = "0d9fe7fdd318ee4625ab9e4edaec2c1ecff1fa91f0db4f79c752b24996c583be",
    schemas = {
        public = true,
    },
    tables = {
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
    },
}
