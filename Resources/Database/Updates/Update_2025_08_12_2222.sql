DROP TABLE IF EXISTS public.proximity_triggers CASCADE;

CREATE TABLE public.proximity_triggers
(
    id serial NOT NULL,
    name text NOT NULL DEFAULT '',
    flags smallint NOT NULL DEFAULT '0',
    map_id smallint NOT NULL DEFAULT '0',
    position_x real NOT NULL DEFAULT '0.0',
    position_y real NOT NULL DEFAULT '0.0',
    position_z real NOT NULL DEFAULT '0.0',
    extents_x real NOT NULL DEFAULT '1.0',
    extents_y real NOT NULL DEFAULT '1.0',
    extents_z real NOT NULL DEFAULT '1.0',
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.proximity_triggers
    OWNER to postgres;