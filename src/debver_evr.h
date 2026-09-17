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

#ifndef DEBVER_EVR_H
#define DEBVER_EVR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Low-level slice comparison algorithm from libsolv adapted for Debian versioning.
 * Compares s1 (up to q1) with s2 (up to q2).
 * Returns -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2.
 */
int solv_vercmp_deb(const char *s1, const char *q1, const char *s2, const char *q2);

/*
 * Parse and split an EVR string ([epoch:]upstream_version[-debian_revision]).
 * Sets pointers and lengths to the respective components within evr.
 * If epoch is absent, *epoch_p is NULL and *epoch_len is 0.
 * If debian_revision is absent, *rev_p is NULL and *rev_len is 0.
 */
void debver_split_evr(const char *evr,
                      const char **epoch_p, size_t *epoch_len,
                      const char **upstream_p, size_t *upstream_len,
                      const char **rev_p, size_t *rev_len);

/*
 * Compare two Debian version strings.
 * Returns -1 if evr1 < evr2, 0 if evr1 == evr2, 1 if evr1 > evr2.
 */
int debver_evrcmp(const char *evr1, const char *evr2);

#ifdef __cplusplus
}
#endif

#endif /* DEBVER_EVR_H */
