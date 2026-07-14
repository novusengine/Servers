-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "character",
    contentHash = "20c37aebc37361b240f8d47e12dd99ab9b2078520e982eda4bf004b7ea91434e",
    id = "0002_add_permission_values",
    name = "add_permission_values",
    operations = {
        [1] = {
            kind = "addColumn",
            persistentId = "postgres.character_permissions.value",
            sql = "ALTER TABLE \"public\".\"character_permissions\" ADD COLUMN \"value\" bigint DEFAULT 1 NOT NULL;",
        },
    },
    parentHash = "7906646833f1a5815ae1ee1f03ae61c117723fb7125d37a30c92d627885bc25f",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"character_permissions\" ADD COLUMN \"value\" bigint DEFAULT 1 NOT NULL;\
COMMIT;\
",
    targetHash = "9f61dc30f9054f428a26d9d474d23104a3348e235409c0e8bae7d6b502e0d1f3",
    transactional = true,
}
