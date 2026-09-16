# BoundlessGIS

BoundlessGIS is an experimental PostgreSQL/PostGIS extension for encoding
[MapLibre Tiles (MLT)](https://github.com/maplibre/maplibre-tile-spec) directly
from SQL. It includes a small web application for importing Shapefiles and
serving XYZ MLT tiles.

## Features

- `ST_AsMLT`: aggregate PostGIS rows into an MLT layer returned as `bytea`
- Shapefile ZIP import through GDAL/OGR
- XYZ endpoints for side-by-side MLT and MVT output
- MapLibre GL web viewer
- Reproducible PostgreSQL 18 / PostGIS 3.6 Docker environment

> [!IMPORTANT]
> This project is experimental. The MLT encoder currently favors smaller tile
> output over encoding speed and can be slower than PostGIS `ST_AsMVT`. Benchmark
> your own data before production use.

## Quick Start

Prerequisites:

- Git
- Docker with Docker Compose

Clone the repository and its encoder dependency:

```bash
git clone --recurse-submodules <repository-url>
cd boundlessgis
docker compose up --build
```

Open <http://localhost:5178>. PostgreSQL is available on port `54329` with the
development credentials defined in `docker-compose.yml`.

If the repository was cloned without submodules, initialize them before building:

```bash
git submodule update --init --recursive
```

Stop the stack without deleting its database volume:

```bash
docker compose down
```

## SQL Usage

The Docker image installs both `postgis` and `boundlessgis` automatically. For a
manual database installation, enable them in this order:

```sql
CREATE EXTENSION postgis;
CREATE EXTENSION boundlessgis;
```

Encode rows that are already clipped and transformed into tile coordinates:

```sql
SELECT ST_AsMLT(tile_rows, 'roads', 4096, 'geom', 'gid')
FROM (
  SELECT
    gid,
    road_type,
    ST_AsMVTGeom(
      ST_Transform(geom, 3857),
      ST_TileEnvelope(10, 808, 420),
      4096,
      64,
      true
    ) AS geom
  FROM roads
  WHERE geom && ST_Transform(ST_TileEnvelope(10, 808, 420), 4326)
) AS tile_rows
WHERE geom IS NOT NULL;
```

`ST_AsMLT` performs encoding only. Coordinate transformation, spatial filtering,
clipping and quantization must happen before the aggregate is called.

See [the function reference](docs/boundlessgis-functions.md) for signatures,
supported types and detailed behavior.

## Tile API

After importing a Shapefile ZIP in the web interface, tiles are available at:

```text
GET /tiles/{layer_id}/{z}/{x}/{y}.mlt
GET /tiles/{layer_id}/{z}/{x}/{y}.mvt
```

The MLT response uses `application/vnd.maplibre-vector-tile`. Uploaded data is
converted to EPSG:4326, indexed with GiST, transformed to EPSG:3857 per tile and
prepared with `ST_AsMVTGeom` before encoding.

The demo service is intended for local evaluation. It has no authentication,
uses development database credentials and disables HTTP tile caching.

## Build the Extension

The supported native build uses CMake. Required tools and libraries include a
C++20 compiler, CMake 3.25+, PostgreSQL server development headers, PostGIS,
and the initialized `maplibre-tile-spec` submodule.

```bash
cmake -S ext/boundlessgis -B ext/boundlessgis/build \
  -DMLT_WITH_JSON=OFF \
  -DMLT_WITH_TESTS=OFF \
  -DMLT_WITH_TOOLS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build ext/boundlessgis/build --target boundlessgis
cmake --install ext/boundlessgis/build
```

The Docker build is the reference build and pins PostgreSQL 18 with PostGIS 3.6.

## Tests

With the Compose stack running:

```bash
docker exec boundlessgis-pg psql -U postgres -d boundlessgis \
  -f /opt/boundlessgis/test-st-asmlt.sql
```

Build the frontend independently with:

```bash
cd web
npm ci
npm run build
```

## Repository Layout

```text
ext/boundlessgis/  PostgreSQL extension source and SQL install scripts
docker/            Extension image, initialization and SQL tests
service/           FastAPI import and tile service
web/               React and MapLibre GL interface
sample/            Sample Shapefile and benchmark scripts
docs/              Function reference
```

## Dependency and License

BoundlessGIS is licensed under the [Apache License 2.0](LICENSE).

`maplibre-tile-spec` is included as a Git submodule and retains its own upstream
licenses. Review the dependency's license files before redistributing binaries.
