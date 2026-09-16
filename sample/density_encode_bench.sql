-- Compare MVT and MLT on synthetic polygon layers with increasing feature density.
-- Geometry is prepared once; timed queries measure aggregate encoding only.
\set ON_ERROR_STOP on
\timing off

DROP TABLE IF EXISTS bench_density;
CREATE TEMP TABLE bench_density (
  density integer NOT NULL,
  gid integer NOT NULL,
  geom geometry NOT NULL
);

-- Keep every square inside one tile.  The four levels are 10x10 through 80x80.
INSERT INTO bench_density (density, gid, geom)
SELECT side * side,
       row_number() OVER (ORDER BY side, iy, ix)::integer,
       ST_AsMVTGeom(
         ST_MakeEnvelope(
           tile.xmin + (ix + 1) * step,
           tile.ymin + (iy + 1) * step,
           tile.xmin + (ix + 1) * step + step * 0.72,
           tile.ymin + (iy + 1) * step + step * 0.72,
           3857
         ),
         tile.box,
         4096, 64, true
       )
FROM (VALUES (10), (20), (40), (80)) AS levels(side)
CROSS JOIN LATERAL (
  SELECT ST_TileEnvelope(10, 808, 420) AS box,
         ST_XMin(ST_TileEnvelope(10, 808, 420)) AS xmin,
         ST_YMin(ST_TileEnvelope(10, 808, 420)) AS ymin,
         ST_XMax(ST_TileEnvelope(10, 808, 420)) - ST_XMin(ST_TileEnvelope(10, 808, 420)) AS width
) AS tile
CROSS JOIN LATERAL (SELECT tile.width / (side + 2) AS step) AS sizing
CROSS JOIN generate_series(0, side - 1) AS ix
CROSS JOIN generate_series(0, side - 1) AS iy;

DELETE FROM bench_density WHERE geom IS NULL;

SELECT d.density, d.density AS features,
       octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) AS mvt_bytes,
       octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) AS mlt_bytes
FROM (SELECT DISTINCT density FROM bench_density) d
CROSS JOIN LATERAL (
  SELECT density, gid, geom FROM bench_density WHERE bench_density.density = d.density
) q
GROUP BY d.density
ORDER BY d.density;

-- Warm each density once before timing.
SELECT d.density, octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) AS bytes
FROM (SELECT DISTINCT density FROM bench_density) d
CROSS JOIN LATERAL (
  SELECT density, gid, geom FROM bench_density WHERE bench_density.density = d.density
) q
GROUP BY d.density ORDER BY d.density;
SELECT d.density, octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) AS bytes
FROM (SELECT DISTINCT density FROM bench_density) d
CROSS JOIN LATERAL (
  SELECT density, gid, geom FROM bench_density WHERE bench_density.density = d.density
) q
GROUP BY d.density ORDER BY d.density;

\echo ===== MVT and MLT x5 by density =====
\timing on
\echo --- density 100 / MVT ---
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
\echo --- density 100 / MLT ---
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid'))
FROM (SELECT gid, geom, density FROM bench_density WHERE density = 100) q;
\echo --- density 400 / MVT ---
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMVT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
\echo --- density 400 / MLT ---
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
SELECT octet_length(ST_AsMLT(q, 'density', 4096, 'geom', 'gid')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = 400) q;
\timing off

-- Per-density timings.  The server-side clock avoids network timing noise.
DO $$
DECLARE
  d integer;
  i integer;
  started timestamptz;
  elapsed double precision;
  ignored integer;
BEGIN
  FOREACH d IN ARRAY ARRAY[100, 400, 1600, 6400] LOOP
    started := clock_timestamp();
    FOR i IN 1..5 LOOP
      EXECUTE format(
        'SELECT octet_length(ST_AsMVT(q, ''density'', 4096, ''geom'', ''gid'')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = %s) q', d
      ) INTO ignored;
    END LOOP;
    elapsed := extract(epoch FROM clock_timestamp() - started) * 1000;
    RAISE NOTICE 'MVT density=% features=% x5: % ms', d, d, round(elapsed::numeric, 3);

    started := clock_timestamp();
    FOR i IN 1..5 LOOP
      EXECUTE format(
        'SELECT octet_length(ST_AsMLT(q, ''density'', 4096, ''geom'', ''gid'')) FROM (SELECT gid, geom, density FROM bench_density WHERE density = %s) q', d
      ) INTO ignored;
    END LOOP;
    elapsed := extract(epoch FROM clock_timestamp() - started) * 1000;
    RAISE NOTICE 'MLT density=% features=% x5: % ms', d, d, round(elapsed::numeric, 3);
  END LOOP;
END $$;

DROP TABLE bench_density;
