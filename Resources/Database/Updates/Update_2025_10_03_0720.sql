ALTER TABLE IF EXISTS public.spells
    ADD COLUMN duration real NOT NULL DEFAULT 0;

CREATE TABLE public.spell_proc_data
(
    id serial NOT NULL,
    phase_mask integer NOT NULL DEFAULT -1,
    type_mask bigint NOT NULL DEFAULT -1,
    hit_mask bigint NOT NULL DEFAULT -1,
    flags bigint NOT NULL DEFAULT 0,
    procs_per_min real NOT NULL DEFAULT 1.0,
    chance_to_proc real NOT NULL DEFAULT 0.0,
    internal_cooldown_ms integer NOT NULL DEFAULT 0,
    charges integer NOT NULL DEFAULT -1,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.spell_proc_data
    OWNER to postgres;

CREATE TABLE public.spell_proc_link
(
    id serial NOT NULL,
    spell_id integer NOT NULL,
    effect_mask bigint NOT NULL DEFAULT -1,
    proc_data_id integer NOT NULL,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.spell_proc_link
    OWNER to postgres;