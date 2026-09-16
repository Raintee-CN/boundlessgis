import os
import re
import shutil
import subprocess
import tempfile
import uuid
import zipfile
from pathlib import Path

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles
from psycopg.rows import dict_row
import psycopg

DATABASE_URL = os.environ.get(
    "DATABASE_URL",
    "postgresql://postgres:postgres@postgis:5432/boundlessgis",
)
UPLOAD_LIMIT = 80 * 1024 * 1024

app = FastAPI(title="BoundlessGIS MLT Lab")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


def connect():
    return psycopg.connect(DATABASE_URL, row_factory=dict_row)


def slugify(name: str) -> str:
    base = Path(name).stem.lower()
    base = re.sub(r"[^a-z0-9]+", "_", base).strip("_")
    return (base or "layer")[:40]


def find_shp(root: Path) -> Path:
    matches = list(root.rglob("*.shp"))
    if not matches:
        raise HTTPException(400, "zip 里没有找到 .shp 文件")
    return matches[0]


def pg_dsn() -> str:
    from urllib.parse import urlparse

    u = urlparse(DATABASE_URL)
    return (
        f"PG:host={u.hostname} port={u.port or 5432} dbname={u.path.lstrip('/')} "
        f"user={u.username} password={u.password}"
    )


def run_ogr2ogr(shp: Path, table_name: str) -> None:
    cmd = [
        "ogr2ogr",
        "-f",
        "PostgreSQL",
        pg_dsn(),
        str(shp),
        "-nln",
        table_name,
        "-lco",
        "GEOMETRY_NAME=geom",
        "-lco",
        "FID=gid",
        "-nlt",
        "PROMOTE_TO_MULTI",
        "-dim",
        "XY",
        "-t_srs",
        "EPSG:4326",
        "-overwrite",
    ]
    env = os.environ.copy()
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "ogr2ogr failed")[-4000:]
        raise HTTPException(400, f"导入失败: {detail}")


def layer_bounds_sql(table_name: str) -> str:
    return f"""
        SELECT
          COUNT(*)::bigint AS feature_count,
          ST_SRID(geom) AS srid,
          GeometryType(ST_CollectionExtract(ST_Collect(geom), 3)) AS geom_type,
          ST_XMin(ext) AS minx,
          ST_YMin(ext) AS miny,
          ST_XMax(ext) AS maxx,
          ST_YMax(ext) AS maxy
        FROM {table_name}, LATERAL (
          SELECT ST_Extent(geom) AS ext FROM {table_name}
        ) e
        GROUP BY ext, ST_SRID(geom)
    """


@app.get("/api/health")
def health():
    with connect() as conn:
        conn.execute("SELECT 1")
    return {"ok": True}


@app.get("/api/layers")
def list_layers():
    with connect() as conn:
        rows = conn.execute(
            "SELECT * FROM mlt_layers ORDER BY created_at DESC"
        ).fetchall()
    return {"layers": rows}


@app.delete("/api/layers/{layer_id}")
def delete_layer(layer_id: str):
    with connect() as conn:
        row = conn.execute(
            "SELECT table_name FROM mlt_layers WHERE id = %s", (layer_id,)
        ).fetchone()
        if not row:
            raise HTTPException(404, "图层不存在")
        conn.execute(f'DROP TABLE IF EXISTS "{row["table_name"]}" CASCADE')
        conn.execute("DELETE FROM mlt_layers WHERE id = %s", (layer_id,))
        conn.commit()
    return {"ok": True}


@app.post("/api/layers")
async def import_layer(file: UploadFile = File(...)):
    if not file.filename or not file.filename.lower().endswith(".zip"):
        raise HTTPException(400, "请上传 shapefile 的 zip 压缩包")

    data = await file.read()
    if len(data) > UPLOAD_LIMIT:
        raise HTTPException(400, "文件超过 80MB")

    layer_id = uuid.uuid4().hex[:10]
    table_name = f"mlt_{slugify(file.filename)}_{layer_id}"
    tmp = Path(tempfile.mkdtemp(prefix="shpzip-"))
    try:
        zip_path = tmp / "upload.zip"
        zip_path.write_bytes(data)
        extract_dir = tmp / "src"
        extract_dir.mkdir()
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_dir)
        shp = find_shp(extract_dir)
        run_ogr2ogr(shp, table_name)

        with connect() as conn:
            conn.execute(
                f'ALTER TABLE "{table_name}" ALTER COLUMN geom TYPE geometry(Geometry, 4326) USING ST_SetSRID(geom, 4326)'
            )
            conn.execute(
                f'CREATE INDEX IF NOT EXISTS "{table_name}_geom_idx" ON "{table_name}" USING GIST (geom)'
            )
            conn.execute(f'ANALYZE "{table_name}"')
            stats = conn.execute(
                f"""
                SELECT
                  COUNT(*)::bigint AS feature_count,
                  COALESCE((SELECT ST_SRID(geom) FROM "{table_name}" WHERE geom IS NOT NULL LIMIT 1), 4326) AS srid,
                  COALESCE((SELECT GeometryType(geom) FROM "{table_name}" WHERE geom IS NOT NULL LIMIT 1), 'GEOMETRY') AS geom_type,
                  ST_XMin(ST_Extent(geom)) AS minx,
                  ST_YMin(ST_Extent(geom)) AS miny,
                  ST_XMax(ST_Extent(geom)) AS maxx,
                  ST_YMax(ST_Extent(geom)) AS maxy
                FROM "{table_name}"
                """
            ).fetchone()
            conn.execute(
                """
                INSERT INTO mlt_layers (
                  id, name, table_name, geom_column, id_column, srid, geom_type,
                  feature_count, minx, miny, maxx, maxy
                ) VALUES (%s,%s,%s,'geom','gid',%s,%s,%s,%s,%s,%s,%s)
                """,
                (
                    layer_id,
                    Path(file.filename).stem,
                    table_name,
                    stats["srid"] or 4326,
                    stats["geom_type"] or "GEOMETRY",
                    stats["feature_count"] or 0,
                    stats["minx"],
                    stats["miny"],
                    stats["maxx"],
                    stats["maxy"],
                ),
            )
            conn.commit()
            layer = conn.execute(
                "SELECT * FROM mlt_layers WHERE id = %s", (layer_id,)
            ).fetchone()
        return {"layer": layer}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def encode_tile(layer_id: str, z: int, x: int, y: int, fmt: str) -> bytes:
    if z < 0 or z > 22 or x < 0 or y < 0:
        raise HTTPException(400, "invalid tile")
    encoder = "ST_AsMLT" if fmt == "mlt" else "ST_AsMVT"
    with connect() as conn:
        layer = conn.execute(
            "SELECT * FROM mlt_layers WHERE id = %s", (layer_id,)
        ).fetchone()
        if not layer:
            raise HTTPException(404, "图层不存在")
        table = layer["table_name"]
        sql = f"""
            SELECT {encoder}(q, %s, 4096, 'geom', 'gid') AS tile
            FROM (
              SELECT
                gid,
                ST_AsMVTGeom(
                  ST_Transform(geom, 3857),
                  ST_TileEnvelope(%s, %s, %s),
                  4096,
                  64,
                  true
                ) AS geom
              FROM "{table}"
              WHERE geom && ST_Transform(ST_TileEnvelope(%s, %s, %s), 4326)
            ) q
            WHERE q.geom IS NOT NULL
        """
        row = conn.execute(sql, (layer_id, z, x, y, z, x, y)).fetchone()
        return bytes(row["tile"]) if row and row["tile"] is not None else b""


@app.get("/tiles/{layer_id}/{z}/{x}/{y}.mlt")
def tile_mlt(layer_id: str, z: int, x: int, y: int):
    payload = encode_tile(layer_id, z, x, y, "mlt")
    return Response(
        content=payload,
        media_type="application/vnd.maplibre-vector-tile",
        headers={
            "Cache-Control": "no-store",
            "Access-Control-Allow-Origin": "*",
        },
    )


@app.get("/tiles/{layer_id}/{z}/{x}/{y}.mvt")
def tile_mvt(layer_id: str, z: int, x: int, y: int):
    payload = encode_tile(layer_id, z, x, y, "mvt")
    return Response(
        content=payload,
        media_type="application/vnd.mapbox-vector-tile",
        headers={
            "Cache-Control": "no-store",
            "Access-Control-Allow-Origin": "*",
        },
    )


STATIC_DIR = Path(os.environ.get("STATIC_DIR", "/app/static"))
if STATIC_DIR.exists():
    assets_dir = STATIC_DIR / "assets"
    if assets_dir.exists():
        app.mount("/assets", StaticFiles(directory=assets_dir), name="assets")

    @app.get("/")
    def index():
        return FileResponse(STATIC_DIR / "index.html")
