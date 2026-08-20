// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_BYTES_H
#define PROTOCORE_BYTES_H

#include "mmgr/endian/endian.h"
#include "mmgr/protomem/protomem.h"
#include "mmgr/protostr/protostr.h"
#include "mmgr/span/span.h"

PROTOCORE_BEGIN_DECLS

typedef struct
{
    void (*put)(protocore_span *w, uint8_t b);
    void (*put_be)(protocore_span *w, uint64_t val, int32_t nbytes);
    void (*raw)(protocore_span *w, const void *src, size_t n);
    proto_bool (*take_be)(protocore_cspan *r, size_t nbytes, uint64_t *out);
    proto_bool (*rd_u32)(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
    proto_bool (*rd_str)(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
    proto_bool (*mpint_fixed)(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
} BytesNs;

void protocore_bw_put(protocore_span *w, uint8_t b);
void protocore_bw_put_be(protocore_span *w, uint64_t val, int32_t nbytes);
void protocore_bw_bytes(protocore_span *w, const void *src, size_t n);
proto_bool protocore_br_take_be(protocore_cspan *r, size_t nbytes, uint64_t *out);
proto_bool protocore_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
proto_bool protocore_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
proto_bool protocore_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);

static const BytesNs bytes
    __attribute__((unused)) = {protocore_bw_put, protocore_bw_put_be, protocore_bw_bytes,      protocore_br_take_be,
                               protocore_rd_u32, protocore_rd_str,    protocore_mpint_to_fixed};

PROTOCORE_END_DECLS

#endif
