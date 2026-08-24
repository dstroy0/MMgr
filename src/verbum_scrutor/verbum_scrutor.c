#include "verbum_scrutor/verbum_scrutor.h"

typedef struct
{
    mmgr_word w;
    mmgr_word v;
    mmgr_word m;
    uint8_t byte;
    uint8_t fam;
    mmgr_bool ci;
} ScrutLaneCtx;

typedef struct
{
    mmgr_word m;
    size_t n;
    size_t wi;
} ScrutMaskCtx;

typedef struct
{
    mmgr_word w;
    const void *at;
    size_t n;
} ScrutWordCtx;

MMGR_INLINE mmgr_word scrut_below_lo(const ScrutMaskCtx *c)
{
    return (c->m - 1u) & ~c->m & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_smear(const ScrutMaskCtx *c)
{
    mmgr_word m = c->m;

    for (uint32_t k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        m |= (m >> k);
    }
    return m;
}

MMGR_INLINE mmgr_word scrut_ge(const ScrutLaneCtx *c)
{
    return ((c->w | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_le(const ScrutLaneCtx *c)
{
    return ((MMGR_SWAR_ONES * c->byte | MMGR_VERBUM_SCRUTOR_HIGH) - c->w) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_sub7(const ScrutLaneCtx *c)
{
    return ((c->w | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_SWAR_LOW7;
}

MMGR_INLINE mmgr_word scrut_has_zero(const ScrutLaneCtx *c)
{
    return ~(((c->w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | c->w) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_alpha(const ScrutLaneCtx *c)
{
    const mmgr_word lo = c->w | (MMGR_SWAR_ONES * 0x20u);

    return MMGR_CALL(scrut_ge, ScrutLaneCtx, .w = lo, .byte = 'a') &
           MMGR_CALL(scrut_le, ScrutLaneCtx, .w = lo, .byte = 'z') & ~lo;
}

MMGR_INLINE mmgr_word scrut_xor(const ScrutLaneCtx *c)
{
    const mmgr_word x = c->w ^ c->v;

    if (!c->ci)
    {
        return x;
    }
    return x & ~(MMGR_CALL(scrut_alpha, ScrutLaneCtx, .w = c->w) >> 2);
}

MMGR_INLINE mmgr_word scrut_eq(const ScrutLaneCtx *c)
{
    const mmgr_word broadcast = MMGR_SWAR_ONES * c->byte;
    const mmgr_word x = MMGR_CALL(scrut_xor, ScrutLaneCtx, .w = c->w, .v = broadcast, .ci = c->ci);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .w = x);
}

MMGR_INLINE mmgr_word scrut_fam_eq(const ScrutLaneCtx *c)
{
    const mmgr_word bits = MMGR_SWAR_ONES * c->fam;
    const mmgr_word want = MMGR_SWAR_ONES * (c->byte & c->fam);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .w = (c->w & bits) ^ want);
}

MMGR_INLINE mmgr_word scrut_any_upper(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .w = c->w, .fam = MMGR_FAM_CS, .byte = MMGR_FAM_CI);
}

MMGR_INLINE mmgr_word scrut_any_digit(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .w = c->w, .fam = 0xF0u, .byte = 0x30u);
}

MMGR_INLINE size_t scrut_lane_count(const ScrutLaneCtx *c)
{
    return ((c->m >> 7) * MMGR_SWAR_ONES) >> (MMGR_SWAR_LANE_BITS - 8u);
}

MMGR_INLINE size_t scrut_lane_lo(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .m = MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .m = c->m));
}

MMGR_INLINE size_t scrut_lane_hi(const ScrutLaneCtx *c)
{
    if (c->m == 0u)
    {
        return MMGR_SWAR_BYTES;
    }
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .m = MMGR_CALL(scrut_smear, ScrutMaskCtx, .m = c->m)) - 1u;
}

MMGR_INLINE mmgr_word scrut_spread(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->m + (c->m - (c->m >> 7)));
}

MMGR_INLINE mmgr_word scrut_drop_lo(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->m & (c->m - 1u));
}

MMGR_INLINE mmgr_word scrut_drop_hi(const ScrutMaskCtx *c)
{
    const mmgr_word s = MMGR_CALL(scrut_smear, ScrutMaskCtx, .m = c->m);

    return c->m & ~(s ^ (s >> 8));
}

MMGR_INLINE mmgr_word scrut_bytes_below(const ScrutMaskCtx *c)
{
    const mmgr_word all = (mmgr_word) ~(mmgr_word)0;

    if (c->n == 0u)
    {
        return 0;
    }
    if (c->n >= MMGR_SWAR_BYTES)
    {
        return all;
    }
#if MMGR_HW_BIG_ENDIAN
    return all << ((MMGR_SWAR_BYTES - c->n) * 8u);
#else
    return all >> ((MMGR_SWAR_BYTES - c->n) * 8u);
#endif
}

MMGR_INLINE mmgr_word scrut_lanes_below(const ScrutMaskCtx *c)
{
    return MMGR_CALL(scrut_bytes_below, ScrutMaskCtx, .n = c->n) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_tail_mask(const ScrutMaskCtx *c)
{
    const size_t done = c->wi * MMGR_SWAR_BYTES;

    if (done >= c->n)
    {
        return 0;
    }
    return MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .n = c->n - done);
}

MMGR_INLINE mmgr_word scrut_lanes_before(const ScrutMaskCtx *c)
{
#if MMGR_HW_BIG_ENDIAN
    return ~MMGR_CALL(scrut_smear, ScrutMaskCtx, .m = c->m) & MMGR_VERBUM_SCRUTOR_HIGH;
#else
    return MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .m = c->m);
#endif
}

MMGR_INLINE mmgr_word scrut_run(const ScrutMaskCtx *c)
{
    mmgr_word m = c->m;
    size_t have = 1u;

    if (c->n > MMGR_SWAR_BYTES)
    {
        return 0;
    }
    while (have < c->n)
    {
        const size_t step = (have < c->n - have) ? have : c->n - have;
#if MMGR_HW_BIG_ENDIAN
        m &= (m << (step * 8u));
#else
        m &= (m >> (step * 8u));
#endif
        have += step;
    }
    return m;
}

MMGR_INLINE mmgr_word scrut_run_edge(const ScrutMaskCtx *c)
{
    if ((c->n <= 1u) || (c->n > MMGR_SWAR_BYTES))
    {
        return 0;
    }
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .n = MMGR_SWAR_BYTES - c->n + 1u);
}

MMGR_INLINE mmgr_word scrut_load(const ScrutWordCtx *c)
{
    return MMGR_CALL(proxim.load, ProximusCfg, .at = c->at);
}

MMGR_INLINE mmgr_word scrut_load_al(const ScrutWordCtx *c)
{
    return MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->at);
}

MMGR_INLINE mmgr_word scrut_fold_lower(const ScrutWordCtx *c)
{
    return c->w | (MMGR_CALL(scrut_alpha, ScrutLaneCtx, .w = c->w) >> 2);
}

MMGR_INLINE size_t scrut_words(const ScrutWordCtx *c)
{
    return (c->n / MMGR_SWAR_BYTES) + (((c->n & (MMGR_SWAR_BYTES - 1u)) != 0u) ? 1u : 0u);
}

mmgr_word mmgr_scrut_ge(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_ge, ScrutLaneCtx, .w = c->w, .byte = c->byte);
}

mmgr_word mmgr_scrut_le(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_le, ScrutLaneCtx, .w = c->w, .byte = c->byte);
}

mmgr_word mmgr_scrut_sub7(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_sub7, ScrutLaneCtx, .w = c->w, .byte = c->byte);
}

mmgr_word mmgr_scrut_has_zero(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .w = c->w);
}

mmgr_word mmgr_scrut_eq(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_eq, ScrutLaneCtx, .w = c->w, .byte = c->byte, .ci = c->ci);
}

mmgr_word mmgr_scrut_xor(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_xor, ScrutLaneCtx, .w = c->w, .v = c->v, .ci = c->ci);
}

mmgr_word mmgr_scrut_fam_eq(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .w = c->w, .fam = c->fam, .byte = c->byte);
}

mmgr_word mmgr_scrut_any_upper(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_any_upper, ScrutLaneCtx, .w = c->w);
}

mmgr_word mmgr_scrut_any_digit(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_any_digit, ScrutLaneCtx, .w = c->w);
}

mmgr_word mmgr_scrut_alpha(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_alpha, ScrutLaneCtx, .w = c->w);
}

size_t mmgr_scrut_lane_count(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .m = c->m);
}

size_t mmgr_scrut_lane_lo(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_lo, ScrutLaneCtx, .m = c->m);
}

size_t mmgr_scrut_lane_hi(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_hi, ScrutLaneCtx, .m = c->m);
}

mmgr_word mmgr_scrut_spread(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_spread, ScrutMaskCtx, .m = c->m);
}

mmgr_word mmgr_scrut_drop_lo(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_drop_lo, ScrutMaskCtx, .m = c->m);
}

mmgr_word mmgr_scrut_drop_hi(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_drop_hi, ScrutMaskCtx, .m = c->m);
}

mmgr_word mmgr_scrut_bytes_below(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_bytes_below, ScrutMaskCtx, .n = c->n);
}

mmgr_word mmgr_scrut_lanes_below(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .n = c->n);
}

mmgr_word mmgr_scrut_lanes_before(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_lanes_before, ScrutMaskCtx, .m = c->m);
}

mmgr_word mmgr_scrut_tail_mask(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_tail_mask, ScrutMaskCtx, .n = c->n, .wi = c->wi);
}

mmgr_word mmgr_scrut_run(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_run, ScrutMaskCtx, .m = c->m, .n = c->n);
}

mmgr_word mmgr_scrut_run_edge(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_run_edge, ScrutMaskCtx, .n = c->n);
}

mmgr_word mmgr_scrut_load(const ScrutWordCfg *c)
{
    return MMGR_CALL(scrut_load, ScrutWordCtx, .at = c->at);
}

mmgr_word mmgr_scrut_load_al(const ScrutWordCfg *c)
{
    return MMGR_CALL(scrut_load_al, ScrutWordCtx, .at = c->at);
}

mmgr_word mmgr_scrut_fold_lower(const ScrutWordCfg *c)
{
    return MMGR_CALL(scrut_fold_lower, ScrutWordCtx, .w = c->w);
}

size_t mmgr_scrut_words(const ScrutWordCfg *c)
{
    return MMGR_CALL(scrut_words, ScrutWordCtx, .n = c->n);
}
