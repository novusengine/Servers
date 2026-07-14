-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "auth",
    contentHash = "072d74335f068d3bcee42f6b8631840a56c9ba964e5895e64e35ca8f66b5b501",
    id = "0005_reserve_system_permission_ids",
    name = "reserve_system_permission_ids",
    operations = {
        [1] = {
            kind = "explicitSql",
            persistentId = "explicit.reserve_system_permission_ids",
            sql = "ALTER SEQUENCE \"public\".\"permissions_id_seq\" AS integer RESTART WITH 1000000;\
ALTER SEQUENCE \"public\".\"permission_groups_id_seq\" AS integer RESTART WITH 1000000;\
",
        },
    },
    parentHash = "48dcc31dc0a6c27688d72eae18bf0d09bc703e4337d6cc6711e91e0040a6676b",
    sql = "BEGIN;\
ALTER SEQUENCE \"public\".\"permissions_id_seq\" AS integer RESTART WITH 1000000;\
ALTER SEQUENCE \"public\".\"permission_groups_id_seq\" AS integer RESTART WITH 1000000;\
\
COMMIT;\
",
    targetHash = "48dcc31dc0a6c27688d72eae18bf0d09bc703e4337d6cc6711e91e0040a6676b",
    transactional = true,
}
