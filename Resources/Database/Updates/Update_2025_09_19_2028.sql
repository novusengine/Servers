DROP TABLE IF EXISTS public.spells CASCADE;
CREATE TABLE public.spells
(
    id serial NOT NULL,
    name text NOT NULL DEFAULT '',
    description text NOT NULL DEFAULT '',
    aura_description text NOT NULL DEFAULT '',
    icon_id integer NOT NULL DEFAULT 0,
    cast_time real NOT NULL DEFAULT 0,
    cooldown real NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.spells
    OWNER to postgres;

DROP TABLE IF EXISTS public.spell_effects CASCADE;
CREATE TABLE public.spell_effects
(
    id serial NOT NULL,
    spell_id integer NOT NULL,
    effect_priority smallint NOT NULL DEFAULT 0,
    effect_type smallint NOT NULL,
    effect_value_1 integer NOT NULL DEFAULT 0,
    effect_value_2 integer NOT NULL DEFAULT 0,
    effect_value_3 integer NOT NULL DEFAULT 0,
    effect_misc_value_1 integer NOT NULL DEFAULT 0,
    effect_misc_value_2 integer NOT NULL DEFAULT 0,
    effect_misc_value_3 integer NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.spell_effects
    OWNER to postgres;

ALTER TABLE IF EXISTS public.creature_templates
    ADD COLUMN script_name text NOT NULL DEFAULT '';

ALTER TABLE IF EXISTS public.creatures
    ADD COLUMN script_name text NOT NULL DEFAULT '';

CREATE OR REPLACE FUNCTION fix_all_sequences(schema_name text)
RETURNS void AS
$$
DECLARE
    r record;
    max_id bigint;
BEGIN
    FOR r IN
        SELECT c.relname AS table_name,
               a.attname AS column_name,
               pg_get_serial_sequence(format('%I.%I', n.nspname, c.relname), a.attname) AS seq_name
        FROM pg_class c
        JOIN pg_namespace n ON n.oid = c.relnamespace
        JOIN pg_attribute a ON a.attrelid = c.oid
        WHERE n.nspname = schema_name
          AND c.relkind = 'r'
          AND a.attnum > 0
          AND NOT a.attisdropped
          AND pg_get_serial_sequence(format('%I.%I', n.nspname, c.relname), a.attname) IS NOT NULL
    LOOP
        EXECUTE format('SELECT COALESCE(MAX(%I), 0) FROM %I.%I',
                       r.column_name, schema_name, r.table_name)
        INTO max_id;

        EXECUTE format('SELECT setval(%L, %s, false)', r.seq_name, max_id + 1);
    END LOOP;
END;
$$ LANGUAGE plpgsql;