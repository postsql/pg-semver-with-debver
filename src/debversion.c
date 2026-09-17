//  -*- tab-width:4; c-basic-offset:4; indent-tabs-mode:nil;  -*-
/*
 * PostgreSQL type definitions for debversion type
 *
 * Copyright 2026 The pg-semver Maintainers. This program is Free
 * Software; see the LICENSE file for the license conditions.
 */

#include "postgres.h"
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include "utils/builtins.h"
#include "catalog/pg_collation.h"
#include "access/hash.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

#include "debver_evr.h"
#include "semver.h"

/* IO methods */
Datum debversion_in(PG_FUNCTION_ARGS);
Datum debversion_out(PG_FUNCTION_ARGS);
Datum debversion_recv(PG_FUNCTION_ARGS);
Datum debversion_send(PG_FUNCTION_ARGS);

/* Comparison functions */
Datum debversion_eq(PG_FUNCTION_ARGS);
Datum debversion_ne(PG_FUNCTION_ARGS);
Datum debversion_lt(PG_FUNCTION_ARGS);
Datum debversion_le(PG_FUNCTION_ARGS);
Datum debversion_ge(PG_FUNCTION_ARGS);
Datum debversion_gt(PG_FUNCTION_ARGS);
Datum debversion_cmp(PG_FUNCTION_ARGS);

/* Hash functions */
Datum hash_debversion(PG_FUNCTION_ARGS);
Datum hash_debversion_extended(PG_FUNCTION_ARGS);

/* Aggregates */
Datum debversion_smaller(PG_FUNCTION_ARGS);
Datum debversion_larger(PG_FUNCTION_ARGS);

/* Constructor and Validator */
Datum to_debversion(PG_FUNCTION_ARGS);
Datum is_debversion(PG_FUNCTION_ARGS);

/* Typecasts */
Datum text_to_debversion(PG_FUNCTION_ARGS);
Datum debversion_to_text(PG_FUNCTION_ARGS);
Datum semver_to_debversion(PG_FUNCTION_ARGS);
Datum debversion_to_semver(PG_FUNCTION_ARGS);

/* Accessors */
Datum get_debversion_epoch(PG_FUNCTION_ARGS);
Datum get_debversion_upstream(PG_FUNCTION_ARGS);
Datum get_debversion_revision(PG_FUNCTION_ARGS);

static bool
debversion_validate(const char *str, bool lax, bool throw_error, char **trimmed_out)
{
    const char *p = str;
    const char *colon;
    const char *version_start;
    const char *last_hyphen;
    const char *upstream;
    size_t upstream_len;
    const char *revision = NULL;
    size_t revision_len = 0;
    bool has_epoch = false;
    size_t len;
    size_t i;

    if (lax)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
    }

    len = strlen(p);
    if (lax)
    {
        while (len > 0 && isspace((unsigned char)p[len - 1]))
            len--;
    }

    if (len == 0)
    {
        if (throw_error)
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                     errmsg("invalid input syntax for type %s: \"%s\"",
                            "debversion", str)));
        return false;
    }

    if (!lax)
    {
        if (isspace((unsigned char)str[0]) || isspace((unsigned char)str[strlen(str) - 1]))
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Leading or trailing whitespace is not allowed.")));
            return false;
        }
    }

    /* Check for colon ':' (epoch delimiter) */
    colon = memchr(p, ':', len);
    if (colon != NULL)
    {
        size_t epoch_len = (size_t)(colon - p);
        if (epoch_len == 0)
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Epoch cannot be empty.")));
            return false;
        }
        for (i = 0; i < epoch_len; i++)
        {
            if (!isdigit((unsigned char)p[i]))
            {
                if (throw_error)
                    ereport(ERROR,
                            (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                             errmsg("invalid input syntax for type %s: \"%s\"",
                                    "debversion", str),
                             errdetail("Epoch must consist only of digits.")));
                return false;
            }
        }
        has_epoch = true;
        version_start = colon + 1;
        len = len - (epoch_len + 1);
    }
    else
    {
        has_epoch = false;
        version_start = p;
    }

    if (len == 0)
    {
        if (throw_error)
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                     errmsg("invalid input syntax for type %s: \"%s\"",
                            "debversion", str),
                     errdetail("Upstream version cannot be empty.")));
        return false;
    }

    /* Find the last hyphen in version_start */
    last_hyphen = NULL;
    for (i = 0; i < len; i++)
    {
        if (version_start[i] == '-')
            last_hyphen = &version_start[i];
    }

    if (last_hyphen != NULL)
    {
        upstream = version_start;
        upstream_len = (size_t)(last_hyphen - version_start);
        revision = last_hyphen + 1;
        revision_len = len - (upstream_len + 1);

        if (revision_len == 0)
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Debian revision cannot be empty.")));
            return false;
        }

        /* Debian revision characters: alphanumerics, plus, dot, tilde */
        for (i = 0; i < revision_len; i++)
        {
            unsigned char c = (unsigned char)revision[i];
            if (!(isalnum(c) || c == '+' || c == '.' || c == '~'))
            {
                if (throw_error)
                    ereport(ERROR,
                            (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                             errmsg("invalid input syntax for type %s: \"%s\"",
                                    "debversion", str),
                             errdetail("Debian revision contains invalid character '%c'.", c)));
                return false;
            }
        }
    }
    else
    {
        upstream = version_start;
        upstream_len = len;
        revision = NULL;
        revision_len = 0;
    }

    if (upstream_len == 0)
    {
        if (throw_error)
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                     errmsg("invalid input syntax for type %s: \"%s\"",
                            "debversion", str),
                     errdetail("Upstream version cannot be empty.")));
        return false;
    }

    /* Upstream version must start with a digit in strict mode */
    if (!lax)
    {
        if (!isdigit((unsigned char)upstream[0]))
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Upstream version must start with a digit.")));
            return false;
        }
    }

    /* Check upstream version characters: alphanumerics, plus, dot, tilde, hyphen, colon */
    for (i = 0; i < upstream_len; i++)
    {
        unsigned char c = (unsigned char)upstream[i];
        if (!(isalnum(c) || c == '.' || c == '+' || c == '~' || c == '-' || c == ':'))
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Upstream version contains invalid character '%c'.", c)));
            return false;
        }
        if (!has_epoch && c == ':')
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Upstream version cannot contain colons when epoch is omitted.")));
            return false;
        }
        if (revision == NULL && c == '-')
        {
            if (throw_error)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                         errmsg("invalid input syntax for type %s: \"%s\"",
                                "debversion", str),
                         errdetail("Upstream version cannot contain hyphens when revision is omitted.")));
            return false;
        }
    }

    if (trimmed_out != NULL)
    {
        size_t total_len = (size_t)((revision ? (revision + revision_len) : (upstream + upstream_len)) - p);
        char *t = palloc(total_len + 1);
        memcpy(t, p, total_len);
        t[total_len] = '\0';
        *trimmed_out = t;
    }

    return true;
}

static void
debversion_normalize_for_hash(const char *str, StringInfo buf)
{
    const char *e, *u, *r;
    size_t el, ul, rl;
    size_t i;

    debver_split_evr(str, &e, &el, &u, &ul, &r, &rl);

    /* Normalize epoch: if absent or all zeros -> "0" */
    if (e != NULL)
    {
        while (el > 1 && *e == '0' && isdigit((unsigned char)e[1]))
        {
            e++;
            el--;
        }
        appendBinaryStringInfo(buf, e, el);
    }
    else
    {
        appendStringInfoChar(buf, '0');
    }
    appendStringInfoChar(buf, ':');

    /* Normalize upstream version: strip leading zeros from digit sequences */
    i = 0;
    while (i < ul)
    {
        if (isdigit((unsigned char)u[i]))
        {
            size_t start = i;
            while (i < ul && isdigit((unsigned char)u[i]))
                i++;
            while ((i - start) > 1 && u[start] == '0' && isdigit((unsigned char)u[start + 1]))
                start++;
            appendBinaryStringInfo(buf, &u[start], i - start);
        }
        else
        {
            appendStringInfoChar(buf, u[i]);
            i++;
        }
    }

    /* Normalize debian revision */
    if (r != NULL)
    {
        appendStringInfoChar(buf, '-');
        i = 0;
        while (i < rl)
        {
            if (isdigit((unsigned char)r[i]))
            {
                size_t start = i;
                while (i < rl && isdigit((unsigned char)r[i]))
                    i++;
                while ((i - start) > 1 && r[start] == '0' && isdigit((unsigned char)r[start + 1]))
                    start++;
                appendBinaryStringInfo(buf, &r[start], i - start);
            }
            else
            {
                appendStringInfoChar(buf, r[i]);
                i++;
            }
        }
    }
}

/*
 * PostgreSQL function bindings
 */

PG_FUNCTION_INFO_V1(debversion_in);
Datum
debversion_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    text *result;

    debversion_validate(str, false, true, NULL);
    result = cstring_to_text(str);
    PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(debversion_out);
Datum
debversion_out(PG_FUNCTION_ARGS)
{
    text *v = PG_GETARG_TEXT_PP(0);
    char *result = text_to_cstring(v);
    PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(debversion_send);
Datum
debversion_send(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    StringInfoData buf;
    char version = 1;

    pq_begintypsend(&buf);
    pq_sendbyte(&buf, version);
    pq_sendtext(&buf, VARDATA_ANY(v), VARSIZE_ANY_EXHDR(v));

    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

PG_FUNCTION_INFO_V1(debversion_recv);
Datum
debversion_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    char version = pq_getmsgbyte(buf);
    char *str;
    int nbytes;
    text *result;

    if (version != 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("unsupported debversion binary version number %d", version)));
    }

    str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
    debversion_validate(str, false, true, NULL);
    result = cstring_to_text_with_len(str, nbytes);
    pfree(str);

    PG_RETURN_TEXT_P(result);
}

static int
_debversion_cmp(struct varlena *v1, struct varlena *v2)
{
    int len1;
    int len2;
    char *s1;
    char *s2;
    int diff;

    if (v1 == v2)
        return 0;

    len1 = VARSIZE_ANY_EXHDR(v1);
    len2 = VARSIZE_ANY_EXHDR(v2);
    if (len1 == len2 && memcmp(VARDATA_ANY(v1), VARDATA_ANY(v2), len1) == 0)
        return 0;

    s1 = text_to_cstring((text *) v1);
    s2 = text_to_cstring((text *) v2);
    diff = debver_evrcmp(s1, s2);
    pfree(s1);
    pfree(s2);
    return diff;
}

PG_FUNCTION_INFO_V1(debversion_eq);
Datum
debversion_eq(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) == 0);
}

PG_FUNCTION_INFO_V1(debversion_ne);
Datum
debversion_ne(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) != 0);
}

PG_FUNCTION_INFO_V1(debversion_lt);
Datum
debversion_lt(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) < 0);
}

PG_FUNCTION_INFO_V1(debversion_le);
Datum
debversion_le(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(debversion_ge);
Datum
debversion_ge(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(debversion_gt);
Datum
debversion_gt(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_BOOL(_debversion_cmp(a, b) > 0);
}

PG_FUNCTION_INFO_V1(debversion_cmp);
Datum
debversion_cmp(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    PG_RETURN_INT32(_debversion_cmp(a, b));
}

PG_FUNCTION_INFO_V1(hash_debversion);
Datum
hash_debversion(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    char *str = text_to_cstring((text *) v);
    StringInfoData buf;
    uint32 hash;

    initStringInfo(&buf);
    debversion_normalize_for_hash(str, &buf);
    hash = DatumGetUInt32(hash_any((const unsigned char *) buf.data, buf.len));
    pfree(buf.data);
    pfree(str);

    PG_RETURN_INT32(hash);
}

PG_FUNCTION_INFO_V1(hash_debversion_extended);
Datum
hash_debversion_extended(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    uint64 seed = PG_GETARG_INT64(1);
    char *str = text_to_cstring((text *) v);
    StringInfoData buf;
    uint64 hash;

    initStringInfo(&buf);
    debversion_normalize_for_hash(str, &buf);
    hash = DatumGetUInt64(hash_any_extended((const unsigned char *) buf.data, buf.len, seed));
    pfree(buf.data);
    pfree(str);

    PG_RETURN_INT64(hash);
}

PG_FUNCTION_INFO_V1(debversion_smaller);
Datum
debversion_smaller(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    int diff = _debversion_cmp(a, b);
    if (diff <= 0)
        PG_RETURN_DATUM(PG_GETARG_DATUM(0));
    PG_RETURN_DATUM(PG_GETARG_DATUM(1));
}

PG_FUNCTION_INFO_V1(debversion_larger);
Datum
debversion_larger(PG_FUNCTION_ARGS)
{
    struct varlena *a = PG_GETARG_VARLENA_PP(0);
    struct varlena *b = PG_GETARG_VARLENA_PP(1);
    int diff = _debversion_cmp(a, b);
    if (diff >= 0)
        PG_RETURN_DATUM(PG_GETARG_DATUM(0));
    PG_RETURN_DATUM(PG_GETARG_DATUM(1));
}

PG_FUNCTION_INFO_V1(to_debversion);
Datum
to_debversion(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int len = VARSIZE_ANY_EXHDR(txt);
    char *str;
    char *trimmed = NULL;
    text *res;

    if (memchr(VARDATA_ANY(txt), '\0', len) != NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid input syntax for type %s: contains null character",
                        "debversion")));

    str = text_to_cstring(txt);
    debversion_validate(str, true, true, &trimmed);
    res = cstring_to_text(trimmed);
    pfree(trimmed);
    pfree(str);

    PG_RETURN_TEXT_P(res);
}

PG_FUNCTION_INFO_V1(is_debversion);
Datum
is_debversion(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int len = VARSIZE_ANY_EXHDR(txt);
    char *str;
    bool ok;

    if (memchr(VARDATA_ANY(txt), '\0', len) != NULL)
        PG_RETURN_BOOL(false);

    str = text_to_cstring(txt);
    ok = debversion_validate(str, false, false, NULL);
    pfree(str);
    PG_RETURN_BOOL(ok);
}

PG_FUNCTION_INFO_V1(text_to_debversion);
Datum
text_to_debversion(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int len = VARSIZE_ANY_EXHDR(txt);
    char *str;

    if (memchr(VARDATA_ANY(txt), '\0', len) != NULL)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid input syntax for type %s: contains null character",
                        "debversion")));

    str = text_to_cstring(txt);
    debversion_validate(str, false, true, NULL);
    pfree(str);
    PG_RETURN_POINTER(txt);
}

PG_FUNCTION_INFO_V1(debversion_to_text);
Datum
debversion_to_text(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    PG_RETURN_TEXT_P(txt);
}

PG_FUNCTION_INFO_V1(semver_to_debversion);
Datum
semver_to_debversion(PG_FUNCTION_ARGS)
{
    semver *sv = PG_GETARG_SEMVER_P(0);
    char *str = emit_semver(sv);
    text *res;

    debversion_validate(str, false, true, NULL);
    res = cstring_to_text(str);
    pfree(str);

    PG_RETURN_TEXT_P(res);
}

PG_FUNCTION_INFO_V1(debversion_to_semver);
Datum
debversion_to_semver(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    char *str = text_to_cstring((text *) v);
    bool bad = false;
    semver *sv = parse_semver(str, false, true, &bad);
    pfree(str);
    if (!sv)
        PG_RETURN_NULL();
    PG_RETURN_POINTER(sv);
}

PG_FUNCTION_INFO_V1(get_debversion_epoch);
Datum
get_debversion_epoch(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    char *str = text_to_cstring((text *) v);
    const char *epoch;
    size_t epoch_len;
    const char *u;
    size_t ul;
    const char *r;
    size_t rl;
    int32 val = 0;

    debver_split_evr(str, &epoch, &epoch_len, &u, &ul, &r, &rl);
    if (epoch != NULL)
    {
        char *endptr;
        long num;
        char tmp[32];

        while (epoch_len > 1 && *epoch == '0')
        {
            epoch++;
            epoch_len--;
        }

        if (epoch_len >= sizeof(tmp))
        {
            pfree(str);
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("debversion epoch exceeds 31-bit range")));
        }
        memcpy(tmp, epoch, epoch_len);
        tmp[epoch_len] = '\0';
        errno = 0;
        num = strtol(tmp, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || num > INT32_MAX || num < 0)
        {
            pfree(str);
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("debversion epoch exceeds 31-bit range")));
        }
        val = (int32) num;
    }
    pfree(str);
    PG_RETURN_INT32(val);
}

PG_FUNCTION_INFO_V1(get_debversion_upstream);
Datum
get_debversion_upstream(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    char *str = text_to_cstring((text *) v);
    const char *epoch;
    size_t epoch_len;
    const char *u;
    size_t ul;
    const char *r;
    size_t rl;
    text *res;

    debver_split_evr(str, &epoch, &epoch_len, &u, &ul, &r, &rl);
    res = cstring_to_text_with_len(u, ul);
    pfree(str);
    PG_RETURN_TEXT_P(res);
}

PG_FUNCTION_INFO_V1(get_debversion_revision);
Datum
get_debversion_revision(PG_FUNCTION_ARGS)
{
    struct varlena *v = PG_GETARG_VARLENA_PP(0);
    char *str = text_to_cstring((text *) v);
    const char *epoch;
    size_t epoch_len;
    const char *u;
    size_t ul;
    const char *r;
    size_t rl;
    text *res;

    debver_split_evr(str, &epoch, &epoch_len, &u, &ul, &r, &rl);
    if (r == NULL)
    {
        pfree(str);
        PG_RETURN_NULL();
    }
    res = cstring_to_text_with_len(r, rl);
    pfree(str);
    PG_RETURN_TEXT_P(res);
}
