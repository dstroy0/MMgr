// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_VERBUM_SCRUTOR_H
#define MMGR_VERBUM_SCRUTOR_H

#include "proximus_operor/proximus_operor.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

// The carrier is the machine word. Always, on every target, with no choice about it.
//
// A narrower carrier is never faster. A 16-bit load on a 32-bit machine moves the same cache line,
// occupies the same load port and then processes a quarter of the lanes; on most ISAs it also costs
// an extra zero- or sign-extend, and in C every operand narrower than int is promoted to int before
// the arithmetic happens anyway. So the lane count follows the register, and the only thing that
// varies between targets is how many lanes a register holds.
typedef mmgr_word mmgr_scrut_word;

#define MMGR_SWAR_BYTES (sizeof(mmgr_scrut_word))
#define MMGR_SWAR_LANE_BITS (MMGR_WORD_BITS)

MMGR_STATIC_ASSERT(sizeof(mmgr_scrut_word) == sizeof(mmgr_word),
                   "the lane carrier is the machine word - it is not separately sized");

#define MMGR_SWAR_ONES (((mmgr_scrut_word) ~(mmgr_scrut_word)0) / 0xFFu)
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)
#define MMGR_SWAR_LOW7 (MMGR_SWAR_ONES * 0x7Fu)

// One spelling for the bit scan. ctz reads the low end, so widening the operand cannot move the
// answer and the 64-bit builtin serves every width. clz reads the high end, so a value widened to
// 64 bits gains leading zeros and the count has to be pulled back by the difference.
#define MMGR_SWAR_CTZ(v) __builtin_ctzll((unsigned long long)(v))
#define MMGR_SWAR_CLZ(v) __builtin_clzll((unsigned long long)(v))
#define MMGR_SWAR_CLZ_WIDTH 64u

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

MMGR_INLINE mmgr_scrut_word mmgr_scrut_ge(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - v * MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_le(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((v * MMGR_SWAR_ONES | MMGR_VERBUM_SCRUTOR_HIGH) - a) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_spread(mmgr_scrut_word m)
{

    return (mmgr_scrut_word)(m + (m - (m >> 7)));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_sub7(mmgr_scrut_word a, mmgr_scrut_word lo)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - lo * MMGR_SWAR_ONES) & MMGR_SWAR_LOW7;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_has_zero(mmgr_scrut_word w)
{
    return ~(((w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | w) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(w ^ (MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

MMGR_INLINE size_t mmgr_scrut_zero_lane(mmgr_scrut_word m)
{
#if MMGR_HW_BIG_ENDIAN
    return (size_t)((MMGR_SWAR_CLZ(m) - (MMGR_SWAR_CLZ_WIDTH - MMGR_SWAR_BITS)) >> 3);
#else
    return (size_t)(MMGR_SWAR_CTZ(m) >> 3);
#endif
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_load(const char *p)
{
    return (mmgr_scrut_word)mmgr_proxim_load(p, MMGR_SWAR_BYTES);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_load_al(const char *p)
{
    return (mmgr_scrut_word)mmgr_aequus_load(p, MMGR_SWAR_BYTES);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    return wa ^ wb;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor_ci(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    mmgr_scrut_word x = wa ^ wb;
    mmgr_scrut_word lo = wa | (MMGR_SWAR_ONES * 0x20u);
    mmgr_scrut_word alpha = mmgr_scrut_ge(lo, 'a') & mmgr_scrut_le(lo, 'z') & ~lo;
    return x & ~(alpha >> 2);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq_ci(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(mmgr_scrut_xor_ci(w, MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq_sel(mmgr_scrut_word w, uint8_t c, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_eq_ci(w, c);
    }
    return mmgr_scrut_eq(w, c);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor_sel(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_xor_ci(wa, wb);
    }
    return mmgr_scrut_xor(wa, wb);
}
mmgr_scrut_word mmgr_scrut_load(const char *p);
mmgr_scrut_word mmgr_scrut_load_al(const char *p);

static const VerbumScrutorNs scrut __attribute__((unused)) = {
    mmgr_scrut_ge,     mmgr_scrut_le,      mmgr_scrut_spread,    mmgr_scrut_sub7, mmgr_scrut_has_zero,
    mmgr_scrut_eq_sel, mmgr_scrut_xor_sel, mmgr_scrut_zero_lane, mmgr_scrut_load, mmgr_scrut_load_al};

MMGR_END_DECLS

#endif
