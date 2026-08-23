#ifndef MMGR_VERBUM_SCRUTOR_H
#define MMGR_VERBUM_SCRUTOR_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef mmgr_word mmgr_scrut_word;

#define MMGR_SWAR_BYTES (sizeof(mmgr_scrut_word))
#define MMGR_SWAR_LANE_BITS (MMGR_WORD_BITS)

MMGR_STATIC_ASSERT(sizeof(mmgr_scrut_word) == sizeof(mmgr_word),
                   "the lane carrier is the machine word - it is not separately sized");

#define MMGR_SWAR_ONES (((mmgr_scrut_word) ~(mmgr_scrut_word)0) / 0xFFu)
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)
#define MMGR_SWAR_LOW7 (MMGR_SWAR_ONES * 0x7Fu)

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
MMGR_NS_LAYOUT(VerbumScrutorNs, ge, le, spread, sub7, has_zero, eq, xor_, zero_lane, load, load_al);

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

MMGR_INLINE size_t mmgr_scrut_lanes(mmgr_scrut_word m)
{
    return (size_t)((mmgr_scrut_word)((mmgr_scrut_word)(m >> 7) * MMGR_SWAR_ONES) >> (MMGR_SWAR_LANE_BITS - 8u));
}

MMGR_INLINE size_t mmgr_scrut_lane_lo(mmgr_scrut_word m)
{
    return mmgr_scrut_lanes((mmgr_scrut_word)((mmgr_scrut_word)((mmgr_scrut_word)(m - 1u) & (mmgr_scrut_word)~m) &
                                              MMGR_VERBUM_SCRUTOR_HIGH));
}

MMGR_INLINE size_t mmgr_scrut_lane_hi(mmgr_scrut_word m)
{
    for (unsigned s = 8u; s < MMGR_SWAR_LANE_BITS; s <<= 1)
    {
        m = (mmgr_scrut_word)(m | (mmgr_scrut_word)(m >> s));
    }
    return mmgr_scrut_lanes(m) - 1u;
}

#if MMGR_HW_BIG_ENDIAN
#define mmgr_scrut_lane_first mmgr_scrut_lane_hi
#define mmgr_scrut_lane_last mmgr_scrut_lane_lo
#else
#define mmgr_scrut_lane_first mmgr_scrut_lane_lo
#define mmgr_scrut_lane_last mmgr_scrut_lane_hi
#endif

MMGR_INLINE mmgr_scrut_word mmgr_scrut_drop_lo(mmgr_scrut_word m)
{
    return (mmgr_scrut_word)(m & (mmgr_scrut_word)(m - 1u));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_drop_hi(mmgr_scrut_word m)
{
    mmgr_scrut_word s = m;
    for (unsigned k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        s = (mmgr_scrut_word)(s | (mmgr_scrut_word)(s >> k));
    }
    return (mmgr_scrut_word)(m & (mmgr_scrut_word) ~(mmgr_scrut_word)(s ^ (mmgr_scrut_word)(s >> 8)));
}

#if MMGR_HW_BIG_ENDIAN
#define mmgr_scrut_drop_first mmgr_scrut_drop_hi
#define mmgr_scrut_drop_last mmgr_scrut_drop_lo
#else
#define mmgr_scrut_drop_first mmgr_scrut_drop_lo
#define mmgr_scrut_drop_last mmgr_scrut_drop_hi
#endif

MMGR_INLINE size_t mmgr_scrut_zero_lane(mmgr_scrut_word m)
{
    return mmgr_scrut_lane_first(m);
}

MMGR_INLINE size_t mmgr_scrut_words(size_t bytes)
{
    return (bytes / MMGR_SWAR_BYTES) + (((bytes & (MMGR_SWAR_BYTES - 1u)) != 0u) ? 1u : 0u);
}

#define MMGR_SCAN_MAX_WORDS ((MMGR_CARCER_MAX + (MMGR_SWAR_BYTES - 1u)) / MMGR_SWAR_BYTES)

MMGR_STATIC_ASSERT(MMGR_SCAN_MAX_WORDS *MMGR_SWAR_BYTES >= MMGR_CARCER_MAX,
                   "the worst-case scan does not cover the largest tenant");
MMGR_STATIC_ASSERT((MMGR_SCAN_MAX_WORDS - 1u) * MMGR_SWAR_BYTES < MMGR_CARCER_MAX,
                   "the worst-case scan is padded by a whole word - the word count is not tight");

MMGR_INLINE mmgr_scrut_word mmgr_scrut_bytes_below(size_t n)
{
    if (n >= MMGR_SWAR_BYTES)
    {
        return (mmgr_scrut_word) ~(mmgr_scrut_word)0;
    }
#if MMGR_HW_BIG_ENDIAN
    return (mmgr_scrut_word)((mmgr_scrut_word) ~(mmgr_scrut_word)0 << ((MMGR_SWAR_BYTES - n) * 8u));
#else
    return (mmgr_scrut_word)((mmgr_scrut_word) ~(mmgr_scrut_word)0 >> ((MMGR_SWAR_BYTES - n) * 8u));
#endif
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_lanes_below(size_t n)
{
    return (mmgr_scrut_word)(mmgr_scrut_bytes_below(n) & MMGR_VERBUM_SCRUTOR_HIGH);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_tail_mask(size_t cap, size_t wi)
{
    return mmgr_scrut_lanes_below(cap - (wi * MMGR_SWAR_BYTES));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_lanes_before(mmgr_scrut_word m)
{
#if MMGR_HW_BIG_ENDIAN
    mmgr_scrut_word s = m;
    for (unsigned k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        s = (mmgr_scrut_word)(s | (mmgr_scrut_word)(s >> k));
    }
    return (mmgr_scrut_word)(~s & MMGR_VERBUM_SCRUTOR_HIGH);
#else
    return (mmgr_scrut_word)((mmgr_scrut_word)((mmgr_scrut_word)(m - 1u) & (mmgr_scrut_word)~m) &
                             MMGR_VERBUM_SCRUTOR_HIGH);
#endif
}

#define MMGR_FAM_CS 0x60u
#define MMGR_FAM_CI 0x40u

MMGR_INLINE mmgr_scrut_word mmgr_scrut_fam_eq(mmgr_scrut_word w, unsigned mask, uint8_t f)
{
    const mmgr_scrut_word bits = (mmgr_scrut_word)(MMGR_SWAR_ONES * (mmgr_scrut_word)mask);
    return mmgr_scrut_has_zero((mmgr_scrut_word)((w & bits) ^ (MMGR_SWAR_ONES * (mmgr_scrut_word)(f & mask))));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_any_upper(mmgr_scrut_word w)
{
    return mmgr_scrut_fam_eq(w, 0x60u, 0x40u);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_any_digit(mmgr_scrut_word w)
{
    return mmgr_scrut_fam_eq(w, 0xF0u, 0x30u);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_alpha(mmgr_scrut_word w)
{
    const mmgr_scrut_word lo = (mmgr_scrut_word)(w | (MMGR_SWAR_ONES * 0x20u));
    return (mmgr_scrut_word)(mmgr_scrut_ge(lo, (mmgr_scrut_word)'a') & mmgr_scrut_le(lo, (mmgr_scrut_word)'z') & ~lo);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_fold_lower(mmgr_scrut_word w)
{
    return (mmgr_scrut_word)(w | (mmgr_scrut_word)(mmgr_scrut_alpha(w) >> 2));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_run(mmgr_scrut_word m, size_t n)
{
    size_t have = 1u;

    while (have < n)
    {
        const size_t step = (have < n - have) ? have : n - have;
#if MMGR_HW_BIG_ENDIAN
        m &= (mmgr_scrut_word)(m << (step * 8u));
#else
        m &= (mmgr_scrut_word)(m >> (step * 8u));
#endif
        have += step;
    }
    return m;
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_run_edge(size_t n)
{
    if (n <= 1u || n > MMGR_SWAR_BYTES)
    {
        return 0;
    }
    return (mmgr_scrut_word)(MMGR_VERBUM_SCRUTOR_HIGH & ~mmgr_scrut_lanes_below(MMGR_SWAR_BYTES - n + 1u));
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_load(const char *p)
{
    return (mmgr_scrut_word)proxim.load(p, MMGR_SWAR_BYTES);
}

MMGR_INLINE mmgr_scrut_word mmgr_scrut_load_al(const char *p)
{
    return (mmgr_scrut_word)proxim.al_load(p, MMGR_SWAR_BYTES);
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

MMGR_NS VerbumScrutorNs scrut MMGR_UNUSED = {
    .ge = mmgr_scrut_ge,
    .le = mmgr_scrut_le,
    .spread = mmgr_scrut_spread,
    .sub7 = mmgr_scrut_sub7,
    .has_zero = mmgr_scrut_has_zero,
    .eq = mmgr_scrut_eq_sel,
    .xor_ = mmgr_scrut_xor_sel,
    .zero_lane = mmgr_scrut_zero_lane,
    .load = mmgr_scrut_load,
    .load_al = mmgr_scrut_load_al,
};

MMGR_FINIS_DECLS

#endif
