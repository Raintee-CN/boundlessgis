extern "C"
{
#include "postgres.h"
#include "fmgr.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_type.h"
#include "executor/tuptable.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "nodes/pg_list.h"
#include "parser/parse_func.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "catalog/pg_namespace.h"
}

#include "boundlessgis.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

extern "C"
{
	PG_MODULE_MAGIC;
}

namespace
{

constexpr uint32_t kDefaultExtent = 4096;

std::string
text_to_std_string(text *t)
{
	char *c = text_to_cstring(t);
	std::string out(c);
	pfree(c);
	return out;
}

constexpr uint32_t WKB_Z = 0x80000000u;
constexpr uint32_t WKB_M = 0x40000000u;
constexpr uint32_t WKB_SRID = 0x20000000u;

struct WkbReader
{
	const uint8_t *data;
	size_t size;
	size_t pos{0};
	bool swap{false};

	explicit WkbReader(const bytea *ba)
		: data(reinterpret_cast<const uint8_t *>(VARDATA_ANY(ba))), size(VARSIZE_ANY_EXHDR(ba))
	{
	}

	void check(size_t n) const
	{
		if (pos + n > size)
			throw std::runtime_error("truncated EWKB");
	}

	uint8_t u8()
	{
		check(1);
		return data[pos++];
	}

	uint32_t u32()
	{
		check(4);
		uint32_t v;
		memcpy(&v, data + pos, 4);
		pos += 4;
		if (swap)
			v = __builtin_bswap32(v);
		return v;
	}

	double f64()
	{
		check(8);
		uint64_t bits;
		memcpy(&bits, data + pos, 8);
		pos += 8;
		if (swap)
			bits = __builtin_bswap64(bits);
		double v;
		memcpy(&v, &bits, 8);
		return v;
	}

	mlt::Encoder::Vertex vertex(bool has_z, bool has_m)
	{
		const double x = f64();
		const double y = f64();
		if (has_z)
			f64();
		if (has_m)
			f64();
		return {static_cast<int32_t>(llround(x)), static_cast<int32_t>(llround(y))};
	}

	std::vector<mlt::Encoder::Vertex> points(bool has_z, bool has_m, bool drop_closing)
	{
		const uint32_t n = u32();
		std::vector<mlt::Encoder::Vertex> pts;
		pts.reserve(n);
		for (uint32_t i = 0; i < n; ++i)
			pts.push_back(vertex(has_z, has_m));
		if (drop_closing && pts.size() >= 2 && pts.front().x == pts.back().x && pts.front().y == pts.back().y)
			pts.pop_back();
		return pts;
	}
};

bool
read_wkb_geometry(WkbReader &r, mlt::Encoder::Geometry &out, int depth)
{
	if (depth > 16)
		return false;

	const uint8_t endian = r.u8();
	r.swap = (endian == 0);
	const uint32_t type_word = r.u32();
	const bool has_z = (type_word & WKB_Z) != 0 || ((type_word & 0x0FFFFFFFu) / 1000) == 1 || ((type_word & 0x0FFFFFFFu) / 1000) == 3;
	const bool has_m = (type_word & WKB_M) != 0 || ((type_word & 0x0FFFFFFFu) / 1000) == 2 || ((type_word & 0x0FFFFFFFu) / 1000) == 3;
	if (type_word & WKB_SRID)
		r.u32();

	const uint32_t wkb_type = (type_word & 0x0FFFFFFFu) % 1000;
	out = {};

	switch (wkb_type)
	{
	case 1:
		out.type = mlt::Encoder::GeometryType::POINT;
		out.coordinates.push_back(r.vertex(has_z, has_m));
		return true;
	case 2:
	{
		out.type = mlt::Encoder::GeometryType::LINESTRING;
		out.coordinates = r.points(has_z, has_m, false);
		return out.coordinates.size() >= 2;
	}
	case 3:
	{
		out.type = mlt::Encoder::GeometryType::POLYGON;
		const uint32_t nrings = r.u32();
		for (uint32_t i = 0; i < nrings; ++i)
		{
			auto ring = r.points(has_z, has_m, true);
			if (ring.size() < 3)
				return false;
			out.ringSizes.push_back(static_cast<uint32_t>(ring.size()));
			out.coordinates.insert(out.coordinates.end(), ring.begin(), ring.end());
		}
		return !out.ringSizes.empty();
	}
	case 4:
	{
		out.type = mlt::Encoder::GeometryType::MULTIPOINT;
		const uint32_t n = r.u32();
		for (uint32_t i = 0; i < n; ++i)
		{
			mlt::Encoder::Geometry part;
			if (!read_wkb_geometry(r, part, depth + 1) || part.type != mlt::Encoder::GeometryType::POINT)
				continue;
			out.coordinates.insert(out.coordinates.end(), part.coordinates.begin(), part.coordinates.end());
		}
		return !out.coordinates.empty();
	}
	case 5:
	{
		out.type = mlt::Encoder::GeometryType::MULTILINESTRING;
		const uint32_t n = r.u32();
		for (uint32_t i = 0; i < n; ++i)
		{
			mlt::Encoder::Geometry part;
			if (!read_wkb_geometry(r, part, depth + 1) || part.type != mlt::Encoder::GeometryType::LINESTRING)
				continue;
			out.parts.push_back(std::move(part.coordinates));
		}
		return !out.parts.empty();
	}
	case 6:
	{
		out.type = mlt::Encoder::GeometryType::MULTIPOLYGON;
		const uint32_t n = r.u32();
		for (uint32_t i = 0; i < n; ++i)
		{
			mlt::Encoder::Geometry part;
			if (!read_wkb_geometry(r, part, depth + 1) || part.type != mlt::Encoder::GeometryType::POLYGON)
				continue;
			out.parts.push_back(std::move(part.coordinates));
			out.partRingSizes.push_back(std::move(part.ringSizes));
		}
		return !out.parts.empty();
	}
	default:
		return false;
	}
}

Oid
lookup_postgis_type(const char *typname)
{
	Oid nsp = get_namespace_oid("public", true);
	if (!OidIsValid(nsp))
		return InvalidOid;
	return GetSysCacheOid2(TYPENAMENSP, Anum_pg_type_oid, CStringGetDatum(typname), ObjectIdGetDatum(nsp));
}

Oid
lookup_st_asewkb()
{
	static Oid cached = InvalidOid;
	if (OidIsValid(cached))
		return cached;

	Oid geometry_oid = lookup_postgis_type("geometry");
	if (!OidIsValid(geometry_oid))
		throw std::runtime_error("geometry type not found; create extension postgis first");

	Oid argtypes[1] = {geometry_oid};
	List *ewkb_name = list_make1(makeString(pstrdup("st_asewkb")));
	cached = LookupFuncName(ewkb_name, 1, argtypes, true);
	if (!OidIsValid(cached))
	{
		List *asbinary_name = list_make1(makeString(pstrdup("st_asbinary")));
		cached = LookupFuncName(asbinary_name, 1, argtypes, true);
	}
	if (!OidIsValid(cached))
		throw std::runtime_error("could not resolve ST_AsEWKB");
	return cached;
}

// Convert a geometry Datum to MLT geometry using a pre-resolved ST_AsEWKB call.
// The FmgrInfo is cached once per aggregation, avoiding per-feature fmgr_info().
bool
convert_geometry_datum(FmgrInfo *flinfo, Datum geom_datum, mlt::Encoder::Geometry &out)
{
	LOCAL_FCINFO(fcinfo, 1);
	InitFunctionCallInfoData(*fcinfo, flinfo, 1, InvalidOid, nullptr, nullptr);
	fcinfo->args[0].value = geom_datum;
	fcinfo->args[0].isnull = false;
	Datum wkb_datum = FunctionCallInvoke(fcinfo);
	if (fcinfo->isnull)
		return false;

	bytea *wkb = DatumGetByteaPP(wkb_datum);
	WkbReader reader(wkb);
	const bool ok = read_wkb_geometry(reader, out, 0);
	// ST_AsEWKB always returns a freshly palloc'd bytea; free it to bound
	// per-tile memory growth in the aggregate context.
	if (reinterpret_cast<char *>(wkb) != DatumGetPointer(wkb_datum))
		pfree(wkb);
	else
		pfree(DatumGetPointer(wkb_datum));
	return ok;
}

mlt::Encoder::PropertyValue
datum_to_property(Oid typoid, Datum datum)
{
	switch (typoid)
	{
	case BOOLOID:
		return static_cast<bool>(DatumGetBool(datum));
	case INT2OID:
		return static_cast<int32_t>(DatumGetInt16(datum));
	case INT4OID:
		return static_cast<int32_t>(DatumGetInt32(datum));
	case INT8OID:
		return static_cast<int64_t>(DatumGetInt64(datum));
	case FLOAT4OID:
		return static_cast<double>(DatumGetFloat4(datum));
	case FLOAT8OID:
		return static_cast<double>(DatumGetFloat8(datum));
	case TEXTOID:
	case VARCHAROID:
	case BPCHAROID:
	{
		char *c = text_to_cstring(DatumGetTextP(datum));
		std::string s(c);
		pfree(c);
		return s;
	}
	case CSTRINGOID:
		return std::string(DatumGetCString(datum));
	default:
	{
		Oid typoutput = InvalidOid;
		bool typisvarlena = false;
		getTypeOutputInfo(typoid, &typoutput, &typisvarlena);
		char *c = OidOutputFunctionCall(typoutput, datum);
		std::string s(c);
		pfree(c);
		return s;
	}
	}
}

// Resolve the row layout exactly once per aggregation. Copies the TupleDesc
// into the aggregate memory context and caches the geometry/id/property plan
// plus the ST_AsEWKB FmgrInfo, so per-row work stays free of syscache lookups.
void
build_plan(MltAggContext *ctx)
{
	TupleDesc src = lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(ctx->row), HeapTupleHeaderGetTypMod(ctx->row));
	const int natts = src->natts;

	Oid geometry_oid = InvalidOid;
	if (!ctx->geom_name.has_value())
		geometry_oid = lookup_postgis_type("geometry");

	for (int i = 0; i < natts; ++i)
	{
		Form_pg_attribute attr = TupleDescAttr(src, i);
		if (attr->attisdropped)
			continue;

		const char *name = NameStr(attr->attname);
		Oid typoid = getBaseType(attr->atttypid);

		if (ctx->geom_index < 0)
		{
			if (ctx->geom_name.has_value())
			{
				if (*ctx->geom_name == name)
					ctx->geom_index = i;
			}
			else if (OidIsValid(geometry_oid) && typoid == geometry_oid)
			{
				ctx->geom_index = i;
			}
		}

		if (ctx->id_name.has_value() && ctx->id_index < 0 && *ctx->id_name == name)
		{
			if (typoid == INT2OID || typoid == INT4OID || typoid == INT8OID)
			{
				ctx->id_index = i;
				ctx->id_base_type = typoid;
			}
		}
	}

	if (ctx->geom_index < 0)
	{
		ReleaseTupleDesc(src);
		ereport(ERROR, (errmsg("ST_AsMLT: could not find geometry column")));
	}

	for (int i = 0; i < natts; ++i)
	{
		if (i == ctx->geom_index || i == ctx->id_index)
			continue;
		Form_pg_attribute attr = TupleDescAttr(src, i);
		if (attr->attisdropped)
			continue;
		ctx->property_columns.push_back(
			MltPropertyColumn{i, std::string(NameStr(attr->attname)), getBaseType(attr->atttypid)});
	}

	// Keep a standalone copy that survives past this call (agg context is current).
	ctx->tupdesc = CreateTupleDescCopyConstr(src);
	ReleaseTupleDesc(src);

	// Cache the ST_AsEWKB call info in the aggregate context.
	ctx->asewkb_flinfo = static_cast<FmgrInfo *>(palloc0(sizeof(FmgrInfo)));
	fmgr_info(lookup_st_asewkb(), ctx->asewkb_flinfo);

	ctx->plan_ready = true;
}

void
add_feature_from_row(MltAggContext *ctx)
{
	if (!ctx->plan_ready)
		build_plan(ctx);

	TupleDesc tupdesc = ctx->tupdesc;
	HeapTupleData tuple;
	tuple.t_len = HeapTupleHeaderGetDatumLength(ctx->row);
	tuple.t_data = ctx->row;

	bool geom_isnull = false;
	Datum geom_datum = heap_getattr(&tuple, ctx->geom_index + 1, tupdesc, &geom_isnull);
	if (geom_isnull)
		return;

	mlt::Encoder::Feature feature;
	if (!convert_geometry_datum(ctx->asewkb_flinfo, geom_datum, feature.geometry))
		return;

	if (ctx->id_index >= 0)
	{
		bool id_isnull = false;
		Datum id_datum = heap_getattr(&tuple, ctx->id_index + 1, tupdesc, &id_isnull);
		if (!id_isnull)
		{
			int64_t id = 0;
			switch (ctx->id_base_type)
			{
			case INT2OID:
				id = DatumGetInt16(id_datum);
				break;
			case INT4OID:
				id = DatumGetInt32(id_datum);
				break;
			default:
				id = DatumGetInt64(id_datum);
				break;
			}
			if (id >= 0)
				feature.id = static_cast<uint64_t>(id);
		}
	}

	for (const auto &col : ctx->property_columns)
	{
		bool isnull = false;
		Datum datum = heap_getattr(&tuple, col.index + 1, tupdesc, &isnull);
		if (isnull)
			continue;
		feature.properties.emplace(col.name, datum_to_property(col.base_type, datum));
	}

	ctx->features.push_back(std::move(feature));
}

bytea *
encode_layer(MltAggContext *ctx)
{
	if (ctx->features.empty())
	{
		bytea *empty = static_cast<bytea *>(palloc(VARHDRSZ));
		SET_VARSIZE(empty, VARHDRSZ);
		return empty;
	}

	mlt::Encoder::Layer layer;
	layer.name = ctx->name;
	layer.extent = ctx->extent;
	layer.features = std::move(ctx->features);

	mlt::EncoderConfig config;
	config.useFastPfor = false;
	config.includeIds = ctx->id_index >= 0;
	config.sortFeatures = true;
	config.preTessellate = false;
	config.includeOutlines = true;
	config.enableMortonEncoding = true;
	config.useFsst = false;

	try
	{
		mlt::Encoder encoder;
		std::vector<uint8_t> bytes = encoder.encode({layer}, config);
		const size_t len = VARHDRSZ + bytes.size();
		bytea *out = static_cast<bytea *>(palloc(len));
		SET_VARSIZE(out, len);
		if (!bytes.empty())
			memcpy(VARDATA(out), bytes.data(), bytes.size());
		return out;
	}
	catch (const std::exception &ex)
	{
		ereport(ERROR, (errmsg("ST_AsMLT: %s", ex.what())));
	}
	catch (...)
	{
		ereport(ERROR, (errmsg("ST_AsMLT: encoding failed")));
	}

	pg_unreachable();
}

} // namespace

extern "C"
{

PG_FUNCTION_INFO_V1(boundlessgis_asmlt_transfn);
PG_FUNCTION_INFO_V1(boundlessgis_asmlt_finalfn);

Datum
boundlessgis_asmlt_transfn(PG_FUNCTION_ARGS)
{
	MemoryContext aggcontext = nullptr;
	if (!AggCheckCallContext(fcinfo, &aggcontext))
		ereport(ERROR, (errmsg("boundlessgis_asmlt_transfn called in non-aggregate context")));

	MltAggContext *ctx = nullptr;
	if (PG_ARGISNULL(0))
	{
		MemoryContext old = MemoryContextSwitchTo(aggcontext);
		ctx = new MltAggContext();
		if (PG_NARGS() > 2 && !PG_ARGISNULL(2))
			ctx->name = text_to_std_string(PG_GETARG_TEXT_P(2));
		if (ctx->name.empty())
			ereport(ERROR, (errmsg("ST_AsMLT: layer name cannot be empty")));
		ctx->extent = kDefaultExtent;
		if (PG_NARGS() > 3 && !PG_ARGISNULL(3))
		{
			int32_t extent = PG_GETARG_INT32(3);
			if (extent <= 0)
				ereport(ERROR, (errmsg("ST_AsMLT: extent must be greater than 0")));
			ctx->extent = static_cast<uint32_t>(extent);
		}
		if (PG_NARGS() > 4 && !PG_ARGISNULL(4))
			ctx->geom_name = text_to_std_string(PG_GETARG_TEXT_P(4));
		if (PG_NARGS() > 5 && !PG_ARGISNULL(5))
			ctx->id_name = text_to_std_string(PG_GETARG_TEXT_P(5));
		MemoryContextSwitchTo(old);
	}
	else
	{
		ctx = reinterpret_cast<MltAggContext *>(PG_GETARG_POINTER(0));
	}

	if (!type_is_rowtype(get_fn_expr_argtype(fcinfo->flinfo, 1)))
		ereport(ERROR, (errmsg("ST_AsMLT: first argument must be a row type")));

	if (PG_ARGISNULL(1))
		PG_RETURN_POINTER(ctx);

	ctx->row = PG_GETARG_HEAPTUPLEHEADER(1);
	MemoryContext old = MemoryContextSwitchTo(aggcontext);
	try
	{
		add_feature_from_row(ctx);
	}
	catch (const std::exception &ex)
	{
		MemoryContextSwitchTo(old);
		ereport(ERROR, (errmsg("ST_AsMLT: %s", ex.what())));
	}
	MemoryContextSwitchTo(old);
	PG_FREE_IF_COPY(ctx->row, 1);
	PG_RETURN_POINTER(ctx);
}

Datum
boundlessgis_asmlt_finalfn(PG_FUNCTION_ARGS)
{
	if (!AggCheckCallContext(fcinfo, nullptr))
		ereport(ERROR, (errmsg("boundlessgis_asmlt_finalfn called in non-aggregate context")));

	if (PG_ARGISNULL(0))
	{
		bytea *empty = static_cast<bytea *>(palloc(VARHDRSZ));
		SET_VARSIZE(empty, VARHDRSZ);
		PG_RETURN_BYTEA_P(empty);
	}

	auto *ctx = reinterpret_cast<MltAggContext *>(PG_GETARG_POINTER(0));
	bytea *out = encode_layer(ctx);
	delete ctx;
	PG_RETURN_BYTEA_P(out);
}

} // extern "C"
