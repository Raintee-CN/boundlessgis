import { useEffect, useMemo, useRef, useState } from "react";
import * as maplibregl from "maplibre-gl";

const API = "";

function bboxFromLayer(layer) {
  if ([layer.minx, layer.miny, layer.maxx, layer.maxy].some((v) => v == null)) {
    return null;
  }
  return [
    [layer.minx, layer.miny],
    [layer.maxx, layer.maxy],
  ];
}

export default function App() {
  const mapRef = useRef(null);
  const mapObj = useRef(null);
  const [layers, setLayers] = useState([]);
  const [activeId, setActiveId] = useState(null);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState("上传 shapefile zip，发布为 MLT 瓦片后可在地图上查看。");
  const [drag, setDrag] = useState(false);
  const fileRef = useRef(null);

  const active = useMemo(
    () => layers.find((l) => l.id === activeId) || null,
    [layers, activeId]
  );

  async function refresh() {
    const res = await fetch(`${API}/api/layers`);
    const data = await res.json();
    setLayers(data.layers || []);
    if (!activeId && data.layers?.length) setActiveId(data.layers[0].id);
  }

  useEffect(() => {
    refresh().catch((err) => setStatus(err.message));
  }, []);

  useEffect(() => {
    const map = new maplibregl.Map({
      container: mapRef.current,
      style: {
        version: 8,
        sources: {
          osm: {
            type: "raster",
            tiles: ["https://tile.openstreetmap.org/{z}/{x}/{y}.png"],
            tileSize: 256,
            attribution: "© OpenStreetMap",
          },
        },
        layers: [{ id: "osm", type: "raster", source: "osm" }],
      },
      center: [104.2, 35.6],
      zoom: 3.4,
    });
    map.addControl(new maplibregl.NavigationControl(), "bottom-right");
    mapObj.current = map;
    return () => map.remove();
  }, []);

  useEffect(() => {
    const map = mapObj.current;
    if (!map) return;

    const apply = () => {
      if (map.getLayer("mlt-fill")) map.removeLayer("mlt-fill");
      if (map.getLayer("mlt-line")) map.removeLayer("mlt-line");
      if (map.getLayer("mlt-point")) map.removeLayer("mlt-point");
      if (map.getSource("mlt")) map.removeSource("mlt");
      if (!active) return;

      map.addSource("mlt", {
        type: "vector",
        tiles: [`${window.location.origin}${API}/tiles/${active.id}/{z}/{x}/{y}.mlt`],
        minzoom: 0,
        maxzoom: 16,
        encoding: "mlt",
      });
      map.addLayer({
        id: "mlt-fill",
        type: "fill",
        source: "mlt",
        "source-layer": active.id,
        paint: { "fill-color": "#38bdf8", "fill-opacity": 0.28 },
      });
      map.addLayer({
        id: "mlt-line",
        type: "line",
        source: "mlt",
        "source-layer": active.id,
        paint: { "line-color": "#7dd3fc", "line-width": 1.6 },
      });
      map.addLayer({
        id: "mlt-point",
        type: "circle",
        source: "mlt",
        "source-layer": active.id,
        paint: {
          "circle-radius": 4.5,
          "circle-color": "#fbbf24",
          "circle-stroke-width": 1,
          "circle-stroke-color": "#0f172a",
        },
      });
      const bbox = bboxFromLayer(active);
      if (bbox) map.fitBounds(bbox, { padding: 48, duration: 700, maxZoom: 12 });
    };

    if (map.isStyleLoaded()) apply();
    else map.once("load", apply);
  }, [active]);

  async function upload(file) {
    if (!file) return;
    setBusy(true);
    setStatus(`正在导入 ${file.name} ...`);
    const body = new FormData();
    body.append("file", file);
    try {
      const res = await fetch(`${API}/api/layers`, { method: "POST", body });
      const data = await res.json();
      if (!res.ok) throw new Error(data.detail || "导入失败");
      await refresh();
      setActiveId(data.layer.id);
      setStatus(`已发布 ${data.layer.name}，要素 ${data.layer.feature_count}`);
    } catch (err) {
      setStatus(err.message);
    } finally {
      setBusy(false);
    }
  }

  async function removeLayer(id) {
    setBusy(true);
    try {
      const res = await fetch(`${API}/api/layers/${id}`, { method: "DELETE" });
      if (!res.ok) throw new Error("删除失败");
      const next = layers.filter((l) => l.id !== id);
      setLayers(next);
      setActiveId(next[0]?.id || null);
      setStatus("图层已删除");
    } catch (err) {
      setStatus(err.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="app">
      <aside className="sidebar">
        <div className="brand">
          <h1>BoundlessGIS MLT Lab</h1>
          <p>上传 Shapefile zip，经 PostGIS `ST_AsMLT` 发布为 MapLibre Tile，并在地图上加载。</p>
        </div>

        <label
          className={`dropzone ${drag ? "active" : ""}`}
          onDragOver={(e) => {
            e.preventDefault();
            setDrag(true);
          }}
          onDragLeave={() => setDrag(false)}
          onDrop={(e) => {
            e.preventDefault();
            setDrag(false);
            upload(e.dataTransfer.files[0]);
          }}
        >
          <input
            ref={fileRef}
            className="hidden"
            type="file"
            accept=".zip"
            onChange={(e) => upload(e.target.files[0])}
          />
          <div>
            <strong>拖入或点击上传 shapefile.zip</strong>
            <span>需包含 .shp / .dbf / .shx，建议附带 .prj</span>
          </div>
        </label>

        <button className="btn primary" disabled={busy} onClick={() => fileRef.current?.click()}>
          {busy ? "处理中..." : "选择压缩包"}
        </button>
        <div className={`status ${status.includes("失败") ? "error" : "ok"}`}>{status}</div>

        <div className="layers">
          {layers.length === 0 && <div className="meta">还没有图层。</div>}
          {layers.map((layer) => (
            <article key={layer.id} className={`layer ${layer.id === activeId ? "active" : ""}`}>
              <h3>{layer.name}</h3>
              <div className="meta">
                {layer.geom_type} · {layer.feature_count} 要素
                <br />
                /tiles/{layer.id}/{"{z}/{x}/{y}"}.mlt
              </div>
              <div className="row">
                <button className="btn primary" onClick={() => setActiveId(layer.id)}>
                  加载到地图
                </button>
                <button className="btn ghost" onClick={() => removeLayer(layer.id)}>
                  删除
                </button>
              </div>
            </article>
          ))}
        </div>
      </aside>

      <div className="map-wrap">
        <div id="map" ref={mapRef} />
        <div className="hud">
          <div>当前图层</div>
          <b>{active ? active.name : "未选择"}</b>
        </div>
      </div>
    </div>
  );
}
