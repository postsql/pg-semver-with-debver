/*
 * Copyright (c) 2007-2009, Novell Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of Novell nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "debver_evr.h"
#include <string.h>

/*
 * Debian version comparison algorithm from libsolv (evr.c: solv_vercmp_deb)
 */
int
solv_vercmp_deb(const char *s1, const char *q1, const char *s2, const char *q2)
{
    int r, c1, c2;
    while (1)
    {
        c1 = s1 < q1 ? *(const unsigned char *)s1++ : 0;
        c2 = s2 < q2 ? *(const unsigned char *)s2++ : 0;
        if ((c1 >= '0' && c1 <= '9') && (c2 >= '0' && c2 <= '9'))
        {
            while (c1 == '0')
                c1 = s1 < q1 ? *(const unsigned char *)s1++ : 0;
            while (c2 == '0')
                c2 = s2 < q2 ? *(const unsigned char *)s2++ : 0;
            r = 0;
            while ((c1 >= '0' && c1 <= '9') && (c2 >= '0' && c2 <= '9'))
            {
                if (!r)
                    r = c1 - c2;
                c1 = s1 < q1 ? *(const unsigned char *)s1++ : 0;
                c2 = s2 < q2 ? *(const unsigned char *)s2++ : 0;
            }
            if (c1 >= '0' && c1 <= '9')
                return 1;
            if (c2 >= '0' && c2 <= '9')
                return -1;
            if (r)
                return r < 0 ? -1 : 1;
        }
        c1 = c1 == '~' ? -1 : !c1 || (c1 >= '0' && c1 <= '9') || (c1 >= 'A' && c1 <= 'Z') || (c1 >= 'a' && c1 <= 'z') ? c1 : c1 + 256;
        c2 = c2 == '~' ? -1 : !c2 || (c2 >= '0' && c2 <= '9') || (c2 >= 'A' && c2 <= 'Z') || (c2 >= 'a' && c2 <= 'z') ? c2 : c2 + 256;
        r = c1 - c2;
        if (r)
            return r < 0 ? -1 : 1;
        if (!c1)
            return 0;
    }
}

void
debver_split_evr(const char *evr,
                 const char **epoch_p, size_t *epoch_len,
                 const char **upstream_p, size_t *upstream_len,
                 const char **rev_p, size_t *rev_len)
{
    const char *colon = strchr(evr, ':');
    const char *version_start;
    const char *last_hyphen;

    if (colon != NULL)
    {
        *epoch_p = evr;
        *epoch_len = (size_t)(colon - evr);
        version_start = colon + 1;
    }
    else
    {
        *epoch_p = NULL;
        *epoch_len = 0;
        version_start = evr;
    }

    last_hyphen = strrchr(version_start, '-');
    if (last_hyphen != NULL)
    {
        *upstream_p = version_start;
        *upstream_len = (size_t)(last_hyphen - version_start);
        *rev_p = last_hyphen + 1;
        *rev_len = strlen(*rev_p);
    }
    else
    {
        *upstream_p = version_start;
        *upstream_len = strlen(version_start);
        *rev_p = NULL;
        *rev_len = 0;
    }
}

int
debver_evrcmp(const char *evr1, const char *evr2)
{
    const char *e1, *u1, *r1;
    size_t el1, ul1, rl1;
    const char *e2, *u2, *r2;
    size_t el2, ul2, rl2;
    int r;

    debver_split_evr(evr1, &e1, &el1, &u1, &ul1, &r1, &rl1);
    debver_split_evr(evr2, &e2, &el2, &u2, &ul2, &r2, &rl2);

    /* Compare epochs (omitted epoch is equivalent to "0") */
    if (e1 != NULL && e2 != NULL)
    {
        r = solv_vercmp_deb(e1, e1 + el1, e2, e2 + el2);
    }
    else if (e1 != NULL)
    {
        r = solv_vercmp_deb(e1, e1 + el1, "0", "0" + 1);
    }
    else if (e2 != NULL)
    {
        r = solv_vercmp_deb("0", "0" + 1, e2, e2 + el2);
    }
    else
    {
        r = 0;
    }
    if (r != 0)
        return r;

    /* Compare upstream version */
    r = solv_vercmp_deb(u1, u1 + ul1, u2, u2 + ul2);
    if (r != 0)
        return r;

    /* Compare debian revision */
    if (r1 != NULL && r2 != NULL)
    {
        r = solv_vercmp_deb(r1, r1 + rl1, r2, r2 + rl2);
    }
    else if (r1 != NULL)
    {
        r = solv_vercmp_deb(r1, r1 + rl1, "", "");
    }
    else if (r2 != NULL)
    {
        r = solv_vercmp_deb("", "", r2, r2 + rl2);
    }
    else
    {
        r = 0;
    }
    return r;
}
