// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BYTEIO_H
#define MMGR_BYTEIO_H

#include "mmgr/cellularum_laboro/cellularum_laboro.h"
#include "mmgr/endian/endian.h"
#include "mmgr/memoria_operor/memoria_operor.h"
#include "mmgr/spatium/spatium.h"

MMGR_BEGIN_DECLS

typedef struct
{
    void (*put)(mmgr_spat *w, uint8_t b);
    void (*put_be)(mmgr_spat *w, uint64_t val, int32_t nbytes);
    void (*raw)(mmgr_spat *w, const void *src, size_t n);
    mmgr_bool (*take_be)(mmgr_fspat *r, size_t nbytes, uint64_t *out);
    mmgr_bool (*rd_u32)(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
    mmgr_bool (*rd_str)(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
    mmgr_bool (*mpint_fixed)(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
} ByteioNs;

void mmgr_byteio_put(mmgr_spat *w, uint8_t b);
void mmgr_byteio_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes);
void mmgr_byteio_bytes(mmgr_spat *w, const void *src, size_t n);
mmgr_bool mmgr_byteio_take_be(mmgr_fspat *r, size_t nbytes, uint64_t *out);
mmgr_bool mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
mmgr_bool mmgr_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);

static const ByteioNs byteio
    __attribute__((unused)) = {mmgr_byteio_put, mmgr_byteio_put_be, mmgr_byteio_bytes,  mmgr_byteio_take_be,
                               mmgr_rd_u32,     mmgr_rd_str,        mmgr_mpint_to_fixed};

MMGR_END_DECLS

#endif
