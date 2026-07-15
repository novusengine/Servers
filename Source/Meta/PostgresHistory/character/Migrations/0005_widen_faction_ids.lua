-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "character",
    contentHash = "9035d8de67a8125b9f56a3757a1a42dcd339302c3753aec0f0f2e208a3f8766d",
    id = "0005_widen_faction_ids",
    name = "widen_faction_ids",
    operations = {
        [1] = {
            kind = "widenColumn",
            persistentId = "postgres.character_reputations.faction_id",
            sql = "ALTER TABLE \"public\".\"character_reputations\" ALTER COLUMN \"faction_id\" TYPE integer;",
        },
        [2] = {
            kind = "widenColumn",
            persistentId = "postgres.characters.faction_id",
            sql = "ALTER TABLE \"public\".\"characters\" ALTER COLUMN \"faction_id\" TYPE integer;",
        },
    },
    parentHash = "c1d0742226192cd3a2b86be7096c7b533b9b6bbaee964e05c9be09ce07299018",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"character_reputations\" ALTER COLUMN \"faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"characters\" ALTER COLUMN \"faction_id\" TYPE integer;\
COMMIT;\
",
    targetHash = "a6d476463afa351ff950eb7d0faa415641189738041bf2debedb391dd5d3de82",
    transactional = true,
}
