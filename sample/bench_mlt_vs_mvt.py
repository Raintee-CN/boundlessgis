from __future__ import annotations

import json
import math
import statistics
import time
import urllib.request
from pathlib import Path

BASE = "http://127.0.0.1:5178"
OUT = Path(__file__).resolve().parent / "bench_result.txt"


def lonlat_to_tile(lon: float, lat: float, z: int) -> tuple[int, int]:
    n = 2**z
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    return x, y


def fetch(url: str) -> tuple[int, float]:
    started = time.perf_counter()
    with urllib.request.urlopen(url, timeout=120) as resp:
        data = resp.read()
    elapsed_ms = (time.perf_counter() - started) * 1000
    return len(data), elapsed_ms


def measure(url: str, rounds: int = 7) -> dict:
    sizes = []
    times = []
    for i in range(rounds):
        size, ms = fetch(url)
        sizes.append(size)
        if i > 0:
            times.append(ms)
    return {
        "bytes": sizes[-1],
        "ms_avg": statistics.mean(times),
        "ms_p50": statistics.median(times),
        "ms_min": min(times),
        "ms_max": max(times),
    }


def main() -> None:
    layers = json.loads(urllib.request.urlopen(f"{BASE}/api/layers").read())["layers"]
    layer = max(layers, key=lambda x: x["feature_count"])
    lon = (layer["minx"] + layer["maxx"]) / 2
    lat = (layer["miny"] + layer["maxy"]) / 2
    zooms = [8, 10, 12, 14]
    lines = [
        f"layer={layer['name']} id={layer['id']} features={layer['feature_count']}",
        f"center={lon:.6f},{lat:.6f}",
        "first request discarded as warmup; remaining 6 averaged",
        "",
        f"{'z/x/y':<16} {'MVT B':>10} {'MLT B':>10} {'save':>8} {'MVT ms':>10} {'MLT ms':>10} {'speedup':>8}",
    ]
    totals = {"mvt": 0, "mlt": 0, "mvt_ms": 0.0, "mlt_ms": 0.0, "n": 0}
    for z in zooms:
        x, y = lonlat_to_tile(lon, lat, z)
        mvt = measure(f"{BASE}/tiles/{layer['id']}/{z}/{x}/{y}.mvt")
        mlt = measure(f"{BASE}/tiles/{layer['id']}/{z}/{x}/{y}.mlt")
        save = 0.0 if mvt["bytes"] == 0 else (1 - mlt["bytes"] / mvt["bytes"]) * 100
        speed = 0.0 if mlt["ms_avg"] == 0 else mvt["ms_avg"] / mlt["ms_avg"]
        lines.append(
            f"{z}/{x}/{y:<12} {mvt['bytes']:10d} {mlt['bytes']:10d} {save:7.1f}% "
            f"{mvt['ms_avg']:10.1f} {mlt['ms_avg']:10.1f} {speed:7.2f}x"
        )
        totals["mvt"] += mvt["bytes"]
        totals["mlt"] += mlt["bytes"]
        totals["mvt_ms"] += mvt["ms_avg"]
        totals["mlt_ms"] += mlt["ms_avg"]
        totals["n"] += 1
    save = 0.0 if totals["mvt"] == 0 else (1 - totals["mlt"] / totals["mvt"]) * 100
    speed = 0.0 if totals["mlt_ms"] == 0 else totals["mvt_ms"] / totals["mlt_ms"]
    lines += [
        "",
        f"SUM bytes  MVT={totals['mvt']}  MLT={totals['mlt']}  bandwidth save={save:.1f}%",
        f"SUM avg ms MVT={totals['mvt_ms']:.1f}  MLT={totals['mlt_ms']:.1f}  encode speedup={speed:.2f}x",
        "",
        "Note: time includes HTTP + PostGIS ST_AsMVTGeom clip/quantize + encode.",
        "Bandwidth is uncompressed tile body size (no gzip).",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
