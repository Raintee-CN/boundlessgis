-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION boundlessgis" to load this file. \quit

CREATE FUNCTION boundlessgis_asmlt_transfn(internal, anyelement)
RETURNS internal
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_transfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE FUNCTION boundlessgis_asmlt_transfn(internal, anyelement, text)
RETURNS internal
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_transfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE FUNCTION boundlessgis_asmlt_transfn(internal, anyelement, text, integer)
RETURNS internal
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_transfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE FUNCTION boundlessgis_asmlt_transfn(internal, anyelement, text, integer, text)
RETURNS internal
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_transfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE FUNCTION boundlessgis_asmlt_transfn(internal, anyelement, text, integer, text, text)
RETURNS internal
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_transfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE FUNCTION boundlessgis_asmlt_finalfn(internal)
RETURNS bytea
AS 'MODULE_PATHNAME', 'boundlessgis_asmlt_finalfn'
LANGUAGE C IMMUTABLE PARALLEL UNSAFE;

CREATE AGGREGATE ST_AsMLT(anyelement)
(
	sfunc = boundlessgis_asmlt_transfn,
	stype = internal,
	finalfunc = boundlessgis_asmlt_finalfn,
	parallel = unsafe
);

CREATE AGGREGATE ST_AsMLT(anyelement, text)
(
	sfunc = boundlessgis_asmlt_transfn,
	stype = internal,
	finalfunc = boundlessgis_asmlt_finalfn,
	parallel = unsafe
);

CREATE AGGREGATE ST_AsMLT(anyelement, text, integer)
(
	sfunc = boundlessgis_asmlt_transfn,
	stype = internal,
	finalfunc = boundlessgis_asmlt_finalfn,
	parallel = unsafe
);

CREATE AGGREGATE ST_AsMLT(anyelement, text, integer, text)
(
	sfunc = boundlessgis_asmlt_transfn,
	stype = internal,
	finalfunc = boundlessgis_asmlt_finalfn,
	parallel = unsafe
);

CREATE AGGREGATE ST_AsMLT(anyelement, text, integer, text, text)
(
	sfunc = boundlessgis_asmlt_transfn,
	stype = internal,
	finalfunc = boundlessgis_asmlt_finalfn,
	parallel = unsafe
);
