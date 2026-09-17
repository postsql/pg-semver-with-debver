#ifndef SEMVER_H
#define SEMVER_H

#include "postgres.h"

/* heap format of version numbers */
typedef int32 vernum;

/* memory/heap structure (not for binary marshalling) */
typedef struct semver {
    int32  vl_len_;  /* varlena header */
    vernum numbers[3];
    char   prerel[]; /* pre-release, including the null byte for convenience */
} semver;

#define PG_GETARG_SEMVER_P(n) (semver *)PG_GETARG_POINTER(n)

char*   emit_semver(semver* version);
semver* make_semver(const int *numbers, const char* prerel);
semver* parse_semver(char* str, bool lax, bool throw, bool *bad);

#endif /* SEMVER_H */
