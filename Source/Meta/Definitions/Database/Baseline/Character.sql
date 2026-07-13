CREATE OR REPLACE FUNCTION public.swap_container_slots(
    p_character_id public.character_items.character_id%TYPE,
    p_src_container public.character_items.container%TYPE,
    p_dest_container public.character_items.container%TYPE,
    p_src_slot public.character_items.slot%TYPE,
    p_dest_slot public.character_items.slot%TYPE)
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE
    destination_item_instance_id bigint;
BEGIN
    SELECT item_instance_id
      INTO destination_item_instance_id
      FROM public.character_items
     WHERE character_id = p_character_id
       AND container = p_dest_container
       AND slot = p_dest_slot;

    IF FOUND THEN
        UPDATE public.character_items
           SET slot = p_dest_slot,
               container = p_dest_container
         WHERE character_id = p_character_id
           AND container = p_src_container
           AND slot = p_src_slot;

        UPDATE public.character_items
           SET slot = p_src_slot,
               container = p_src_container
         WHERE character_id = p_character_id
           AND container = p_dest_container
           AND slot = p_dest_slot
           AND item_instance_id = destination_item_instance_id;
    ELSE
        UPDATE public.character_items
           SET slot = p_dest_slot,
               container = p_dest_container
         WHERE character_id = p_character_id
           AND container = p_src_container
           AND slot = p_src_slot;
    END IF;
END;
$$;
