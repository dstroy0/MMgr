#include "verbum_scrutor/verbum_scrutor.h"

typedef struct
{
    mmgr_word word;
    mmgr_word val;
    mmgr_word mask;
    uint8_t byte;
    uint8_t fam;
    mmgr_bool ci;
} ScrutLaneCtx;

typedef struct
{
    mmgr_word mask;
    size_t bytes;
    size_t wi;
} ScrutMaskCtx;

typedef struct
{
    mmgr_word word;
    const void *at;
    size_t bytes;
} ScrutWordCtx;

MMGR_INLINE mmgr_word scrut_below_lo(const ScrutMaskCtx *c)
{
    return (c->mask - 1u) & ~c->mask & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_smear(const ScrutMaskCtx *c)
{
    mmgr_word m = c->mask;

    for (uint32_t k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        m |= (m >> k);
    }
    return m;
}

MMGR_INLINE mmgr_word scrut_ge(const ScrutLaneCtx *c)
{
    return ((c->word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_le(const ScrutLaneCtx *c)
{
    return ((MMGR_SWAR_ONES * c->byte | MMGR_VERBUM_SCRUTOR_HIGH) - c->word) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_sub7(const ScrutLaneCtx *c)
{
    return ((c->word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_SWAR_LOW7;
}

MMGR_INLINE mmgr_word scrut_has_zero(const ScrutLaneCtx *c)
{
    return ~(((c->word & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | c->word) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_alpha(const ScrutLaneCtx *c)
{
    const mmgr_word lo = c->word | (MMGR_SWAR_ONES * 0x20u);

    return MMGR_CALL(scrut_ge, ScrutLaneCtx, .word = lo, .byte = 'a') &
           MMGR_CALL(scrut_le, ScrutLaneCtx, .word = lo, .byte = 'z') & ~lo;
}

MMGR_INLINE mmgr_word scrut_xor(const ScrutLaneCtx *c)
{
    const mmgr_word x = c->word ^ c->val;

    if (!c->ci)
    {
        return x;
    }
    return x & ~(MMGR_CALL(scrut_alpha, ScrutLaneCtx, .word = c->word) >> 2);
}

MMGR_INLINE mmgr_word scrut_eq(const ScrutLaneCtx *c)
{
    const mmgr_word broadcast = MMGR_SWAR_ONES * c->byte;
    const mmgr_word x = MMGR_CALL(scrut_xor, ScrutLaneCtx, .word = c->word, .val = broadcast, .ci = c->ci);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .word = x);
}

MMGR_INLINE mmgr_word scrut_fam_eq(const ScrutLaneCtx *c)
{
    const mmgr_word bits = MMGR_SWAR_ONES * c->fam;
    const mmgr_word want = MMGR_SWAR_ONES * (c->byte & c->fam);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .word = (c->word & bits) ^ want);
}

MMGR_INLINE mmgr_word scrut_any_upper(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .word = c->word, .fam = MMGR_FAM_CS, .byte = MMGR_FAM_CI);
}

MMGR_INLINE mmgr_word scrut_any_digit(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .word = c->word, .fam = 0xF0u, .byte = 0x30u);
}

MMGR_INLINE size_t scrut_lane_count(const ScrutLaneCtx *c)
{
    return ((c->mask >> 7) * MMGR_SWAR_ONES) >> (MMGR_SWAR_LANE_BITS - 8u);
}

MMGR_INLINE size_t scrut_lane_lo(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .mask = MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .mask = c->mask));
}

MMGR_INLINE size_t scrut_lane_hi(const ScrutLaneCtx *c)
{
    if (c->mask == 0u)
    {
        return MMGR_SWAR_BYTES;
    }
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .mask = MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask)) - 1u;
}

MMGR_INLINE mmgr_word scrut_spread(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->mask + (c->mask - (c->mask >> 7)));
}

MMGR_INLINE mmgr_word scrut_drop_lo(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->mask & (c->mask - 1u));
}

MMGR_INLINE mmgr_word scrut_drop_hi(const ScrutMaskCtx *c)
{
    const mmgr_word s = MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask);

    return c->mask & ~(s ^ (s >> 8));
}

MMGR_INLINE mmgr_word scrut_bytes_below(const ScrutMaskCtx *c)
{
    const mmgr_word all = (mmgr_word) ~(mmgr_word)0;

    if (c->bytes == 0u)
    {
        return 0;
    }
    if (c->bytes >= MMGR_SWAR_BYTES)
    {
        return all;
    }
#if MMGR_HW_BIG_ENDIAN
    return all << ((MMGR_SWAR_BYTES - c->bytes) * 8u);
#else
    return all >> ((MMGR_SWAR_BYTES - c->bytes) * 8u);
#endif
}

MMGR_INLINE mmgr_word scrut_lanes_below(const ScrutMaskCtx *c)
{
    return MMGR_CALL(scrut_bytes_below, ScrutMaskCtx, .bytes = c->bytes) & MMGR_VERBUM_SCRUTOR_HIGH;
}

MMGR_INLINE mmgr_word scrut_tail_mask(const ScrutMaskCtx *c)
{
    const size_t done = c->wi * MMGR_SWAR_BYTES;

    if (done >= c->bytes)
    {
        return 0;
    }
    return MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .bytes = c->bytes - done);
}

MMGR_INLINE mmgr_word scrut_lanes_before(const ScrutMaskCtx *c)
{
#if MMGR_HW_BIG_ENDIAN
    return ~MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask) & MMGR_VERBUM_SCRUTOR_HIGH;
#else
    return MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .mask = c->mask);
#endif
}

MMGR_INLINE mmgr_word scrut_run(const ScrutMaskCtx *c)
{
    mmgr_word m = c->mask;
    size_t have = 1u;

    if (c->bytes > MMGR_SWAR_BYTES)
    {
        return 0;
    }
    while (have < c->bytes)
    {
        const size_t step = (have < c->bytes - have) ? have : c->bytes - have;
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
    if ((c->bytes <= 1u) || (c->bytes > MMGR_SWAR_BYTES))
    {
        return 0;
    }
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .bytes = MMGR_SWAR_BYTES - c->bytes + 1u);
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
    return c->word | (MMGR_CALL(scrut_alpha, ScrutLaneCtx, .word = c->word) >> 2);
}

MMGR_INLINE size_t scrut_words(const ScrutWordCtx *c)
{
    return (c->bytes / MMGR_SWAR_BYTES) + (((c->bytes & (MMGR_SWAR_BYTES - 1u)) != 0u) ? 1u : 0u);
}

mmgr_word mmgr_scrut_ge(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_ge, ScrutLaneCtx, .word = c->word, .byte = c->byte);
}

mmgr_word mmgr_scrut_le(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_le, ScrutLaneCtx, .word = c->word, .byte = c->byte);
}

mmgr_word mmgr_scrut_sub7(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_sub7, ScrutLaneCtx, .word = c->word, .byte = c->byte);
}

mmgr_word mmgr_scrut_has_zero(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .word = c->word);
}

mmgr_word mmgr_scrut_eq(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_eq, ScrutLaneCtx, .word = c->word, .byte = c->byte, .ci = c->ci);
}

mmgr_word mmgr_scrut_xor(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_xor, ScrutLaneCtx, .word = c->word, .val = c->val, .ci = c->ci);
}

mmgr_word mmgr_scrut_fam_eq(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .word = c->word, .fam = c->fam, .byte = c->byte);
}

mmgr_word mmgr_scrut_any_upper(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_any_upper, ScrutLaneCtx, .word = c->word);
}

mmgr_word mmgr_scrut_any_digit(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_any_digit, ScrutLaneCtx, .word = c->word);
}

mmgr_word mmgr_scrut_alpha(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_alpha, ScrutLaneCtx, .word = c->word);
}

size_t mmgr_scrut_lane_count(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .mask = c->mask);
}

size_t mmgr_scrut_lane_lo(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_lo, ScrutLaneCtx, .mask = c->mask);
}

size_t mmgr_scrut_lane_hi(const ScrutLaneCfg *c)
{
    return MMGR_CALL(scrut_lane_hi, ScrutLaneCtx, .mask = c->mask);
}

mmgr_word mmgr_scrut_spread(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_spread, ScrutMaskCtx, .mask = c->mask);
}

mmgr_word mmgr_scrut_drop_lo(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_drop_lo, ScrutMaskCtx, .mask = c->mask);
}

mmgr_word mmgr_scrut_drop_hi(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_drop_hi, ScrutMaskCtx, .mask = c->mask);
}

mmgr_word mmgr_scrut_bytes_below(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_bytes_below, ScrutMaskCtx, .bytes = c->bytes);
}

mmgr_word mmgr_scrut_lanes_below(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .bytes = c->bytes);
}

mmgr_word mmgr_scrut_lanes_before(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_lanes_before, ScrutMaskCtx, .mask = c->mask);
}

mmgr_word mmgr_scrut_tail_mask(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_tail_mask, ScrutMaskCtx, .bytes = c->bytes, .wi = c->wi);
}

mmgr_word mmgr_scrut_run(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_run, ScrutMaskCtx, .mask = c->mask, .bytes = c->bytes);
}

mmgr_word mmgr_scrut_run_edge(const ScrutMaskCfg *c)
{
    return MMGR_CALL(scrut_run_edge, ScrutMaskCtx, .bytes = c->bytes);
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
    return MMGR_CALL(scrut_fold_lower, ScrutWordCtx, .word = c->word);
}

size_t mmgr_scrut_words(const ScrutWordCfg *c)
{
    return MMGR_CALL(scrut_words, ScrutWordCtx, .bytes = c->bytes);
}
