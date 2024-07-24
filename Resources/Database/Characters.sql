CREATE TABLE IF NOT EXISTS public.characters
(
    id bigint NOT NULL DEFAULT nextval('characters_id_seq'::regclass),
    name text COLLATE pg_catalog."default" NOT NULL,
    permissionlevel smallint NOT NULL DEFAULT 0,
    CONSTRAINT characters_pkey PRIMARY KEY (id)
);

INSERT INTO public.characters(
	id, name, permissionlevel)
	VALUES (1, 'dev', 5);