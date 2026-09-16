\set ON_ERROR_STOP on

SELECT postgis_full_version();
SELECT extname, extversion FROM pg_extension WHERE extname IN ('postgis', 'boundlessgis');

SELECT octet_length(ST_AsMLT(q, 'layer', 4096, 'geom', 'id')) AS mlt_bytes
FROM (
  SELECT
    ST_AsMVTGeom(geom, ST_MakeBox2D(ST_Point(0, 0), ST_Point(4096, 4096)), 4096, 256, true) AS geom,
    id,
    name
  FROM mlt_smoke
) q;

SELECT encode(ST_AsMLT(q), 'hex') AS mlt_hex
FROM (
  SELECT ST_AsMVTGeom(geom, ST_MakeBox2D(ST_Point(0, 0), ST_Point(4096, 4096)), 4096, 0, false) AS geom
  FROM mlt_smoke
  WHERE id = 1
) q;
