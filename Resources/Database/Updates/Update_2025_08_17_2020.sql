ALTER TABLE public.characters
    ALTER COLUMN position_x TYPE real;

ALTER TABLE public.characters
    ALTER COLUMN position_y TYPE real;

ALTER TABLE public.characters
    ALTER COLUMN position_z TYPE real;

ALTER TABLE public.characters
    ALTER COLUMN position_o TYPE real;

DROP TABLE IF EXISTS public.creature_template CASCADE;
CREATE TABLE public.creature_template
(
    id serial NOT NULL,
    flags integer NOT NULL DEFAULT 0,
    name text NOT NULL DEFAULT 'Unknown',
    subname text NOT NULL DEFAULT '',
    displayid integer NOT NULL DEFAULT 0,
    scale real NOT NULL DEFAULT 1.0,
    minlevel smallint NOT NULL DEFAULT 1,
    maxlevel smallint NOT NULL DEFAULT 1,
    armormod real NOT NULL DEFAULT 1.0,
    healthmod real NOT NULL DEFAULT 1.0,
    resourcemod real NOT NULL DEFAULT 1.0,
    damagemod real NOT NULL DEFAULT 1.0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.creature_template
    OWNER to postgres;

DROP TABLE IF EXISTS public.creature CASCADE;
CREATE TABLE public.creature
(
    id bigserial NOT NULL,
    templateid integer NOT NULL,
    displayid integer NOT NULL,
    mapid smallint NOT NULL,
    position_x real NOT NULL,
    position_y real NOT NULL,
    position_z real NOT NULL,
    position_o real NOT NULL,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.creature
    OWNER to postgres;

ALTER TABLE IF EXISTS public.character_currency
    RENAME characterid TO character_id;

ALTER TABLE IF EXISTS public.character_currency
    RENAME currencyid TO currency_id;

ALTER TABLE IF EXISTS public.character_items
    RENAME charid TO character_id;

ALTER TABLE IF EXISTS public.character_items
    RENAME iteminstanceid TO item_instance_id;

ALTER TABLE IF EXISTS public.character_permission_groups
    RENAME characterid TO character_id;

ALTER TABLE IF EXISTS public.character_permission_groups
    RENAME permissiongroupid TO permission_group_id;

ALTER TABLE IF EXISTS public.character_permissions
    RENAME characterid TO character_id;

ALTER TABLE IF EXISTS public.character_permissions
    RENAME permissionid TO permission_id;

ALTER TABLE IF EXISTS public.characters
    RENAME accountid TO account_id;

ALTER TABLE IF EXISTS public.characters
    RENAME totaltime TO total_time;

ALTER TABLE IF EXISTS public.characters
    RENAME leveltime TO level_time;

ALTER TABLE IF EXISTS public.characters
    RENAME logouttime TO logout_time;

ALTER TABLE IF EXISTS public.characters
    RENAME racegenderclass TO race_gender_class;

ALTER TABLE IF EXISTS public.characters
    RENAME experiencepoints TO experience_points;

ALTER TABLE IF EXISTS public.characters
    RENAME mapid TO map_id;

ALTER TABLE IF EXISTS public.creature
    RENAME templateid TO template_id;

ALTER TABLE IF EXISTS public.creature
    RENAME displayid TO display_id;

ALTER TABLE IF EXISTS public.creature
    RENAME mapid TO map_id;

ALTER TABLE IF EXISTS public.creature_template
    RENAME displayid TO display_id;

ALTER TABLE IF EXISTS public.creature_template
    RENAME minlevel TO min_level;

ALTER TABLE IF EXISTS public.creature_template
    RENAME maxlevel TO max_level;

ALTER TABLE IF EXISTS public.creature_template
    RENAME armormod TO armor_mod;

ALTER TABLE IF EXISTS public.creature_template
    RENAME healthmod TO health_mod;

ALTER TABLE IF EXISTS public.creature_template
    RENAME resourcemod TO resource_mod;

ALTER TABLE IF EXISTS public.creature_template
    RENAME damagemod TO damage_mod;

ALTER TABLE IF EXISTS public.item_armor_template
    RENAME equiptype TO equip_type;

ALTER TABLE IF EXISTS public.item_armor_template
    RENAME bonusarmor TO bonus_armor;

ALTER TABLE IF EXISTS public.item_instances
    RENAME itemid TO item_id;

ALTER TABLE IF EXISTS public.item_instances
    RENAME ownerid TO owner_id;

ALTER TABLE IF EXISTS public.item_shield_template
    RENAME bonusarmor TO bonus_armor;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype1 TO stat_type_1;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype2 TO stat_type_2;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype3 TO stat_type_3;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype4 TO stat_type_4;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype5 TO stat_type_5;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype6 TO stat_type_6;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype7 TO stat_type_7;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME stattype8 TO stat_type_8;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue1 TO stat_value_1;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue2 TO stat_value_2;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue3 TO stat_value_3;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue4 TO stat_value_4;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue5 TO stat_value_5;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue6 TO stat_value_6;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue7 TO stat_value_7;

ALTER TABLE IF EXISTS public.item_stat_template
    RENAME statvalue8 TO stat_value_8;

ALTER TABLE IF EXISTS public.item_template
    RENAME displayid TO display_id;

ALTER TABLE IF EXISTS public.item_template
    RENAME virtuallevel TO virtual_level;

ALTER TABLE IF EXISTS public.item_template
    RENAME requiredlevel TO required_level;

ALTER TABLE IF EXISTS public.item_template
    RENAME iconid TO icon_id;

ALTER TABLE IF EXISTS public.item_template
    RENAME stattemplateid TO stat_template_id;

ALTER TABLE IF EXISTS public.item_template
    RENAME armortemplateid TO armor_template_id;

ALTER TABLE IF EXISTS public.item_template
    RENAME weapontemplateid TO weapon_template_id;

ALTER TABLE IF EXISTS public.item_template
    RENAME shieldtemplateid TO shield_template_id;

ALTER TABLE IF EXISTS public.item_weapon_template
    RENAME weaponstyle TO weapon_style;

ALTER TABLE IF EXISTS public.item_weapon_template
    RENAME mindamage TO min_damage;

ALTER TABLE IF EXISTS public.item_weapon_template
    RENAME maxdamage TO max_damage;

ALTER TABLE public.item_weapon_template
    ALTER COLUMN speed TYPE real;

ALTER TABLE IF EXISTS public.permission_group_data
    RENAME groupid TO group_id;

ALTER TABLE IF EXISTS public.permission_group_data
    RENAME permissionid TO permission_id;

ALTER TABLE IF EXISTS public.creature
    RENAME TO creatures;

ALTER TABLE IF EXISTS public.creature_template
    RENAME TO creature_templates;

ALTER TABLE IF EXISTS public.item_template
    RENAME TO item_templates;

ALTER TABLE IF EXISTS public.item_armor_template
    RENAME TO item_armor_templates;

ALTER TABLE IF EXISTS public.item_shield_template
    RENAME TO item_shield_templates;
    
ALTER TABLE IF EXISTS public.item_stat_template
    RENAME TO item_stat_templates;

ALTER TABLE IF EXISTS public.item_weapon_template
    RENAME TO item_weapon_templates;

DROP FUNCTION swap_container_slots(bigint,bigint,bigint,smallint,smallint);
CREATE OR REPLACE FUNCTION swap_container_slots(
p_character_id public.character_items.character_id%TYPE,
p_src_container public.character_items.container%TYPE,
p_dest_container public.character_items.container%TYPE,
p_src_slot public.character_items.slot%TYPE,
p_dest_slot public.character_items.slot%TYPE
)
RETURNS VOID
AS $$
DECLARE
    dest_item_instance_id BIGINT;
BEGIN
    -- Check if the destination slot is occupied
    SELECT item_instance_id INTO dest_item_instance_id
    FROM public.character_items
    WHERE character_id = p_character_id
      AND container = p_dest_container
      AND slot = p_dest_slot;

    -- If destination slot is occupied, perform a swap
    IF FOUND THEN
        -- Update item src -> dest
        UPDATE public.character_items
        SET slot = p_dest_slot, container = p_dest_container
        WHERE character_id = p_character_id
          AND container = p_src_container
          AND slot = p_src_slot;

        -- Update item dest -> src
        UPDATE public.character_items
        SET slot = p_src_slot, container = p_src_container
        WHERE character_id = p_character_id
          AND container = p_dest_container
          AND slot = p_dest_slot
          AND item_instance_id = dest_item_instance_id;
    ELSE
        -- If destination slot is empty, just move the item to destination slot/container
        UPDATE public.character_items
        SET slot = p_dest_slot, container = p_dest_container
        WHERE character_id = p_character_id
          AND container = p_src_container
          AND slot = p_src_slot;
    END IF;
END;
$$ LANGUAGE plpgsql;

DROP TABLE IF EXISTS public.maps CASCADE;
CREATE TABLE public.maps
(
    id serial NOT NULL,
    flags integer NOT NULL DEFAULT 0,
    name text NOT NULL DEFAULT '',
    type smallint NOT NULL DEFAULT 0,
    max_players smallint NOT NULL DEFAULT 0,
    PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.maps
    OWNER to postgres;

DROP TABLE IF EXISTS public.map_locations CASCADE;
CREATE TABLE public.map_locations
(
    id serial NOT NULL,
    name text NOT NULL DEFAULT '',
    map_id integer NOT NULL,
    position_x real NOT NULL DEFAULT 0.0,
    position_y real NOT NULL DEFAULT 0.0,
    position_z real NOT NULL DEFAULT 0.0,
    position_o real NOT NULL DEFAULT 0.0,
    PRIMARY KEY (id),
    CONSTRAINT map_id FOREIGN KEY (map_id)
        REFERENCES public.maps (id) MATCH SIMPLE
        ON UPDATE NO ACTION
        ON DELETE NO ACTION
        NOT VALID
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.map_locations
    OWNER to postgres;