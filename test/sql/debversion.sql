\set ECHO none
BEGIN;

\i test/pgtap-core.sql
CREATE EXTENSION semver;

SELECT plan(120);

-- 1. Type existence & NULLability
SELECT has_type('debversion');
SELECT is( NULL::debversion, NULL, 'debversion should be NULLable' );

-- 2. Valid debversions (lives_ok)
SELECT lives_ok(
    $$ SELECT '$$ || v || $$'::debversion $$,
    '"' || v || '" is a valid debversion'
) FROM unnest(ARRAY[
    '1.0',
    '1.2.3',
    '0.1.0',
    '0',
    '0.0.0',
    '0:1.0',
    '1:2.3.4',
    '10:0.1',
    '00:1.0',
    '01:2.0',
    '1.0-1',
    '1.2.3-0ubuntu1',
    '1.0-1+deb11u1',
    '1.0-test-1',
    '1.0-a-b-c-1',
    '1:1.0:beta-1',
    '1.0~rc1-1',
    '1.0~alpha-1',
    '1.0-0',
    '1.0-~1',
    '20230101',
    '1.0.0+dfsg-1'
]) AS v;

-- 3. Invalid debversions in strict mode (throws_ok)
SELECT throws_ok(
    $$ SELECT '$$ || v || $$'::debversion $$,
    NULL,
    '"' || v || '" is not a valid debversion'
) FROM unnest(ARRAY[
    '',
    '   ',
    ' 1.0',
    '1.0 ',
    ' 1.0-1 ',
    'v1.0',
    ':1.0',
    'a:1.0',
    '1a:1.0',
    '-1:1.0',
    '1.0-',
    '1.0--',
    '1.0:1',
    '1.0_1',
    '1.0@1',
    '1.0-1$2',
    '1.0#1',
    '1.0-1:2'
]) AS v;

-- 4. Epoch normalization and equality
SELECT is( '0:1.0'::debversion = '1.0'::debversion, true, '0:1.0 = 1.0' );
SELECT is( '00:1.0'::debversion = '1.0'::debversion, true, '00:1.0 = 1.0' );
SELECT is( '00:1.0-1'::debversion = '1.0-01'::debversion, true, '00:1.0-1 = 1.0-01' );
SELECT is( '1.0-1'::debversion = '0:1.0-01'::debversion, true, '1.0-1 = 0:1.0-01' );
SELECT is( '1.0'::debversion = '1.00'::debversion, true, '1.0 = 1.00' );
SELECT is( '1:1.0'::debversion <> '0:1.0'::debversion, true, '1:1.0 <> 0:1.0' );

-- 5. Ordering operators (<, <=, >, >=)
SELECT is( '1:1.0'::debversion > '0:1.0'::debversion, true, '1:1.0 > 0:1.0' );
SELECT is( '2:1.0'::debversion > '1:2.0'::debversion, true, '2:1.0 > 1:2.0' );
SELECT is( '1.0'::debversion < '1.1'::debversion, true, '1.0 < 1.1' );
SELECT is( '1.1'::debversion < '1.2'::debversion, true, '1.1 < 1.2' );
SELECT is( '1.2'::debversion < '1.10'::debversion, true, '1.2 < 1.10' );
SELECT is( '1.0~rc1'::debversion < '1.0'::debversion, true, '1.0~rc1 < 1.0' );
SELECT is( '1.0~rc1'::debversion < '1.0~rc2'::debversion, true, '1.0~rc1 < 1.0~rc2' );
SELECT is( '1.0'::debversion < '1.0+git1'::debversion, true, '1.0 < 1.0+git1' );
SELECT is( '1.0'::debversion < '1.0.1'::debversion, true, '1.0 < 1.0.1' );
SELECT is( '1.0'::debversion < '1.0a'::debversion, true, '1.0 < 1.0a' );
SELECT is( '1.0'::debversion < '1.0-1'::debversion, true, '1.0 < 1.0-1' );
SELECT is( '1.0-~1'::debversion < '1.0'::debversion, true, '1.0-~1 < 1.0' );
SELECT is( '1.0-1'::debversion < '1.0-2'::debversion, true, '1.0-1 < 1.0-2' );
SELECT is( '1.0-1'::debversion < '1.0-10'::debversion, true, '1.0-1 < 1.0-10' );
SELECT is( '1.0-1~bpo'::debversion < '1.0-1'::debversion, true, '1.0-1~bpo < 1.0-1' );

SELECT is( '1.0-1'::debversion <= '1.0-1'::debversion, true, '1.0-1 <= 1.0-1' );
SELECT is( '1.0-1'::debversion >= '1.0-1'::debversion, true, '1.0-1 >= 1.0-1' );
SELECT is( '1.0'::debversion < '1.0-0'::debversion, true, '1.0 < 1.0-0' );
SELECT is( '1.0-0'::debversion = '1.0-00'::debversion, true, '1.0-0 = 1.0-00' );
SELECT is( '1.0-0'::debversion < '1.0-1'::debversion, true, '1.0-0 < 1.0-1' );
SELECT is( '1.0-~'::debversion < '1.0'::debversion, true, '1.0-~ < 1.0' );
SELECT is( '1.0-~'::debversion < '1.0-0'::debversion, true, '1.0-~ < 1.0-0' );
SELECT is( debversion_cmp('1.0-1'::debversion, '1.0-2'::debversion), -1, 'debversion_cmp <' );
SELECT is( debversion_cmp('1.0-1'::debversion, '1.0-1'::debversion), 0, 'debversion_cmp =' );
SELECT is( debversion_cmp('1.0-2'::debversion, '1.0-1'::debversion), 1, 'debversion_cmp >' );

-- 6. Hash normalization invariant
SELECT is(
    hash_debversion('0:1.0-1'::debversion),
    hash_debversion('1.0-1'::debversion),
    'hash(0:1.0-1) = hash(1.0-1)'
);
SELECT is(
    hash_debversion('00:1.0-1'::debversion),
    hash_debversion('1.0-1'::debversion),
    'hash(00:1.0-1) = hash(1.0-1)'
);
SELECT is(
    hash_debversion('0:1.0-01'::debversion),
    hash_debversion('1.0-1'::debversion),
    'hash(0:1.0-01) = hash(1.0-1)'
);
SELECT is(
    hash_debversion('1.0'::debversion),
    hash_debversion('0:1.0'::debversion),
    'hash(1.0) = hash(0:1.0)'
);
SELECT is(
    hash_debversion_extended('0:1.0-1'::debversion, 42),
    hash_debversion_extended('1.0-1'::debversion, 42),
    'hash_extended(0:1.0-1) = hash_extended(1.0-1)'
);
SELECT is(
    hash_debversion_extended('00:1.0-1'::debversion, 42),
    hash_debversion_extended('1.0-1'::debversion, 42),
    'hash_extended(00:1.0-1) = hash_extended(1.0-1)'
);
SELECT is(
    hash_debversion_extended('1.0'::debversion, 42),
    hash_debversion_extended('0:1.0'::debversion, 42),
    'hash_extended(1.0) = hash_extended(0:1.0)'
);

-- 7. Lax constructor & validator
SELECT is( to_debversion('  1.0  '), '1.0'::debversion, 'to_debversion trims whitespace' );
SELECT is( (to_debversion('v1.0'))::text, 'v1.0', 'to_debversion allows leading v' );
SELECT is( (to_debversion('  v1.0-1  '))::text, 'v1.0-1', 'to_debversion trims and allows leading v' );
SELECT is( to_debversion('v1.0') < to_debversion('v2.0'), true, 'to_debversion comparison' );
SELECT is( is_debversion('1.0'), true, 'is_debversion valid' );
SELECT is( is_debversion('v1.0'), false, 'is_debversion rejects leading v in strict mode' );
SELECT is( is_debversion('invalid@'), false, 'is_debversion rejects illegal chars' );
SELECT is( is_debversion(''), false, 'is_debversion rejects empty string' );

-- 8. Accessors
SELECT is( get_debversion_epoch('1.0-1'::debversion), 0, 'epoch defaults to 0' );
SELECT is( get_debversion_epoch('2:1.0-1'::debversion), 2, 'epoch returns 2' );
SELECT is( get_debversion_epoch('003:1.0'::debversion), 3, 'epoch returns 3 for 003' );
SELECT is( get_debversion_upstream('1.0-1'::debversion), '1.0', 'upstream returns 1.0' );
SELECT is( get_debversion_upstream('2:1.5~rc1-ubuntu1'::debversion), '1.5~rc1', 'upstream with epoch/tilde' );
SELECT is( get_debversion_revision('1.0-1'::debversion), '1', 'revision returns 1' );
SELECT is( get_debversion_revision('1.0-ubuntu1'::debversion), 'ubuntu1', 'revision returns ubuntu1' );
SELECT is( get_debversion_revision('1.0'::debversion), NULL, 'native package revision is NULL' );
SELECT is( get_debversion_epoch('00000000000000000000000000000000000:1.0'::debversion), 0, 'epoch with 35 zeros returns 0' );
SELECT is( get_debversion_epoch('000000000000000000000000000000000005:1.0'::debversion), 5, 'epoch with 35 zeros and 5 returns 5' );
SELECT throws_ok(
    $$ SELECT get_debversion_epoch('99999999999999999999999999999999999:1.0'::debversion) $$,
    NULL,
    'epoch exceeding 31-bit range throws error'
);

-- 9. Aggregates min/max
CREATE TABLE deb_agg_test (v debversion);
INSERT INTO deb_agg_test VALUES ('1.0-1'), ('1.2-1'), ('2.0-1'), ('1.0~rc1');
SELECT is( min(v), '1.0~rc1'::debversion, 'min aggregate' ) FROM deb_agg_test;
SELECT is( max(v), '2.0-1'::debversion, 'max aggregate' ) FROM deb_agg_test;

-- 10. Opclasses & Indexes (B-tree and Hash)
CREATE TABLE deb_idx_test (id int, v debversion);
CREATE INDEX idx_deb_btree ON deb_idx_test USING btree (v);
CREATE INDEX idx_deb_hash ON deb_idx_test USING hash (v);
INSERT INTO deb_idx_test VALUES (1, '1.0-1'), (2, '0:2.0'), (3, '1.5');

SELECT is( id, 1, 'hash index lookup with epoch normalization' )
FROM deb_idx_test WHERE v = '0:1.0-1'::debversion;

SELECT is(
    ARRAY(SELECT v FROM deb_idx_test ORDER BY v),
    ARRAY['1.0-1'::debversion, '1.5'::debversion, '0:2.0'::debversion],
    'btree index sort order'
);

CREATE TABLE deb_part (id int, v debversion) PARTITION BY HASH (v);
CREATE TABLE deb_part_0 PARTITION OF deb_part FOR VALUES WITH (MODULUS 2, REMAINDER 0);
CREATE TABLE deb_part_1 PARTITION OF deb_part FOR VALUES WITH (MODULUS 2, REMAINDER 1);
INSERT INTO deb_part VALUES (1, '1.0-1'), (2, '0:1.0-1'), (3, '1.0-01'), (4, '2.0');
SELECT is(
    (SELECT count(DISTINCT tableoid) FROM deb_part WHERE id IN (1, 2, 3)),
    1::bigint,
    'hash partitioning routes equivalent versions to same partition'
);
DROP TABLE deb_part;

-- 11. Range type (debversionrange)
SELECT lives_ok(
    $$ SELECT '[1.0-1,2.0-1]'::debversionrange $$,
    'debversionrange created'
);
SELECT is(
    '[1.0-1,2.0-1]'::debversionrange @> '1.5'::debversion,
    true,
    'range contains 1.5'
);
SELECT is(
    '[1.0-1,2.0-1]'::debversionrange @> '2.5'::debversion,
    false,
    'range does not contain 2.5'
);
SELECT is(
    debversionrange('1.0-1'::debversion, '2.0-1'::debversion) @> '1.5'::debversion,
    true,
    'constructor range contains 1.5'
);

-- 12. Typecasts
SELECT is( ('1.2.3'::semver)::debversion, '1.2.3'::debversion, 'semver to debversion cast' );
SELECT is( ('1.2.3'::debversion)::semver, '1.2.3'::semver, 'debversion to semver cast' );
SELECT throws_ok(
    $$ SELECT ('1:1.0'::debversion)::semver $$,
    NULL,
    'cannot cast epoch debversion to semver'
);
SELECT throws_ok(
    $$ SELECT ('1.0~rc1'::debversion)::semver $$,
    NULL,
    'cannot cast tilde debversion to semver'
);
SELECT is( ('1.0-1'::debversion)::text, '1.0-1', 'debversion to text cast' );
SELECT is( ('1.0-1'::text)::debversion, '1.0-1'::debversion, 'text to debversion cast' );

-- 13. Binary Send and Receive
CREATE TABLE deb_bin_src (v debversion);
INSERT INTO deb_bin_src VALUES ('1.0-1'), ('2:3.4.5~rc1-0ubuntu1'), (NULL);
SELECT function_returns('debversion_send', 'bytea');
SELECT has_function('debversion_recv', ARRAY['internal']);
SELECT function_returns('debversion_recv', 'debversion');

CREATE TABLE deb_bin_dst (v debversion);
\copy deb_bin_src TO debversion_binary_copy.bin WITH BINARY;
\copy deb_bin_dst from debversion_binary_copy.bin with binary;
SELECT bag_eq(
    'SELECT * FROM deb_bin_src',
    'SELECT * FROM deb_bin_dst',
    'binary copy preserves debversion values'
);

-- 14. Negative and Security Testing with unprivileged role (Postgres rule 28)
CREATE ROLE regress_deb_unpriv;
SET ROLE regress_deb_unpriv;

CREATE TEMP TABLE deb_unpriv_test (v debversion);
INSERT INTO deb_unpriv_test VALUES ('1.0-1'), ('0:1.0-1');
SELECT is( COUNT(*), 2::bigint, 'unprivileged user can use debversion' ) FROM deb_unpriv_test;
SELECT is( ('1.0-1'::debversion = '0:1.0-1'::debversion), true, 'unprivileged user can compare debversions' );
DROP TABLE deb_unpriv_test;

RESET ROLE;
DROP ROLE regress_deb_unpriv;

SELECT * FROM finish();
ROLLBACK;
