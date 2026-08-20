// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_SWAR_H
#define PROTOCORE_SWAR_H

#include "mmgr/rawmemcpy/rawmemcpy.h"

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#if PROTO_SWAR_BITS == 64
typedef uint64_t protocore_swar_word;
#elif PROTO_SWAR_BITS == 32
typedef uint32_t protocore_swar_word;
#elif PROTO_SWAR_BITS == 16
typedef uint16_t protocore_swar_word;
#elif PROTO_SWAR_BITS == 8

typedef uint8_t protocore_swar_word;
#else
#error "PROTO_SWAR_BITS must be 8, 16, 32 or 64"
#endif

#define PROTOCORE_SWAR_BYTES ((size_t)(PROTO_SWAR_BITS / 8u))

static_assert(sizeof(protocore_swar_word) * 8u == PROTO_SWAR_BITS,
              "the lane carrier must be exactly PROTO_SWAR_BITS wide");

#define PROTOCORE_SWAR_ONES (((protocore_swar_word) ~(protocore_swar_word)0) / 0xFFu)
#define PROTOCORE_SWAR_HIGH (PROTOCORE_SWAR_ONES * 0x80u)
#define PROTOCORE_SWAR_LOW7 (PROTOCORE_SWAR_ONES * 0x7Fu)

#if PROTO_SWAR_BITS <= 32
#define PROTOCORE_SWAR_CTZ(v) __builtin_ctz((unsigned)(v))
#define PROTOCORE_SWAR_CLZ(v) __builtin_clz((unsigned)(v))
#define PROTOCORE_SWAR_CLZ_WIDTH 32u
#else
#define PROTOCORE_SWAR_CTZ(v) __builtin_ctzll((unsigned long long)(v))
#define PROTOCORE_SWAR_CLZ(v) __builtin_clzll((unsigned long long)(v))
#define PROTOCORE_SWAR_CLZ_WIDTH 64u
#endif

#define PROTOCORE_SWAR_GO 0
#define PROTOCORE_SWAR_YES 1
#define PROTOCORE_SWAR_NO 2

typedef struct
{
    protocore_swar_word (*ge)(protocore_swar_word a, protocore_swar_word v);
    protocore_swar_word (*le)(protocore_swar_word a, protocore_swar_word v);
    protocore_swar_word (*spread)(protocore_swar_word m);
    protocore_swar_word (*sub7)(protocore_swar_word a, protocore_swar_word lo);
    protocore_swar_word (*has_zero)(protocore_swar_word w);
    protocore_swar_word (*eq)(protocore_swar_word w, uint8_t c, proto_bool ci);
    protocore_swar_word (*xor_)(protocore_swar_word wa, protocore_swar_word wb, proto_bool ci);
    size_t (*zero_lane)(protocore_swar_word m);
    protocore_swar_word (*load)(const char *p);
    protocore_swar_word (*load_al)(const char *p);
} SwarNs;

protocore_swar_word protocore_swar_ge(protocore_swar_word a, protocore_swar_word v);
protocore_swar_word protocore_swar_le(protocore_swar_word a, protocore_swar_word v);
protocore_swar_word protocore_swar_spread(protocore_swar_word m);
protocore_swar_word protocore_swar_sub7(protocore_swar_word a, protocore_swar_word lo);
protocore_swar_word protocore_swar_has_zero(protocore_swar_word w);
protocore_swar_word protocore_swar_eq(protocore_swar_word w, uint8_t c);
protocore_swar_word protocore_swar_eq_ci(protocore_swar_word w, uint8_t c);
protocore_swar_word protocore_swar_eq_sel(protocore_swar_word w, uint8_t c, proto_bool ci);
protocore_swar_word protocore_swar_xor(protocore_swar_word wa, protocore_swar_word wb);
protocore_swar_word protocore_swar_xor_ci(protocore_swar_word wa, protocore_swar_word wb);
protocore_swar_word protocore_swar_xor_sel(protocore_swar_word wa, protocore_swar_word wb, proto_bool ci);
size_t protocore_swar_zero_lane(protocore_swar_word m);
protocore_swar_word protocore_swar_load(const char *p);
protocore_swar_word protocore_swar_load_al(const char *p);

static const SwarNs swar __attribute__((unused)) = {
    protocore_swar_ge,       protocore_swar_le,     protocore_swar_spread,  protocore_swar_sub7,
    protocore_swar_has_zero, protocore_swar_eq_sel, protocore_swar_xor_sel, protocore_swar_zero_lane,
    protocore_swar_load,     protocore_swar_load_al};

PROTOCORE_END_DECLS

#endif
