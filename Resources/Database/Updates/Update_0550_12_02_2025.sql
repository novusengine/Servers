CREATE TABLE IF NOT EXISTS public.item_instances
(
    id bigserial NOT NULL,
    itemid integer NOT NULL,
    ownerid bigint NOT NULL,
    count smallint NOT NULL,
    durability smallint NOT NULL,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.item_instances
    OWNER to postgres;

CREATE TABLE IF NOT EXISTS public.character_items
(
    charid bigint NOT NULL,
    container bigint NOT NULL,
    slot smallint NOT NULL,
    iteminstanceid bigint NOT NULL,
    CONSTRAINT unique_item_instance_id UNIQUE (iteminstanceid)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.character_items
    OWNER to postgres;

CREATE TABLE public.item_stat_template
(
    id serial NOT NULL,
    stattype1 smallint NOT NULL DEFAULT 0,
    stattype2 smallint NOT NULL DEFAULT 0,
    stattype3 smallint NOT NULL DEFAULT 0,
    stattype4 smallint NOT NULL DEFAULT 0,
    stattype5 smallint NOT NULL DEFAULT 0,
    stattype6 smallint NOT NULL DEFAULT 0,
    stattype7 smallint NOT NULL DEFAULT 0,
    stattype8 smallint NOT NULL DEFAULT 0,
    statvalue1 integer NOT NULL DEFAULT 0,
    statvalue2 integer NOT NULL DEFAULT 0,
    statvalue3 integer NOT NULL DEFAULT 0,
    statvalue4 integer NOT NULL DEFAULT 0,
    statvalue5 integer NOT NULL DEFAULT 0,
    statvalue6 integer NOT NULL DEFAULT 0,
    statvalue7 integer NOT NULL DEFAULT 0,
    statvalue8 integer NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.item_stat_template
    OWNER to postgres;

CREATE TABLE public.item_armor_template
(
    id serial NOT NULL,
    equiptype smallint NOT NULL DEFAULT 0,
    bonusarmor integer NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
);

ALTER TABLE IF EXISTS public.item_armor_template
    OWNER to postgres;

CREATE TABLE public.item_weapon_template
(
    id serial NOT NULL,
    weaponstyle smallint NOT NULL DEFAULT 0,
    mindamage integer NOT NULL DEFAULT 0,
    maxdamage integer NOT NULL DEFAULT 0,
    speed numeric(4, 2) NOT NULL DEFAULT 1.0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.item_weapon_template
    OWNER to postgres;

CREATE TABLE public.item_shield_template
(
    id serial NOT NULL,
    bonusarmor integer NOT NULL DEFAULT 0,
    block integer NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.item_shield_template
    OWNER to postgres;

CREATE TABLE public.item_template
(
    id serial NOT NULL,
    displayid integer NOT NULL DEFAULT 0,
    bind smallint NOT NULL DEFAULT 0,
    rarity smallint NOT NULL DEFAULT 0,
    category smallint NOT NULL DEFAULT 1,
    type smallint NOT NULL DEFAULT 1,
    virtuallevel smallint NOT NULL DEFAULT 0,
    requiredlevel smallint NOT NULL DEFAULT 0,
    durability integer NOT NULL DEFAULT 0,
    iconid integer NOT NULL DEFAULT 0,
    name text NOT NULL,
    description text NOT NULL DEFAULT '',
    armor integer NOT NULL DEFAULT 0,
    stattemplateid integer NOT NULL DEFAULT 0,
    armortemplateid integer NOT NULL DEFAULT 0,
    weapontemplateid integer NOT NULL DEFAULT 0,
    shieldtemplateid integer NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.item_template
    OWNER to postgres;

CREATE OR REPLACE FUNCTION swap_container_slots(
    p_charid public.character_items.charid%TYPE,
    p_srccontainer public.character_items.container%TYPE,
    p_destcontainer public.character_items.container%TYPE,
    p_srcslot public.character_items.slot%TYPE,
    p_destslot public.character_items.slot%TYPE
)
RETURNS VOID
AS $$
DECLARE
    dest_iteminstanceid INT;
BEGIN
    -- Check if the destination slot is occupied
    SELECT iteminstanceid INTO dest_iteminstanceid
    FROM public.character_items
    WHERE charid = p_charid
      AND container = p_destcontainer
      AND slot = p_destslot;

    -- If destination slot is occupied, perform a swap
    IF FOUND THEN
        -- Update item src -> dest
        UPDATE public.character_items
        SET slot = p_destslot, container = p_destcontainer
        WHERE charid = p_charid
          AND container = p_srccontainer
          AND slot = p_srcslot;

        -- Update item dest -> src
        UPDATE public.character_items
        SET slot = p_srcslot, container = p_srccontainer
        WHERE charid = p_charid
          AND container = p_destcontainer
          AND slot = p_destslot
          AND iteminstanceid = dest_iteminstanceid;
    ELSE
        -- If destination slot is empty, just move the item to destination slot/container
        UPDATE public.character_items
        SET slot = p_destslot, container = p_destcontainer
        WHERE charid = p_charid
          AND container = p_srccontainer
          AND slot = p_srcslot;
    END IF;
END;
$$ LANGUAGE plpgsql;