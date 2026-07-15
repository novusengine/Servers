-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "world",
    contentHash = "1559324de38610329d8f64567d4313c2c990afd331614a6bc1ccdf3dc29c23b5",
    id = "0006_widen_faction_ids",
    name = "widen_faction_ids",
    operations = {
        [1] = {
            kind = "widenColumn",
            persistentId = "postgres.creature_templates.faction_id",
            sql = "ALTER TABLE \"public\".\"creature_templates\" ALTER COLUMN \"faction_id\" TYPE integer;",
        },
        [2] = {
            kind = "widenColumn",
            persistentId = "postgres.faction_relations.source_faction_id",
            sql = "ALTER TABLE \"public\".\"faction_relations\" ALTER COLUMN \"source_faction_id\" TYPE integer;",
        },
        [3] = {
            kind = "widenColumn",
            persistentId = "postgres.faction_relations.target_faction_id",
            sql = "ALTER TABLE \"public\".\"faction_relations\" ALTER COLUMN \"target_faction_id\" TYPE integer;",
        },
        [4] = {
            kind = "widenColumn",
            persistentId = "postgres.faction_starting_reputations.player_faction_id",
            sql = "ALTER TABLE \"public\".\"faction_starting_reputations\" ALTER COLUMN \"player_faction_id\" TYPE integer;",
        },
        [5] = {
            kind = "widenColumn",
            persistentId = "postgres.faction_starting_reputations.target_faction_id",
            sql = "ALTER TABLE \"public\".\"faction_starting_reputations\" ALTER COLUMN \"target_faction_id\" TYPE integer;",
        },
        [6] = {
            kind = "widenColumn",
            persistentId = "postgres.factions.id",
            sql = "ALTER TABLE \"public\".\"factions\" ALTER COLUMN \"id\" TYPE integer;",
        },
        [7] = {
            kind = "widenColumn",
            persistentId = "postgres.unit_race_factions.faction_id",
            sql = "ALTER TABLE \"public\".\"unit_race_factions\" ALTER COLUMN \"faction_id\" TYPE integer;",
        },
    },
    parentHash = "d5c7d885be2c04d4a188fb5fc189c87ec2cfd62689a9485b163a312058954183",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"creature_templates\" ALTER COLUMN \"faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"faction_relations\" ALTER COLUMN \"source_faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"faction_relations\" ALTER COLUMN \"target_faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"faction_starting_reputations\" ALTER COLUMN \"player_faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"faction_starting_reputations\" ALTER COLUMN \"target_faction_id\" TYPE integer;\
ALTER TABLE \"public\".\"factions\" ALTER COLUMN \"id\" TYPE integer;\
ALTER TABLE \"public\".\"unit_race_factions\" ALTER COLUMN \"faction_id\" TYPE integer;\
COMMIT;\
",
    targetHash = "6d577cbb997ac197d2a49a09638a86a500be929c56afe30a57e80a1a9a22f413",
    transactional = true,
}
