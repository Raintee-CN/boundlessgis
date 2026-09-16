from pathlib import Path
import shapefile
import zipfile

root = Path(__file__).resolve().parent
shp = root / "demo_poly"
w = shapefile.Writer(str(shp), shapeType=shapefile.POLYGON)
w.field("name", "C", size=40)
w.field("code", "N", decimal=0)
w.poly([[(116.3, 39.8), (116.5, 39.8), (116.5, 40.0), (116.3, 40.0), (116.3, 39.8)]])
w.record("beijing-box", 1)
w.poly([[(121.4, 31.1), (121.6, 31.1), (121.6, 31.3), (121.4, 31.3), (121.4, 31.1)]])
w.record("shanghai-box", 2)
w.close()
(root / "demo_poly.prj").write_text(
    'GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]'
)
zip_path = root / "demo_poly.zip"
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for ext in [".shp", ".shx", ".dbf", ".prj"]:
        zf.write(shp.with_suffix(ext), f"demo_poly{ext}")
(root / "ok.txt").write_text(str(zip_path.stat().st_size))
