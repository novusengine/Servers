-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "world",
    contentHash = "43b7c205a7db8707b566764b504e1b101039e2ea3bffa2c2f556e3d166035cba",
    id = "0003_move_permissions_to_auth",
    name = "move_permissions_to_auth",
    operations = {
        [1] = {
            kind = "explicitSql",
            persistentId = "explicit.move_permissions_to_auth",
            sql = "DROP TABLE \"public\".\"permission_group_data\";\
DROP TABLE \"public\".\"permission_groups\";\
DROP TABLE \"public\".\"permissions\";\
",
        },
    },
    parentHash = "3bb27092864fdecadd92c0b83014affd6b3d9b416da475a9dc2b2d00472e6a08",
    sql = "BEGIN;\
DROP TABLE \"public\".\"permission_group_data\";\
DROP TABLE \"public\".\"permission_groups\";\
DROP TABLE \"public\".\"permissions\";\
\
COMMIT;\
",
    targetHash = "73f9f6c438618662fef32b5b414bee5d3b9f9daf8feb2bc54221f04afe75ade1",
    transactional = true,
}
