local U = require("PostgresDefinitions")
local D, P, Type = U.Definition, U.Postgres, U.Type
local Column, Table, Unique, ForeignKey = U.Column, U.Table, U.Unique, U.ForeignKey
local I, D0, N = U.Identity, U.Default, U.Nullable

return D.Definitions
{
    Table("auth", "permissions",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("name", Type.STRING, P.Text),
        Column("category", Type.STRING, P.Text, D0("")),
        Column("value_kind", Type.U16, P.SmallInt, D0(0)),
        Column("description", Type.STRING, P.Text, D0("")),
        Column("default_value", Type.I64, P.BigInt, D0(0))
    }, { constraints = { Unique("permissions", "permissions_unique_name", { "name" }) },
        queries = { P.Query("ByName", { "name" }, { persistentId = "postgres.permissions.by_name", cardinality = "zeroOrOne" }) },
        operations = {
            P.Upsert("Set", { "id", "name", "category", "valueKind", "description", "defaultValue" }, { "id" },
                { "name", "category", "valueKind", "description", "defaultValue" }, { persistentId = "postgres.permissions.set" })
        } }),

    Table("auth", "permission_groups",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("name", Type.STRING, P.Text),
        Column("description", Type.STRING, P.Text, D0("")),
        Column("is_system", Type.BOOL, P.Boolean, D0(false))
    }, { constraints = { Unique("permission_groups", "permission_groups_unique_name", { "name" }) },
        operations = {
            P.Upsert("Set", { "id", "name", "description", "isSystem" }, { "id" },
                { "name", "description", "isSystem" }, { persistentId = "postgres.permission_groups.set" })
        } }),

    Table("auth", "permission_group_data",
    {
        Column("id", Type.U32, P.Integer, I),
        Column("group_id", Type.U32, P.Integer),
        Column("permission_id", Type.U32, P.Integer),
        Column("value", Type.I64, P.BigInt, D0(1))
    }, { constraints =
        {
            Unique("permission_group_data", "permission_group_data_key", { "group_id", "permission_id" }),
            ForeignKey("permission_group_data", "permission_group_data_group_fk", { "group_id" }, "permission_groups", { "id" }, "cascade"),
            ForeignKey("permission_group_data", "permission_group_data_permission_fk", { "permission_id" }, "permissions", { "id" }, "cascade")
        }, operations = {
            P.Upsert("Set", { "groupId", "permissionId", "value" }, { "groupId", "permissionId" }, { "value" },
                { persistentId = "postgres.permission_group_data.set" }),
            P.Delete("DeleteByGroup", { "groupId" }, { persistentId = "postgres.permission_group_data.delete_by_group", cardinality = "zeroOrMore" })
        }
    }),

    Table("auth", "accounts",
    {
        Column("id", Type.U64, P.BigInt, I),
        Column("flags", Type.U64, P.BigInt, D0(0)),
        Column("name", Type.STRING, P.Text),
        Column("email", Type.STRING, P.Text, D0("")),
        Column("registration_timestamp", Type.U64, P.BigInt),
        Column("last_login_timestamp", Type.U64, P.BigInt, D0(0)),
        Column("blob", Type.BYTEBUFFER, P.Bytea, N)
    }, { constraints = { Unique("accounts", "unique_name", { "name" }) },
        queries = { P.Query("ByName", { "name" }, { persistentId = "postgres.accounts.by_name", cardinality = "zeroOrOne" }) } }),

    Table("auth", "account_permissions",
    {
        Column("id", Type.U64, P.BigInt, I),
        Column("account_id", Type.U64, P.BigInt),
        Column("permission_id", Type.U32, P.Integer),
        Column("value", Type.I64, P.BigInt, D0(1))
    }, { constraints =
        {
            Unique("account_permissions", "account_permission_key", { "account_id", "permission_id" }),
            ForeignKey("account_permissions", "account_permissions_account_fk", { "account_id" }, "accounts", { "id" }, "cascade"),
            ForeignKey("account_permissions", "account_permissions_permission_fk", { "permission_id" }, "permissions", { "id" }, "cascade")
        }, queries = {
            P.Query("ByAccount", { "accountId" }, { persistentId = "postgres.account_permissions.by_account", orderBy = { P.Asc("id") } })
        }, operations = {
            P.Upsert("Set", { "accountId", "permissionId", "value" }, { "accountId", "permissionId" }, { "value" }, { persistentId = "postgres.account_permissions.set" }),
            P.Delete("Delete", { "accountId", "permissionId" }, { persistentId = "postgres.account_permissions.delete" })
        }
    }),

    Table("auth", "account_permission_groups",
    {
        Column("id", Type.U64, P.BigInt, I),
        Column("account_id", Type.U64, P.BigInt),
        Column("permission_group_id", Type.U32, P.Integer)
    }, { constraints =
        {
            Unique("account_permission_groups", "account_permission_group_key", { "account_id", "permission_group_id" }),
            ForeignKey("account_permission_groups", "account_permission_groups_account_fk", { "account_id" }, "accounts", { "id" }, "cascade"),
            ForeignKey("account_permission_groups", "account_permission_groups_group_fk", { "permission_group_id" }, "permission_groups", { "id" }, "cascade")
        }, queries = {
            P.Query("ByAccount", { "accountId" }, { persistentId = "postgres.account_permission_groups.by_account", orderBy = { P.Asc("id") } })
        }, operations = {
            P.Upsert("Set", { "accountId", "permissionGroupId" }, { "accountId", "permissionGroupId" }, {}, { persistentId = "postgres.account_permission_groups.set" }),
            P.Delete("Delete", { "accountId", "permissionGroupId" }, { persistentId = "postgres.account_permission_groups.delete" })
        }
    })
}
