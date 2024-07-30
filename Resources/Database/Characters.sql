CREATE SEQUENCE IF NOT EXISTS public.characters_id_seq
    INCREMENT 1
    START 1
    MINVALUE 1
    MAXVALUE 9223372036854775807
    CACHE 1;

ALTER SEQUENCE public.characters_id_seq
    OWNER TO postgres;

CREATE TABLE IF NOT EXISTS public.characters
(
    id bigint NOT NULL DEFAULT nextval('characters_id_seq'::regclass),
    name text COLLATE pg_catalog."default" NOT NULL,
    permissionlevel smallint NOT NULL DEFAULT 0,
    CONSTRAINT characters_pkey PRIMARY KEY (id)
);

ALTER SEQUENCE public.characters_id_seq
    OWNED BY public.characters.id;

INSERT INTO public.characters(
	id, name, permissionlevel)
	VALUES (1, 'dev', 5);