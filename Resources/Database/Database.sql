--
-- PostgreSQL database dump
--

-- Dumped from database version 16.3
-- Dumped by pg_dump version 16.3

-- Started on 2024-07-31 15:30:22

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- TOC entry 231 (class 1259 OID 32930)
-- Name: character_currency; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.character_currency CASCADE;
CREATE TABLE public.character_currency (
    id bigint NOT NULL,
    characterid bigint NOT NULL,
    currencyid smallint NOT NULL,
    value bigint DEFAULT 0 NOT NULL
);


ALTER TABLE public.character_currency OWNER TO postgres;

--
-- TOC entry 230 (class 1259 OID 32929)
-- Name: character_currency_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.character_currency_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.character_currency_id_seq OWNER TO postgres;

--
-- TOC entry 4942 (class 0 OID 0)
-- Dependencies: 230
-- Name: character_currency_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.character_currency_id_seq OWNED BY public.character_currency.id;


--
-- TOC entry 227 (class 1259 OID 32864)
-- Name: character_permission_groups; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.character_permission_groups CASCADE;
CREATE TABLE public.character_permission_groups (
    id bigint NOT NULL,
    characterid bigint NOT NULL,
    permissiongroupid smallint NOT NULL
);


ALTER TABLE public.character_permission_groups OWNER TO postgres;

--
-- TOC entry 226 (class 1259 OID 32863)
-- Name: character_permission_groups_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.character_permission_groups_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.character_permission_groups_id_seq OWNER TO postgres;

--
-- TOC entry 4943 (class 0 OID 0)
-- Dependencies: 226
-- Name: character_permission_groups_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.character_permission_groups_id_seq OWNED BY public.character_permission_groups.id;


--
-- TOC entry 225 (class 1259 OID 32857)
-- Name: character_permissions; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.character_permissions CASCADE;
CREATE TABLE public.character_permissions (
    id bigint NOT NULL,
    characterid bigint NOT NULL,
    permissionid smallint NOT NULL
);


ALTER TABLE public.character_permissions OWNER TO postgres;

--
-- TOC entry 224 (class 1259 OID 32856)
-- Name: character_permissions_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.character_permissions_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.character_permissions_id_seq OWNER TO postgres;

--
-- TOC entry 4944 (class 0 OID 0)
-- Dependencies: 224
-- Name: character_permissions_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.character_permissions_id_seq OWNED BY public.character_permissions.id;


--
-- TOC entry 217 (class 1259 OID 16406)
-- Name: characters; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.characters CASCADE;
CREATE TABLE public.characters (
    id bigint NOT NULL,
    accountid integer DEFAULT 0 NOT NULL,
    name character varying(12) NOT NULL,
    totaltime integer DEFAULT 0 NOT NULL,
    leveltime integer DEFAULT 0 NOT NULL,
    logouttime bigint DEFAULT 0 NOT NULL,
    flags integer DEFAULT 0 NOT NULL,
    racegenderclass smallint NOT NULL,
    level smallint DEFAULT 1 NOT NULL,
    experiencepoints bigint DEFAULT 0 NOT NULL,
    mapid smallint DEFAULT 0 NOT NULL,
    position_x numeric(10,4) DEFAULT 0.0 NOT NULL,
    position_y numeric(10,4) DEFAULT 0.0 NOT NULL,
    position_z numeric(10,4) DEFAULT 0.0 NOT NULL,
    position_o numeric(10,4) DEFAULT 0.0 NOT NULL
);


ALTER TABLE public.characters OWNER TO postgres;

--
-- TOC entry 216 (class 1259 OID 16405)
-- Name: characters_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.characters_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.characters_id_seq OWNER TO postgres;

--
-- TOC entry 4945 (class 0 OID 0)
-- Dependencies: 216
-- Name: characters_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.characters_id_seq OWNED BY public.characters.id;


--
-- TOC entry 229 (class 1259 OID 32901)
-- Name: currency; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.currency CASCADE;
CREATE TABLE public.currency (
    id smallint NOT NULL,
    name text NOT NULL
);


ALTER TABLE public.currency OWNER TO postgres;

--
-- TOC entry 228 (class 1259 OID 32900)
-- Name: currency_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.currency_id_seq
    AS smallint
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.currency_id_seq OWNER TO postgres;

--
-- TOC entry 4946 (class 0 OID 0)
-- Dependencies: 228
-- Name: currency_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.currency_id_seq OWNED BY public.currency.id;


--
-- TOC entry 223 (class 1259 OID 32850)
-- Name: permission_group_data; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.permission_group_data CASCADE;
CREATE TABLE public.permission_group_data (
    id integer NOT NULL,
    groupid smallint NOT NULL,
    permissionid smallint NOT NULL
);


ALTER TABLE public.permission_group_data OWNER TO postgres;

--
-- TOC entry 222 (class 1259 OID 32849)
-- Name: permission_group_data_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_group_data_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_group_data_id_seq OWNER TO postgres;

--
-- TOC entry 4947 (class 0 OID 0)
-- Dependencies: 222
-- Name: permission_group_data_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.permission_group_data_id_seq OWNED BY public.permission_group_data.id;


--
-- TOC entry 221 (class 1259 OID 32841)
-- Name: permission_groups; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.permission_groups CASCADE;
CREATE TABLE public.permission_groups (
    id smallint NOT NULL,
    name text NOT NULL
);


ALTER TABLE public.permission_groups OWNER TO postgres;

--
-- TOC entry 220 (class 1259 OID 32840)
-- Name: permission_groups_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_groups_id_seq
    AS smallint
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_groups_id_seq OWNER TO postgres;

--
-- TOC entry 4948 (class 0 OID 0)
-- Dependencies: 220
-- Name: permission_groups_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.permission_groups_id_seq OWNED BY public.permission_groups.id;


--
-- TOC entry 219 (class 1259 OID 32832)
-- Name: permissions; Type: TABLE; Schema: public; Owner: postgres
--

DROP TABLE IF EXISTS public.permissions CASCADE;
CREATE TABLE public.permissions (
    id smallint NOT NULL,
    name text NOT NULL
);


ALTER TABLE public.permissions OWNER TO postgres;

--
-- TOC entry 218 (class 1259 OID 32831)
-- Name: permissions_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permissions_id_seq
    AS smallint
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permissions_id_seq OWNER TO postgres;

--
-- TOC entry 4949 (class 0 OID 0)
-- Dependencies: 218
-- Name: permissions_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.permissions_id_seq OWNED BY public.permissions.id;


--
-- TOC entry 4735 (class 2604 OID 32933)
-- Name: character_currency id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_currency ALTER COLUMN id SET DEFAULT nextval('public.character_currency_id_seq'::regclass);


--
-- TOC entry 4733 (class 2604 OID 32867)
-- Name: character_permission_groups id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permission_groups ALTER COLUMN id SET DEFAULT nextval('public.character_permission_groups_id_seq'::regclass);


--
-- TOC entry 4732 (class 2604 OID 32860)
-- Name: character_permissions id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permissions ALTER COLUMN id SET DEFAULT nextval('public.character_permissions_id_seq'::regclass);


--
-- TOC entry 4716 (class 2604 OID 16409)
-- Name: characters id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.characters ALTER COLUMN id SET DEFAULT nextval('public.characters_id_seq'::regclass);


--
-- TOC entry 4734 (class 2604 OID 32904)
-- Name: currency id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.currency ALTER COLUMN id SET DEFAULT nextval('public.currency_id_seq'::regclass);


--
-- TOC entry 4731 (class 2604 OID 32853)
-- Name: permission_group_data id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_data ALTER COLUMN id SET DEFAULT nextval('public.permission_group_data_id_seq'::regclass);


--
-- TOC entry 4730 (class 2604 OID 32844)
-- Name: permission_groups id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_groups ALTER COLUMN id SET DEFAULT nextval('public.permission_groups_id_seq'::regclass);


--
-- TOC entry 4729 (class 2604 OID 32835)
-- Name: permissions id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permissions ALTER COLUMN id SET DEFAULT nextval('public.permissions_id_seq'::regclass);


--
-- TOC entry 4936 (class 0 OID 32930)
-- Dependencies: 231
-- Data for Name: character_currency; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.character_currency (id, characterid, currencyid, value) VALUES (1, 1, 1, 0);


--
-- TOC entry 4932 (class 0 OID 32864)
-- Dependencies: 227
-- Data for Name: character_permission_groups; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.character_permission_groups (id, characterid, permissiongroupid) VALUES (1, 1, 1);
INSERT INTO public.character_permission_groups (id, characterid, permissiongroupid) VALUES (2, 1, 2);


--
-- TOC entry 4930 (class 0 OID 32857)
-- Dependencies: 225
-- Data for Name: character_permissions; Type: TABLE DATA; Schema: public; Owner: postgres
--



--
-- TOC entry 4922 (class 0 OID 16406)
-- Dependencies: 217
-- Data for Name: characters; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.characters (id, accountid, name, totaltime, leveltime, logouttime, flags, racegenderclass, level, experiencepoints, mapid, position_x, position_y, position_z, position_o) VALUES (1, 0, 'dev', 0, 0, 0, 0, 1, 1, 0, 0, 0.0000, 0.0000, 0.0000, 0.0000);


--
-- TOC entry 4934 (class 0 OID 32901)
-- Dependencies: 229
-- Data for Name: currency; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.currency (id, name) VALUES (1, 'gold');


--
-- TOC entry 4928 (class 0 OID 32850)
-- Dependencies: 223
-- Data for Name: permission_group_data; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (1, 1, 8);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (2, 1, 9);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (3, 1, 10);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (4, 1, 11);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (5, 1, 12);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (6, 1, 13);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (7, 2, 1);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (8, 2, 2);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (9, 2, 3);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (10, 2, 4);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (11, 2, 5);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (12, 2, 6);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (13, 2, 7);
INSERT INTO public.permission_group_data (id, groupid, permissionid) VALUES (14, 2, 14);


--
-- TOC entry 4926 (class 0 OID 32841)
-- Dependencies: 221
-- Data for Name: permission_groups; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.permission_groups (id, name) VALUES (1, 'admin');
INSERT INTO public.permission_groups (id, name) VALUES (2, 'gamemaster');
INSERT INTO public.permission_groups (id, name) VALUES (3, 'moderator');
INSERT INTO public.permission_groups (id, name) VALUES (4, 'qa');
INSERT INTO public.permission_groups (id, name) VALUES (5, 'tester');


--
-- TOC entry 4924 (class 0 OID 32832)
-- Dependencies: 219
-- Data for Name: permissions; Type: TABLE DATA; Schema: public; Owner: postgres
--

INSERT INTO public.permissions (id, name) VALUES (1, 'cheatdamage');
INSERT INTO public.permissions (id, name) VALUES (2, 'cheatheal');
INSERT INTO public.permissions (id, name) VALUES (3, 'cheatkill');
INSERT INTO public.permissions (id, name) VALUES (4, 'cheatresurrect');
INSERT INTO public.permissions (id, name) VALUES (5, 'cheatmorph');
INSERT INTO public.permissions (id, name) VALUES (6, 'cheatdemorph');
INSERT INTO public.permissions (id, name) VALUES (7, 'cheatteleport');
INSERT INTO public.permissions (id, name) VALUES (8, 'cheatcreatechar');
INSERT INTO public.permissions (id, name) VALUES (9, 'cheatdeletechar');
INSERT INTO public.permissions (id, name) VALUES (10, 'cheatdeletepermission');
INSERT INTO public.permissions (id, name) VALUES (11, 'cheatsetpermission');
INSERT INTO public.permissions (id, name) VALUES (12, 'cheatdeletepermissiongroup');
INSERT INTO public.permissions (id, name) VALUES (13, 'cheatsetpermissiongroup');
INSERT INTO public.permissions (id, name) VALUES (14, 'cheatsetcurrency');


--
-- TOC entry 4950 (class 0 OID 0)
-- Dependencies: 230
-- Name: character_currency_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.character_currency_id_seq', 1, true);


--
-- TOC entry 4951 (class 0 OID 0)
-- Dependencies: 226
-- Name: character_permission_groups_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.character_permission_groups_id_seq', 2, true);


--
-- TOC entry 4952 (class 0 OID 0)
-- Dependencies: 224
-- Name: character_permissions_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.character_permissions_id_seq', 1, false);


--
-- TOC entry 4953 (class 0 OID 0)
-- Dependencies: 216
-- Name: characters_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.characters_id_seq', 1, true);


--
-- TOC entry 4954 (class 0 OID 0)
-- Dependencies: 228
-- Name: currency_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.currency_id_seq', 1, true);


--
-- TOC entry 4955 (class 0 OID 0)
-- Dependencies: 222
-- Name: permission_group_data_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.permission_group_data_id_seq', 14, true);


--
-- TOC entry 4956 (class 0 OID 0)
-- Dependencies: 220
-- Name: permission_groups_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.permission_groups_id_seq', 5, true);


--
-- TOC entry 4957 (class 0 OID 0)
-- Dependencies: 218
-- Name: permissions_id_seq; Type: SEQUENCE SET; Schema: public; Owner: postgres
--

SELECT pg_catalog.setval('public.permissions_id_seq', 14, true);


--
-- TOC entry 4767 (class 2606 OID 32935)
-- Name: character_currency character_currency_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_currency
    ADD CONSTRAINT character_currency_pkey PRIMARY KEY (id);


--
-- TOC entry 4759 (class 2606 OID 32869)
-- Name: character_permission_groups character_permission_groups_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permission_groups
    ADD CONSTRAINT character_permission_groups_pkey PRIMARY KEY (id);


--
-- TOC entry 4755 (class 2606 OID 32862)
-- Name: character_permissions character_permissions_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permissions
    ADD CONSTRAINT character_permissions_pkey PRIMARY KEY (id);


--
-- TOC entry 4739 (class 2606 OID 16413)
-- Name: characters characters_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.characters
    ADD CONSTRAINT characters_pkey PRIMARY KEY (id);


--
-- TOC entry 4737 (class 2606 OID 32910)
-- Name: characters checknamelength; Type: CHECK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE public.characters
    ADD CONSTRAINT checknamelength CHECK ((char_length((name)::text) >= 2)) NOT VALID;


--
-- TOC entry 4763 (class 2606 OID 32908)
-- Name: currency currency_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.currency
    ADD CONSTRAINT currency_pkey PRIMARY KEY (id);


--
-- TOC entry 4751 (class 2606 OID 32855)
-- Name: permission_group_data permission_group_data_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_data
    ADD CONSTRAINT permission_group_data_pkey PRIMARY KEY (id);


--
-- TOC entry 4747 (class 2606 OID 32848)
-- Name: permission_groups permission_groups_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_groups
    ADD CONSTRAINT permission_groups_pkey PRIMARY KEY (id);


--
-- TOC entry 4743 (class 2606 OID 32839)
-- Name: permissions permissions_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permissions
    ADD CONSTRAINT permissions_pkey PRIMARY KEY (id);


--
-- TOC entry 4741 (class 2606 OID 32912)
-- Name: characters unique character name; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.characters
    ADD CONSTRAINT "unique character name" UNIQUE (name);


--
-- TOC entry 4769 (class 2606 OID 32948)
-- Name: character_currency unique characterid to currencyid; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_currency
    ADD CONSTRAINT "unique characterid to currencyid" UNIQUE (characterid, currencyid);


--
-- TOC entry 4761 (class 2606 OID 32924)
-- Name: character_permission_groups unique characterid to permissiongroupid; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permission_groups
    ADD CONSTRAINT "unique characterid to permissiongroupid" UNIQUE (characterid, permissiongroupid);


--
-- TOC entry 4757 (class 2606 OID 32926)
-- Name: character_permissions unique characterid to permissionid; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permissions
    ADD CONSTRAINT "unique characterid to permissionid" UNIQUE (characterid, permissionid);


--
-- TOC entry 4765 (class 2606 OID 32914)
-- Name: currency unique currency name; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.currency
    ADD CONSTRAINT "unique currency name" UNIQUE (name);


--
-- TOC entry 4749 (class 2606 OID 32918)
-- Name: permission_groups unique permission group name; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_groups
    ADD CONSTRAINT "unique permission group name" UNIQUE (name);


--
-- TOC entry 4745 (class 2606 OID 32916)
-- Name: permissions unique permission name; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permissions
    ADD CONSTRAINT "unique permission name" UNIQUE (name);


--
-- TOC entry 4753 (class 2606 OID 32928)
-- Name: permission_group_data unique permissiongroupid to permissionid; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_data
    ADD CONSTRAINT "unique permissiongroupid to permissionid" UNIQUE (groupid, permissionid);


--
-- TOC entry 4772 (class 2606 OID 32870)
-- Name: character_permissions characterid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permissions
    ADD CONSTRAINT characterid FOREIGN KEY (characterid) REFERENCES public.characters(id) ON DELETE CASCADE NOT VALID;


--
-- TOC entry 4774 (class 2606 OID 32880)
-- Name: character_permission_groups characterid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permission_groups
    ADD CONSTRAINT characterid FOREIGN KEY (characterid) REFERENCES public.characters(id) ON DELETE CASCADE NOT VALID;


--
-- TOC entry 4776 (class 2606 OID 32936)
-- Name: character_currency characterid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_currency
    ADD CONSTRAINT characterid FOREIGN KEY (characterid) REFERENCES public.characters(id) ON DELETE CASCADE;


--
-- TOC entry 4777 (class 2606 OID 32941)
-- Name: character_currency currencyid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_currency
    ADD CONSTRAINT currencyid FOREIGN KEY (currencyid) REFERENCES public.currency(id) ON DELETE CASCADE;


--
-- TOC entry 4770 (class 2606 OID 32890)
-- Name: permission_group_data groupid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_data
    ADD CONSTRAINT groupid FOREIGN KEY (groupid) REFERENCES public.permission_groups(id) ON DELETE CASCADE NOT VALID;


--
-- TOC entry 4775 (class 2606 OID 32885)
-- Name: character_permission_groups permissiongroupid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permission_groups
    ADD CONSTRAINT permissiongroupid FOREIGN KEY (permissiongroupid) REFERENCES public.permission_groups(id) NOT VALID;


--
-- TOC entry 4773 (class 2606 OID 32875)
-- Name: character_permissions permissionid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.character_permissions
    ADD CONSTRAINT permissionid FOREIGN KEY (permissionid) REFERENCES public.permissions(id) ON DELETE CASCADE NOT VALID;


--
-- TOC entry 4771 (class 2606 OID 32895)
-- Name: permission_group_data permissionid; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_data
    ADD CONSTRAINT permissionid FOREIGN KEY (permissionid) REFERENCES public.permissions(id) ON DELETE CASCADE NOT VALID;


-- Completed on 2024-07-31 15:30:22

--
-- PostgreSQL database dump complete
--

