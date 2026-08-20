// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_VERBUM_SCRUTOR_H
#define MMGR_VERBUM_SCRUTOR_H

#include "proximus_operor/proximus_operor.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#if MMGR_SWAR_BITS == 64
typedef uint64_t mmgr_scrut_word;
#elif MMGR_SWAR_BITS == 32
typedef uint32_t mmgr_scrut_word;
#elif MMGR_SWAR_BITS == 16
typedef uint16_t mmgr_scrut_word;
#elif MMGR_SWAR_BITS == 8

typedef uint8_t mmgr_scrut_word;
#else
#error "MMGR_SWAR_BITS must be 8, 16, 32 or 64"
#endif

#define MMGR_SWAR_BYTES ((size_t)(MMGR_SWAR_BITS / 8u))

MMGR_STATIC_ASSERT(sizeof(mmgr_scrut_word) * 8u == MMGR_SWAR_BITS,
                   "the lane carrier must be exactly MMGR_SWAR_BITS wide");

#define MMGR_SWAR_ONES (((mmgr_scrut_word) ~(mmgr_scrut_word)0) / 0xFFu)
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)
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
    mmgr_scrut_word (*ge)(mmgr_scrut_word a, mmgr_scrut_word v);
    mmgr_scrut_word (*le)(mmgr_scrut_word a, mmgr_scrut_word v);
    mmgr_scrut_word (*spread)(mmgr_scrut_word m);
    mmgr_scrut_word (*sub7)(mmgr_scrut_word a, mmgr_scrut_word lo);
    mmgr_scrut_word (*has_zero)(mmgr_scrut_word w);
    mmgr_scrut_word (*eq)(mmgr_scrut_word w, uint8_t c, mmgr_bool ci);
    mmgr_scrut_word (*xor_)(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci);
    size_t (*zero_lane)(mmgr_scrut_word m);
    mmgr_scrut_word (*load)(const char *p);
    mmgr_scrut_word (*load_al)(const char *p);
} VerbumScrutorNs;

mmgr_scrut_word mmgr_scrut_ge(mmgr_scrut_word a, mmgr_scrut_word v);
mmgr_scrut_word mmgr_scrut_le(mmgr_scrut_word a, mmgr_scrut_word v);
mmgr_scrut_word mmgr_scrut_spread(mmgr_scrut_word m);
mmgr_scrut_word mmgr_scrut_sub7(mmgr_scrut_word a, mmgr_scrut_word lo);
mmgr_scrut_word mmgr_scrut_has_zero(mmgr_scrut_word w);
mmgr_scrut_word mmgr_scrut_eq(mmgr_scrut_word w, uint8_t c);
mmgr_scrut_word mmgr_scrut_eq_ci(mmgr_scrut_word w, uint8_t c);
mmgr_scrut_word mmgr_scrut_eq_sel(mmgr_scrut_word w, uint8_t c, mmgr_bool ci);
mmgr_scrut_word mmgr_scrut_xor(mmgr_scrut_word wa, mmgr_scrut_word wb);
mmgr_scrut_word mmgr_scrut_xor_ci(mmgr_scrut_word wa, mmgr_scrut_word wb);
mmgr_scrut_word mmgr_scrut_xor_sel(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci);
size_t mmgr_scrut_zero_lane(mmgr_scrut_word m);
mmgr_scrut_word mmgr_scrut_load(const char *p);
mmgr_scrut_word mmgr_scrut_load_al(const char *p);

static const VerbumScrutorNs scrut __attribute__((unused)) = {
    mmgr_scrut_ge,     mmgr_scrut_le,      mmgr_scrut_spread,    mmgr_scrut_sub7, mmgr_scrut_has_zero,
    mmgr_scrut_eq_sel, mmgr_scrut_xor_sel, mmgr_scrut_zero_lane, mmgr_scrut_load, mmgr_scrut_load_al};

MMGR_END_DECLS

#endif
