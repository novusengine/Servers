-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "auth",
    contentHash = "bc0af3da06e5e3de47797d0557ecbdd4f6e8d6073fe4b7a84c897046253913af",
    id = "0003_seed_initial_permissions",
    name = "seed_initial_permissions",
    operations = {
        [1] = {
            kind = "explicitSql",
            persistentId = "explicit.seed_initial_permissions",
            sql = "INSERT INTO \"public\".\"permissions\" (\"name\")\
VALUES ('visibility.creature.see_despawned')\
ON CONFLICT (\"name\") DO NOTHING;\
",
        },
    },
    parentHash = "be37cd3f464296db566a27a0c02284ce6d7885d42d80c51b1d61188816c45f3d",
    sql = "BEGIN;\
INSERT INTO \"public\".\"permissions\" (\"name\")\
VALUES ('visibility.creature.see_despawned')\
ON CONFLICT (\"name\") DO NOTHING;\
\
COMMIT;\
",
    targetHash = "be37cd3f464296db566a27a0c02284ce6d7885d42d80c51b1d61188816c45f3d",
    transactional = true,
}
