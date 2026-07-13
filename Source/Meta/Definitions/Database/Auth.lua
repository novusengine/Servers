local U = require("PostgresDefinitions")
local D, P, Type = U.Definition, U.Postgres, U.Type
local Column, Table = U.Column, U.Table
local I, D0, N = U.Identity, U.Default, U.Nullable

return D.Definitions
{
    Table("auth", "accounts",
    {
        Column("id", Type.U64, P.BigInt, I),
        Column("flags", Type.U64, P.BigInt, D0(0)),
        Column("name", Type.STRING, P.Text),
        Column("email", Type.STRING, P.Text, D0("")),
        Column("registration_timestamp", Type.U64, P.BigInt),
        Column("last_login_timestamp", Type.U64, P.BigInt, D0(0)),
        Column("blob", Type.BYTEBUFFER, P.Bytea, N)
    }, { constraints = { U.Unique("accounts", "unique_name", { "name" }) },
        queries = { P.Query("ByName", { "name" }, { persistentId = "postgres.accounts.by_name", cardinality = "zeroOrOne" }) } })
}
