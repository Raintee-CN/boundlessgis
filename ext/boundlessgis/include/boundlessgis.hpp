#pragma once

#include "postgres.h"
#include "fmgr.h"
#include "access/htup.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <mlt/encoder.hpp>

struct MltPropertyColumn
{
	int index;        // 0-based attribute index
	std::string name; // column name
	Oid base_type;    // getBaseType(atttypid)
};

struct MltAggContext
{
	std::string name{"default"};
	uint32_t extent{4096};
	std::optional<std::string> geom_name;
	std::optional<std::string> id_name;
	int32_t geom_index{-1};
	int32_t id_index{-1};
	HeapTupleHeader row{nullptr};
	std::vector<mlt::Encoder::Feature> features;

	// cached per-aggregation state (computed once on the first input row)
	bool plan_ready{false};
	TupleDesc tupdesc{nullptr};       // standalone copy, lives in agg memory context
	Oid id_base_type{InvalidOid};     // base type of the id column, if any
	std::vector<MltPropertyColumn> property_columns;
	FmgrInfo *asewkb_flinfo{nullptr}; // cached ST_AsEWKB call info, in agg memory context
};
