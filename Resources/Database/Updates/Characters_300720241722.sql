ALTER TABLE public.characters
    ADD position_x numeric NOT NULL DEFAULT 0.0,
    ADD position_y numeric NOT NULL DEFAULT 0.0,
    ADD position_z numeric NOT NULL DEFAULT 0.0,
    ADD position_o numeric NOT NULL DEFAULT 0.0;