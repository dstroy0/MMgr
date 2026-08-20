// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SWAR_H
#define MMGR_SWAR_H

#include "mmgr/rawmemcpy/rawmemcpy.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#if MMGR_SWAR_BITS == 64
typedef uint64_t mmgr_swar_word;
#elif MMGR_SWAR_BITS == 32
typedef uint32_t mmgr_swar_word;
#elif MMGR_SWAR_BITS == 16
typedef uint16_t mmgr_swar_word;
#elif MMGR_SWAR_BITS == 8

typedef uint8_t mmgr_swar_word;
#else
#error "MMGR_SWAR_BITS must be 8, 16, 32 or 64"
#endif

#define MMGR_SWAR_BYTES ((size_t)(MMGR_SWAR_BITS / 8u))

MMGR_STATIC_ASSERT(sizeof(mmgr_swar_word) * 8u == MMGR_SWAR_BITS,
                   "the lane carrier must be exactly MMGR_SWAR_BITS wide");

#define MMGR_SWAR_ONES (((mmgr_swar_word) ~(mmgr_swar_word)0) / 0xFFu)
#define MMGR_SWAR_HIGH (MMGR_SWAR_ONES * 0x80u)
#define MMGR_SWAR_LOW7 (MMGR_SWAR_ONES * 0x7Fu)

#if MMGR_SWAR_BITS <= 32
#define MMGR_SWAR_CTZ(v) __builtin_ctz((unsigned)(v))
#define MMGR_SWAR_CLZ(v) __builtin_clz((unsigned)(v))
#define MMGR_SWAR_CLZ_WIDTH 32u
#else
#define MMGR_SWAR_CTZ(v) __builtin_ctzll((unsigned long long)(v))
#define MMGR_SWAR_CLZ(v) __builtin_clzll((unsigned long long)(v))
#define MMGR_SWAR_CLZ_WIDTH 64u
#endif

#define MMGR_SWAR_GO 0
#define MMGR_SWAR_YES 1
#define MMGR_SWAR_NO 2

typedef struct
{
    mmgr_swar_word (*ge)(mmgr_swar_word a, mmgr_swar_word v);
    mmgr_swar_word (*le)(mmgr_swar_word a, mmgr_swar_word v);
    mmgr_swar_word (*spread)(mmgr_swar_word m);
    mmgr_swar_word (*sub7)(mmgr_swar_word a, mmgr_swar_word lo);
    mmgr_swar_word (*has_zero)(mmgr_swar_word w);
    mmgr_swar_word (*eq)(mmgr_swar_word w, uint8_t c, mmgr_bool ci);
    mmgr_swar_word (*xor_)(mmgr_swar_word wa, mmgr_swar_word wb, mmgr_bool ci);
    size_t (*zero_lane)(mmgr_swar_word m);
    mmgr_swar_word (*load)(const char *p);
    mmgr_swar_word (*load_al)(const char *p);
} SwarNs;

mmgr_swar_word mmgr_swar_ge(mmgr_swar_word a, mmgr_swar_word v);
mmgr_swar_word mmgr_swar_le(mmgr_swar_word a, mmgr_swar_word v);
mmgr_swar_word mmgr_swar_spread(mmgr_swar_word m);
mmgr_swar_word mmgr_swar_sub7(mmgr_swar_word a, mmgr_swar_word lo);
mmgr_swar_word mmgr_swar_has_zero(mmgr_swar_word w);
mmgr_swar_word mmgr_swar_eq(mmgr_swar_word w, uint8_t c);
mmgr_swar_word mmgr_swar_eq_ci(mmgr_swar_word w, uint8_t c);
mmgr_swar_word mmgr_swar_eq_sel(mmgr_swar_word w, uint8_t c, mmgr_bool ci);
mmgr_swar_word mmgr_swar_xor(mmgr_swar_word wa, mmgr_swar_word wb);
mmgr_swar_word mmgr_swar_xor_ci(mmgr_swar_word wa, mmgr_swar_word wb);
mmgr_swar_word mmgr_swar_xor_sel(mmgr_swar_word wa, mmgr_swar_word wb, mmgr_bool ci);
size_t mmgr_swar_zero_lane(mmgr_swar_word m);
mmgr_swar_word mmgr_swar_load(const char *p);
mmgr_swar_word mmgr_swar_load_al(const char *p);

static const SwarNs swar __attribute__((unused)) = {
    mmgr_swar_ge,     mmgr_swar_le,      mmgr_swar_spread,    mmgr_swar_sub7, mmgr_swar_has_zero,
    mmgr_swar_eq_sel, mmgr_swar_xor_sel, mmgr_swar_zero_lane, mmgr_swar_load, mmgr_swar_load_al};

MMGR_END_DECLS

#endif
