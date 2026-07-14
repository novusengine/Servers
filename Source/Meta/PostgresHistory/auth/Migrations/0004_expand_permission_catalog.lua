-- MetaGen PostgreSQL history. Review and commit this file.
return {
    bundle = "auth",
    contentHash = "93f78e1cafa944fbf977affe5f5525023bbe8a07e9b59c142e47617a80e67c6c",
    id = "0004_expand_permission_catalog",
    name = "expand_permission_catalog",
    operations = {
        [1] = {
            kind = "addColumn",
            persistentId = "postgres.permission_groups.description",
            sql = "ALTER TABLE \"public\".\"permission_groups\" ADD COLUMN \"description\" text DEFAULT '' NOT NULL;",
        },
        [2] = {
            kind = "addColumn",
            persistentId = "postgres.permission_groups.is_system",
            sql = "ALTER TABLE \"public\".\"permission_groups\" ADD COLUMN \"is_system\" boolean DEFAULT FALSE NOT NULL;",
        },
        [3] = {
            kind = "addColumn",
            persistentId = "postgres.permissions.category",
            sql = "ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"category\" text DEFAULT '' NOT NULL;",
        },
        [4] = {
            kind = "addColumn",
            persistentId = "postgres.permissions.default_value",
            sql = "ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"default_value\" bigint DEFAULT 0 NOT NULL;",
        },
        [5] = {
            kind = "addColumn",
            persistentId = "postgres.permissions.description",
            sql = "ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"description\" text DEFAULT '' NOT NULL;",
        },
        [6] = {
            kind = "addColumn",
            persistentId = "postgres.permissions.value_kind",
            sql = "ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"value_kind\" smallint DEFAULT 0 NOT NULL;",
        },
        [7] = {
            kind = "widenColumn",
            persistentId = "postgres.account_permission_groups.permission_group_id",
            sql = "ALTER TABLE \"public\".\"account_permission_groups\" ALTER COLUMN \"permission_group_id\" TYPE integer;",
        },
        [8] = {
            kind = "widenColumn",
            persistentId = "postgres.account_permissions.permission_id",
            sql = "ALTER TABLE \"public\".\"account_permissions\" ALTER COLUMN \"permission_id\" TYPE integer;",
        },
        [9] = {
            kind = "widenColumn",
            persistentId = "postgres.permission_group_data.group_id",
            sql = "ALTER TABLE \"public\".\"permission_group_data\" ALTER COLUMN \"group_id\" TYPE integer;",
        },
        [10] = {
            kind = "widenColumn",
            persistentId = "postgres.permission_group_data.permission_id",
            sql = "ALTER TABLE \"public\".\"permission_group_data\" ALTER COLUMN \"permission_id\" TYPE integer;",
        },
        [11] = {
            kind = "widenColumn",
            persistentId = "postgres.permission_groups.id",
            sql = "ALTER TABLE \"public\".\"permission_groups\" ALTER COLUMN \"id\" TYPE integer;",
        },
        [12] = {
            kind = "widenColumn",
            persistentId = "postgres.permissions.id",
            sql = "ALTER TABLE \"public\".\"permissions\" ALTER COLUMN \"id\" TYPE integer;",
        },
    },
    parentHash = "be37cd3f464296db566a27a0c02284ce6d7885d42d80c51b1d61188816c45f3d",
    sql = "BEGIN;\
ALTER TABLE \"public\".\"permission_groups\" ADD COLUMN \"description\" text DEFAULT '' NOT NULL;\
ALTER TABLE \"public\".\"permission_groups\" ADD COLUMN \"is_system\" boolean DEFAULT FALSE NOT NULL;\
ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"category\" text DEFAULT '' NOT NULL;\
ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"default_value\" bigint DEFAULT 0 NOT NULL;\
ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"description\" text DEFAULT '' NOT NULL;\
ALTER TABLE \"public\".\"permissions\" ADD COLUMN \"value_kind\" smallint DEFAULT 0 NOT NULL;\
ALTER TABLE \"public\".\"account_permission_groups\" ALTER COLUMN \"permission_group_id\" TYPE integer;\
ALTER TABLE \"public\".\"account_permissions\" ALTER COLUMN \"permission_id\" TYPE integer;\
ALTER TABLE \"public\".\"permission_group_data\" ALTER COLUMN \"group_id\" TYPE integer;\
ALTER TABLE \"public\".\"permission_group_data\" ALTER COLUMN \"permission_id\" TYPE integer;\
ALTER TABLE \"public\".\"permission_groups\" ALTER COLUMN \"id\" TYPE integer;\
ALTER TABLE \"public\".\"permissions\" ALTER COLUMN \"id\" TYPE integer;\
COMMIT;\
",
    targetHash = "48dcc31dc0a6c27688d72eae18bf0d09bc703e4337d6cc6711e91e0040a6676b",
    transactional = true,
}
