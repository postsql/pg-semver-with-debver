--
-- Debian version data type (debversion)
--

CREATE TYPE debversion;

--
-- essential IO
--
CREATE OR REPLACE FUNCTION debversion_in(cstring)
	RETURNS debversion
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION debversion_out(debversion)
	RETURNS cstring
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION debversion_recv(internal)
    RETURNS debversion
    AS 'MODULE_PATHNAME'
    LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION debversion_send(debversion)
    RETURNS bytea
    AS 'MODULE_PATHNAME'
    LANGUAGE C STRICT IMMUTABLE;

--
--  The type itself.
--

CREATE TYPE debversion (
	INPUT = debversion_in,
	OUTPUT = debversion_out,
	RECEIVE = debversion_recv,
	SEND = debversion_send,
    STORAGE = plain,
	INTERNALLENGTH = variable,
	CATEGORY = 'S',
	PREFERRED = false
);

--
--  A lax constructor function and validator function.
--

CREATE OR REPLACE FUNCTION to_debversion(text)
	RETURNS debversion
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION is_debversion(text)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

--
--  Typecasting functions.
--

CREATE OR REPLACE FUNCTION debversion(text)
	RETURNS debversion
    AS 'MODULE_PATHNAME', 'text_to_debversion'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION text(debversion)
	RETURNS text
    AS 'MODULE_PATHNAME', 'debversion_to_text'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION debversion(semver)
	RETURNS debversion
    AS 'MODULE_PATHNAME', 'semver_to_debversion'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION semver(debversion)
	RETURNS semver
    AS 'MODULE_PATHNAME', 'debversion_to_semver'
	LANGUAGE C STRICT IMMUTABLE;

--
--  Explicit type casts.
--

CREATE CAST (debversion AS text)     WITH FUNCTION text(debversion);
CREATE CAST (text AS debversion)     WITH FUNCTION debversion(text);
CREATE CAST (semver AS debversion)   WITH FUNCTION debversion(semver);
CREATE CAST (debversion AS semver)   WITH FUNCTION semver(debversion);

--
--	Comparison functions and their corresponding operators.
--

CREATE OR REPLACE FUNCTION debversion_eq(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR = (
	leftarg = debversion,
	rightarg = debversion,
	negator = <>,
	procedure = debversion_eq,
	restrict = eqsel,
	commutator = =,
	join = eqjoinsel,
	hashes, merges
);

CREATE OR REPLACE FUNCTION debversion_ne(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR <> (
	leftarg = debversion,
	rightarg = debversion,
	negator = =,
	procedure = debversion_ne,
	restrict = neqsel,
	join = neqjoinsel
);

CREATE OR REPLACE FUNCTION debversion_le(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR <= (
	leftarg = debversion,
	rightarg = debversion,
	negator = >,
	procedure = debversion_le
);

CREATE OR REPLACE FUNCTION debversion_lt(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR < (
	leftarg = debversion,
	rightarg = debversion,
	negator = >=,
	procedure = debversion_lt
);

CREATE OR REPLACE FUNCTION debversion_ge(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR >= (
	leftarg = debversion,
	rightarg = debversion,
	negator = <,
	procedure = debversion_ge
);

CREATE OR REPLACE FUNCTION debversion_gt(debversion, debversion)
	RETURNS bool
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR > (
	leftarg = debversion,
	rightarg = debversion,
	negator = <=,
	procedure = debversion_gt
);

--
-- Support functions for indexing.
--

CREATE OR REPLACE FUNCTION debversion_cmp(debversion, debversion)
	RETURNS int4
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION hash_debversion(debversion)
	RETURNS int4
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION hash_debversion_extended(debversion, int8)
	RETURNS int8
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

--
-- The btree indexing operator class.
--

CREATE OPERATOR CLASS debversion_ops
DEFAULT FOR TYPE debversion USING btree AS
    OPERATOR    1   <  (debversion, debversion),
    OPERATOR    2   <= (debversion, debversion),
    OPERATOR    3   =  (debversion, debversion),
    OPERATOR    4   >= (debversion, debversion),
    OPERATOR    5   >  (debversion, debversion),
    FUNCTION    1   debversion_cmp(debversion, debversion);

--
-- The hash indexing operator class.
--

CREATE OPERATOR CLASS debversion_ops
DEFAULT FOR TYPE debversion USING hash AS
    OPERATOR    1   =  (debversion, debversion),
    FUNCTION    1   hash_debversion(debversion),
    FUNCTION    2   hash_debversion_extended(debversion, int8);

--
-- Aggregates.
--

CREATE OR REPLACE FUNCTION debversion_smaller(debversion, debversion)
	RETURNS debversion
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE AGGREGATE min(debversion)  (
    SFUNC = debversion_smaller,
    STYPE = debversion,
    SORTOP = <
);

CREATE OR REPLACE FUNCTION debversion_larger(debversion, debversion)
	RETURNS debversion
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE AGGREGATE max(debversion)  (
    SFUNC = debversion_larger,
    STYPE = debversion,
    SORTOP = >
);

--
-- Accessor functions
--

CREATE OR REPLACE FUNCTION get_debversion_epoch(debversion)
	RETURNS int4
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION get_debversion_upstream(debversion)
	RETURNS text
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION get_debversion_revision(debversion)
	RETURNS text
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

--
-- Range type
--

CREATE TYPE debversionrange AS RANGE (SUBTYPE = debversion);
