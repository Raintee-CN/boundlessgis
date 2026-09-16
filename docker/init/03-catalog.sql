CREATE TABLE IF NOT EXISTS mlt_layers (
  id text PRIMARY KEY,
  name text NOT NULL,
  table_name text NOT NULL,
  geom_column text NOT NULL DEFAULT 'geom',
  id_column text NOT NULL DEFAULT 'gid',
  srid integer NOT NULL DEFAULT 4326,
  geom_type text NOT NULL DEFAULT 'GEOMETRY',
  feature_count bigint NOT NULL DEFAULT 0,
  minx double precision,
  miny double precision,
  maxx double precision,
  maxy double precision,
  created_at timestamptz NOT NULL DEFAULT now()
);
