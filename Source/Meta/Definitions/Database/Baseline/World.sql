CREATE OR REPLACE FUNCTION public.fix_all_sequences(schema_name text)
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE
    entry record;
    maximum_id bigint;
BEGIN
    FOR entry IN
        SELECT table_class.relname AS table_name,
               attribute.attname AS column_name,
               pg_get_serial_sequence(format('%I.%I', namespace.nspname, table_class.relname), attribute.attname) AS sequence_name
          FROM pg_class AS table_class
          JOIN pg_namespace AS namespace ON namespace.oid = table_class.relnamespace
          JOIN pg_attribute AS attribute ON attribute.attrelid = table_class.oid
         WHERE namespace.nspname = schema_name
           AND table_class.relkind = 'r'
           AND attribute.attnum > 0
           AND NOT attribute.attisdropped
           AND pg_get_serial_sequence(format('%I.%I', namespace.nspname, table_class.relname), attribute.attname) IS NOT NULL
    LOOP
        EXECUTE format('SELECT COALESCE(MAX(%I), 0) FROM %I.%I',
                       entry.column_name, schema_name, entry.table_name)
           INTO maximum_id;
        EXECUTE format('SELECT setval(%L, %s, false)', entry.sequence_name, maximum_id + 1);
    END LOOP;
END;
$$;

INSERT INTO public.currency (id, name) VALUES (1, 'gold');

INSERT INTO public.permission_groups (id, name) VALUES
    (1, 'admin'),
    (2, 'gamemaster'),
    (3, 'moderator'),
    (4, 'qa'),
    (5, 'tester');

INSERT INTO public.permissions (id, name) VALUES
    (1, 'cheatdamage'),
    (2, 'cheatheal'),
    (3, 'cheatkill'),
    (4, 'cheatresurrect'),
    (5, 'cheatmorph'),
    (6, 'cheatdemorph'),
    (7, 'cheatteleport'),
    (8, 'cheatcreatechar'),
    (9, 'cheatdeletechar'),
    (10, 'cheatdeletepermission'),
    (11, 'cheatsetpermission'),
    (12, 'cheatdeletepermissiongroup'),
    (13, 'cheatsetpermissiongroup'),
    (14, 'cheatsetcurrency');

INSERT INTO public.permission_group_data (id, group_id, permission_id) VALUES
    (1, 1, 8),
    (2, 1, 9),
    (3, 1, 10),
    (4, 1, 11),
    (5, 1, 12),
    (6, 1, 13),
    (7, 2, 1),
    (8, 2, 2),
    (9, 2, 3),
    (10, 2, 4),
    (11, 2, 5),
    (12, 2, 6),
    (13, 2, 7),
    (14, 2, 14);

SELECT public.fix_all_sequences('public');
