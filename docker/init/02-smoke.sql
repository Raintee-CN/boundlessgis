CREATE TABLE IF NOT EXISTS mlt_smoke (
  id integer PRIMARY KEY,
  name text,
  geom geometry(Geometry, 3857)
);

INSERT INTO mlt_smoke (id, name, geom) VALUES
  (1, 'pt', ST_SetSRID(ST_MakePoint(100, 200), 3857)),
  (2, 'ln', ST_SetSRID(ST_GeomFromText('LINESTRING(0 0, 100 0, 100 100)'), 3857)),
  (3, 'py', ST_SetSRID(ST_GeomFromText('POLYGON((0 0, 80 0, 80 80, 0 80, 0 0))'), 3857))
ON CONFLICT (id) DO NOTHING;
