-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "world",
    contentHash = "cf5fcf836894b1c99c7d26acab7065b4bd15de828e310963dab7663ff2df3b8d",
    id = "0005_add_creature_faction_policies",
    name = "add_creature_faction_policies",
    operations = {
        [1] = {
            kind = "addColumn",
            persistentId = "postgres.creature_templates.aggression_policy",
            sql = "ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"aggression_policy\" smallint DEFAULT 1 NOT NULL;",
        },
        [2] = {
            kind = "addColumn",
            persistentId = "postgres.creature_templates.assistance_policy",
            sql = "ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"assistance_policy\" smallint DEFAULT 0 NOT NULL;",
        },
        [3] = {
            kind = "addColumn",
            persistentId = "postgres.creature_templates.assistance_range",
            sql = "ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"assistance_range\" smallint DEFAULT 20 NOT NULL;",
        },
    },
    parentHash = "8ed32cbdd1160252bdef8f511e8d88f3f38d086f16e9bb487edb42e1803830ef",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"aggression_policy\" smallint DEFAULT 1 NOT NULL;\
ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"assistance_policy\" smallint DEFAULT 0 NOT NULL;\
ALTER TABLE \"public\".\"creature_templates\" ADD COLUMN \"assistance_range\" smallint DEFAULT 20 NOT NULL;\
COMMIT;\
",
    targetHash = "d5c7d885be2c04d4a188fb5fc189c87ec2cfd62689a9485b163a312058954183",
    transactional = true,
}
