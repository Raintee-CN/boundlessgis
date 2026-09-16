# BoundlessGIS 函数文档

## 1. 扩展概览

`boundlessgis` 是一个独立的 PostgreSQL/PostGIS 扩展，当前提供将 PostGIS 几何要素聚合编码为 MapLibre Tile（MLT）的函数。

安装扩展：

```sql
CREATE EXTENSION boundlessgis;
```

检查版本：

```sql
SELECT extname, extversion
FROM pg_extension
WHERE extname = 'boundlessgis';
```

当前版本：`1.0`

## 2. ST_AsMLT

### 2.1 功能

`ST_AsMLT` 将一组 PostgreSQL 行记录编码为一个 MLT layer，并以 `bytea` 返回。

它的调用方式参考 PostGIS 的 `ST_AsMVT` 聚合函数：

```text
输入行记录
    -> 读取 geometry、ID 和属性字段
    -> 转换为 MLT Feature
    -> 聚合为 MLT Layer
    -> 返回 bytea
```

该函数只负责 MLT 编码，不负责：

- 坐标系转换
- 瓦片范围计算
- 几何裁剪
- 坐标量化
- 空间索引过滤

这些操作应在调用 `ST_AsMLT` 之前完成，通常使用 `ST_Transform`、`ST_TileEnvelope` 和 `ST_AsMVTGeom`。

### 2.2 函数签名

```sql
ST_AsMLT(anyelement) RETURNS bytea
ST_AsMLT(anyelement, text) RETURNS bytea
ST_AsMLT(anyelement, text, integer) RETURNS bytea
ST_AsMLT(anyelement, text, integer, text) RETURNS bytea
ST_AsMLT(anyelement, text, integer, text, text) RETURNS bytea
```

参数定义：

| 参数 | 类型 | 说明 |
|---|---|---|
| `row` | `anyelement` | 包含 geometry 的行记录，必须是复合行类型 |
| `layer_name` | `text` | MLT layer 名称，不能为 NULL 或空字符串 |
| `extent` | `integer` | 瓦片坐标范围，默认 `4096`，必须大于 0 |
| `geom_name` | `text` | geometry 字段名；不指定时自动查找 geometry 类型字段 |
| `id_name` | `text` | Feature ID 字段名；只接受 `smallint`、`integer`、`bigint` |

### 2.3 推荐调用形式

最小推荐形式：

```sql
SELECT ST_AsMLT(q, 'roads')
FROM (
    SELECT gid, geom
    FROM roads
) AS q;
```

指定 extent：

```sql
SELECT ST_AsMLT(q, 'roads', 4096)
FROM (
    SELECT gid, geom
    FROM roads
) AS q;
```

指定 geometry 字段和 ID 字段：

```sql
SELECT ST_AsMLT(q, 'roads', 4096, 'geom', 'gid')
FROM (
    SELECT gid, geom, road_type
    FROM roads
) AS q;
```

指定别名后的字段：

```sql
SELECT ST_AsMLT(tile_rows, 'landuse', 4096, 'shape', 'feature_id')
FROM (
    SELECT
        gid AS feature_id,
        geom AS shape,
        category,
        area_m2
    FROM landuse
) AS tile_rows;
```

### 2.4 与瓦片预处理结合

`ST_AsMLT` 接收的 geometry 应该已经是瓦片坐标中的整数几何。典型调用如下：

```sql
SELECT ST_AsMLT(q, 'roads', 4096, 'geom', 'gid') AS tile
FROM (
    SELECT
        gid,
        ST_AsMVTGeom(
            ST_Transform(r.geom, 3857),
            ST_TileEnvelope(10, 808, 420),
            4096,
            64,
            true
        ) AS geom
    FROM roads AS r
    WHERE r.geom && ST_Transform(
        ST_TileEnvelope(10, 808, 420),
        4326
    )
) AS q
WHERE q.geom IS NOT NULL;
```

这里的 `ST_AsMVTGeom` 负责：

- 投影后的瓦片坐标转换
- 瓦片范围裁剪
- buffer 处理
- 坐标量化

`ST_AsMLT` 只负责将处理后的行记录编码为 MLT。

### 2.5 输入行要求

第一个参数必须是行类型，例如：

```sql
SELECT ST_AsMLT(q, 'demo')
FROM (
    SELECT gid, geom, name
    FROM demo_poly
) AS q;
```

直接传递单个 geometry 不符合当前接口要求：

```sql
-- 不支持
SELECT ST_AsMLT(geom, 'demo')
FROM demo_poly;
```

函数会在第一次处理行时解析行布局，并缓存以下信息：

- geometry 字段位置
- ID 字段位置和基础类型
- 属性字段位置、名称和基础类型
- `ST_AsEWKB` 函数调用信息

### 2.6 Geometry 字段选择

如果未指定 `geom_name`，函数会自动查找基础类型为 PostGIS `geometry` 的字段。

如果行中存在多个 geometry 字段，建议显式指定：

```sql
SELECT ST_AsMLT(q, 'layer', 4096, 'tile_geom', 'gid')
FROM (
    SELECT
        gid,
        original_geom,
        tile_geom
    FROM prepared_features
) AS q;
```

指定的字段名必须与输入行的字段名完全一致。

找不到 geometry 字段时，函数抛出错误：

```text
ST_AsMLT: could not find geometry column
```

### 2.7 Feature ID

通过 `id_name` 指定 Feature ID 字段：

```sql
SELECT ST_AsMLT(q, 'buildings', 4096, 'geom', 'building_id')
FROM (
    SELECT building_id, geom
    FROM buildings
) AS q;
```

支持的 ID 类型：

- `smallint`
- `integer`
- `bigint`

行为：

- NULL ID 不写入 Feature ID
- 负数 ID 不写入 Feature ID
- 非负整数 ID 写入 MLT Feature ID

如果没有指定 `id_name`，MLT 中不包含 Feature ID 列。

### 2.8 属性字段

除了 geometry 和 ID 字段之外，其他未被忽略的字段会作为 Feature 属性编码。

当前直接支持：

- `boolean`
- `smallint`
- `integer`
- `bigint`
- `real`
- `double precision`
- `text`
- `varchar`
- `char`
- `cstring`

其他 PostgreSQL 类型会尝试调用该类型的输出函数，并以字符串形式写入 MLT。

NULL 属性不会写入该 Feature 的属性集合：

```text
NULL 属性
    -> 不生成属性值
    -> 不会写入字符串 "NULL"
```

### 2.9 支持的几何类型

当前 WKB/EWKB 解析器支持：

- `POINT`
- `LINESTRING`
- `POLYGON`
- `MULTIPOINT`
- `MULTILINESTRING`
- `MULTIPOLYGON`

支持以下输入特征：

- WKB 大端和小端字节序
- EWKB SRID 标记
- Z 维度标记
- M 维度标记
- Polygon 闭合点移除
- 多几何部件

Z/M 坐标会被读取并跳过，当前 MLT 输出只编码二维瓦片坐标。

空 geometry、NULL geometry 和无法转换的 geometry 不会加入输出 Feature。

### 2.10 空结果

当输入没有有效 Feature 时，返回空 `bytea`：

```sql
SELECT octet_length(
    ST_AsMLT(q, 'empty', 4096, 'geom', 'gid')
)
FROM (
    SELECT NULL::integer AS gid,
           NULL::geometry AS geom
) AS q;
```

结果为：

```text
0
```

### 2.11 Layer name 和 extent

`layer_name` 必须提供有效的非空字符串：

```sql
-- 错误：layer name 为空
SELECT ST_AsMLT(q, '', 4096, 'geom', 'gid')
FROM prepared_features AS q;
```

`extent` 必须大于 0：

```sql
-- 错误：extent 必须大于 0
SELECT ST_AsMLT(q, 'roads', 0, 'geom', 'gid')
FROM prepared_features AS q;
```

常用值为：

- `4096`：默认值，适合常见矢量瓦片
- `8192`：需要更高瓦片坐标精度时使用

extent 应与前置的 `ST_AsMVTGeom` 使用相同的值。

### 2.12 当前编码配置

当前扩展使用 MapLibre 官方 C++ encoder，配置如下：

```text
sortFeatures          = true
preTessellate         = false
includeOutlines       = true
enableMortonEncoding  = true
useFastPfor           = false
useFsst               = false
```

ID 是否编码由 `id_name` 是否有效决定。

这些参数目前不是 SQL 函数参数，调用方不能单独覆盖。

### 2.13 返回值和 HTTP 输出

SQL 函数返回：

```sql
bytea
```

HTTP 服务通常将返回值直接作为响应体，并使用：

```text
Content-Type: application/vnd.maplibre-vector-tile
```

示例：

```python
payload = bytes(row["tile"])
```

### 2.14 并行和稳定性

当前 `ST_AsMLT` 聚合函数声明为：

```sql
IMMUTABLE PARALLEL UNSAFE
```

原因是当前聚合状态包含 C++ 对象和 PostgreSQL 内存上下文，暂未实现 PostgreSQL parallel aggregate 的状态序列化、合并和安全并行执行。

不要在 PostgreSQL 聚合函数内部自行创建线程。

大数据场景建议在函数外部按瓦片或任务分块，并由 Go/Rust worker 进程并行调用数据库。

### 2.15 性能特征

当前性能测试显示：

- MLT 比 MVT 通常具有更小的未压缩体积
- 当前 MLT 编码 CPU 成本明显高于 MVT
- 主要耗时在 MLT encoder 的列式编码、几何流组织和 Morton 相关处理
- `ST_AsEWKB` 与 WKB 解析不是主要耗时来源

因此建议：

- 对热点瓦片启用结果缓存
- 对静态图层预生成 MLT 瓦片
- 避免无缓存地重复编码相同瓦片
- 生产环境中限制单瓦片要素数量和最大输出大小

## 3. 当前未提供的函数

当前扩展尚未实现以下能力：

- Clip
- Intersection
- Erase
- Identity
- Union / Dissolve
- Unary Union
- Buffer
- SymDifference
- Update
- Raster processing
- Map algebra
- 空间连接
- 大数据任务调度

这些能力目前应使用 PostGIS、GDAL/OGR 或外围 Go 服务实现，后续可继续加入 `boundlessgis`。

## 4. 完整测试示例

```sql
CREATE EXTENSION IF NOT EXISTS postgis;
CREATE EXTENSION IF NOT EXISTS boundlessgis;

WITH tile_rows AS (
    SELECT
        gid,
        name,
        ST_AsMVTGeom(
            ST_Transform(geom, 3857),
            ST_TileEnvelope(10, 808, 420),
            4096,
            64,
            true
        ) AS geom
    FROM demo_poly
    WHERE geom && ST_Transform(
        ST_TileEnvelope(10, 808, 420),
        4326
    )
)
SELECT octet_length(
    ST_AsMLT(tile_rows, 'demo', 4096, 'geom', 'gid')
) AS mlt_bytes
FROM tile_rows
WHERE geom IS NOT NULL;
```

实际聚合时应确保 `ST_AsMLT` 接收完整的行集合，而不是对每一行分别调用一次。

## 5. 源码位置

```text
ext/boundlessgis/src/asmlt.cpp
ext/boundlessgis/include/boundlessgis.hpp
ext/boundlessgis/sql/boundlessgis--1.0.sql
ext/boundlessgis/CMakeLists.txt
```

核心流程：

```text
PostgreSQL row
    -> ST_AsEWKB
    -> C++ WKB parser
    -> mlt::Encoder::Feature
    -> mlt::Encoder::Layer
    -> bytea
```
