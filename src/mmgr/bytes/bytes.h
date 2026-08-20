// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BYTES_H
#define MMGR_BYTES_H

#include "mmgr/endian/endian.h"
#include "mmgr/protomem/protomem.h"
#include "mmgr/protostr/protostr.h"
#include "mmgr/span/span.h"

MMGR_BEGIN_DECLS

typedef struct
{
    void (*put)(mmgr_span *w, uint8_t b);
    void (*put_be)(mmgr_span *w, uint64_t val, int32_t nbytes);
    void (*raw)(mmgr_span *w, const void *src, size_t n);
    mmgr_bool (*take_be)(mmgr_cspan *r, size_t nbytes, uint64_t *out);
    mmgr_bool (*rd_u32)(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
    mmgr_bool (*rd_str)(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
    mmgr_bool (*mpint_fixed)(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
} BytesNs;

void mmgr_bw_put(mmgr_span *w, uint8_t b);
void mmgr_bw_put_be(mmgr_span *w, uint64_t val, int32_t nbytes);
void mmgr_bw_bytes(mmgr_span *w, const void *src, size_t n);
mmgr_bool mmgr_br_take_be(mmgr_cspan *r, size_t nbytes, uint64_t *out);
mmgr_bool mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
mmgr_bool mmgr_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);

static const BytesNs bytes __attribute__((unused)) = {mmgr_bw_put, mmgr_bw_put_be, mmgr_bw_bytes,      mmgr_br_take_be,
                                                      mmgr_rd_u32, mmgr_rd_str,    mmgr_mpint_to_fixed};

MMGR_END_DECLS

#endif
