-- Isolated encode-only benchmark: geometry already clipped/quantized.
-- Measures ST_AsMLT vs ST_AsMVT on identical prepared rows, no ST_AsMVTGeom,
-- no HTTP. Run inside one psql session.
\set ON_ERROR_STOP on
\timing off

DROP TABLE IF EXISTS bench_clip;
CREATE TABLE bench_clip AS
SELECT gid,
       ST_AsMVTGeom(ST_Transform(geom, 3857), ST_TileEnvelope(10, 808, 420), 4096, 64, true) AS geom
FROM mlt_pewgh_aa7d1c07f2
WHERE geom && ST_Transform(ST_TileEnvelope(10, 808, 420), 4326);
DELETE FROM bench_clip WHERE geom IS NULL;
SELECT count(*) AS prepared_features FROM bench_clip;

-- warmup (not timed)
SELECT sum(octet_length(t)) FROM (
  SELECT ST_AsMVT(q, 'pewgh', 4096, 'geom', 'gid') AS t
  FROM generate_series(1, 3) s, LATERAL (SELECT gid, geom FROM bench_clip) q GROUP BY s
) w;
SELECT sum(octet_length(t)) FROM (
  SELECT ST_AsMLT(q, 'pewgh', 4096, 'geom', 'gid') AS t
  FROM generate_series(1, 3) s, LATERAL (SELECT gid, geom FROM bench_clip) q GROUP BY s
) w;

\echo ===== MVT encode x30 =====
\timing on
SELECT sum(octet_length(t)) FROM (
  SELECT ST_AsMVT(q, 'pewgh', 4096, 'geom', 'gid') AS t
  FROM generate_series(1, 30) s, LATERAL (SELECT gid, geom FROM bench_clip) q GROUP BY s
) w;
\timing off

\echo ===== MLT encode x30 =====
\timing on
SELECT sum(octet_length(t)) FROM (
  SELECT ST_AsMLT(q, 'pewgh', 4096, 'geom', 'gid') AS t
  FROM generate_series(1, 30) s, LATERAL (SELECT gid, geom FROM bench_clip) q GROUP BY s
) w;
\timing off

DROP TABLE bench_clip;
