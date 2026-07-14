-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "character",
    contentHash = "f0ba759561cc6cd137ce38ed9b4a684b67e895b28e2cde1a04608e539833af47",
    id = "0003_expand_permission_ids",
    name = "expand_permission_ids",
    operations = {
        [1] = {
            kind = "widenColumn",
            persistentId = "postgres.character_permission_groups.permission_group_id",
            sql = "ALTER TABLE \"public\".\"character_permission_groups\" ALTER COLUMN \"permission_group_id\" TYPE integer;",
        },
        [2] = {
            kind = "widenColumn",
            persistentId = "postgres.character_permissions.permission_id",
            sql = "ALTER TABLE \"public\".\"character_permissions\" ALTER COLUMN \"permission_id\" TYPE integer;",
        },
    },
    parentHash = "9f61dc30f9054f428a26d9d474d23104a3348e235409c0e8bae7d6b502e0d1f3",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"character_permission_groups\" ALTER COLUMN \"permission_group_id\" TYPE integer;\
ALTER TABLE \"public\".\"character_permissions\" ALTER COLUMN \"permission_id\" TYPE integer;\
COMMIT;\
",
    targetHash = "f590c458be6688f0a1ae5d90eacf1230ba69d13f08118f30becc163a679cffd4",
    transactional = true,
}
