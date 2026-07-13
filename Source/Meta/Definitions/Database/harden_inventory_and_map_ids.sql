LOCK TABLE public.characters, public.item_instances, public.character_items IN SHARE ROW EXCLUSIVE MODE;

DO $$
BEGIN
    IF EXISTS (
        SELECT 1 FROM public.character_items
        GROUP BY character_id, container, slot
        HAVING count(*) > 1
    ) THEN
        RAISE EXCEPTION 'character_items contains duplicate character/container/slot placements; resolve them before migration';
    END IF;

    IF EXISTS (
        SELECT 1 FROM public.item_instances ii
        LEFT JOIN public.characters c ON c.id = ii.owner_id
        WHERE c.id IS NULL
    ) THEN
        RAISE EXCEPTION 'item_instances contains owner_id values without matching characters; resolve them before migration';
    END IF;

    IF EXISTS (
        SELECT 1 FROM public.character_items ci
        LEFT JOIN public.characters c ON c.id = ci.character_id
        WHERE c.id IS NULL
    ) THEN
        RAISE EXCEPTION 'character_items contains character_id values without matching characters; resolve them before migration';
    END IF;

    IF EXISTS (
        SELECT 1 FROM public.character_items ci
        LEFT JOIN public.item_instances ii
            ON ii.id = ci.item_instance_id AND ii.owner_id = ci.character_id
        WHERE ii.id IS NULL
    ) THEN
        RAISE EXCEPTION 'character_items contains missing or cross-owner item instances; resolve them before migration';
    END IF;
END;
$$;

DROP INDEX public.character_items_character_container_slot_idx;

ALTER TABLE public.item_instances
    ADD CONSTRAINT item_instances_owner_id_id_key UNIQUE (owner_id, id),
    ADD CONSTRAINT item_instances_owner_fk FOREIGN KEY (owner_id)
        REFERENCES public.characters (id) ON DELETE CASCADE;

ALTER TABLE public.character_items
    ADD CONSTRAINT character_items_character_container_slot_key UNIQUE (character_id, container, slot),
    ADD CONSTRAINT character_items_character_fk FOREIGN KEY (character_id)
        REFERENCES public.characters (id) ON DELETE CASCADE,
    ADD CONSTRAINT character_items_owner_item_fk FOREIGN KEY (character_id, item_instance_id)
        REFERENCES public.item_instances (owner_id, id) ON DELETE CASCADE;
