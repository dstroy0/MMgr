// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "verbum_scrutor/verbum_scrutor.h"

mmgr_scrut_word mmgr_scrut_ge(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - v * MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH;
}

mmgr_scrut_word mmgr_scrut_le(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((v * MMGR_SWAR_ONES | MMGR_VERBUM_SCRUTOR_HIGH) - a) & MMGR_VERBUM_SCRUTOR_HIGH;
}

mmgr_scrut_word mmgr_scrut_spread(mmgr_scrut_word m)
{
    // The cast is not decoration. When the lane is 16 bits the word is narrower than int, so C
    // promotes every operand here to int, does the arithmetic in 32 bits, and then narrows on
    // return - which -Wconversion reports, correctly, as a conversion that may change the value.
    //
    // SWAR arithmetic is modular on purpose: the carries that leave the top of the lane are exactly
    // the ones being discarded. Saying so with a cast keeps the intent explicit and keeps the
    // 16-bit build warning-clean, without changing what any lane width computes.
    return (mmgr_scrut_word)(m + (m - (m >> 7)));
}

mmgr_scrut_word mmgr_scrut_sub7(mmgr_scrut_word a, mmgr_scrut_word lo)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - lo * MMGR_SWAR_ONES) & MMGR_SWAR_LOW7;
}

mmgr_scrut_word mmgr_scrut_has_zero(mmgr_scrut_word w)
{
    return ~(((w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | w) & MMGR_VERBUM_SCRUTOR_HIGH;
}

mmgr_scrut_word mmgr_scrut_eq(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(w ^ (MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

size_t mmgr_scrut_zero_lane(mmgr_scrut_word m)
{
#if MMGR_HW_BIG_ENDIAN

    return (size_t)((MMGR_SWAR_CLZ(m) - (MMGR_SWAR_CLZ_WIDTH - MMGR_SWAR_BITS)) >> 3);
#else
    return (size_t)(MMGR_SWAR_CTZ(m) >> 3);
#endif
}

mmgr_scrut_word mmgr_scrut_load(const char *p)
{
    return (mmgr_scrut_word)mmgr_proxim_load(p, MMGR_SWAR_BYTES);
}

mmgr_scrut_word mmgr_scrut_load_al(const char *p)
{
    return (mmgr_scrut_word)mmgr_aequus_load(p, MMGR_SWAR_BYTES);
}

mmgr_scrut_word mmgr_scrut_xor(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    return wa ^ wb;
}

mmgr_scrut_word mmgr_scrut_xor_ci(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    mmgr_scrut_word x = wa ^ wb;
    mmgr_scrut_word lo = wa | (MMGR_SWAR_ONES * 0x20u);
    mmgr_scrut_word alpha = mmgr_scrut_ge(lo, 'a') & mmgr_scrut_le(lo, 'z') & ~lo;
    return x & ~(alpha >> 2);
}

mmgr_scrut_word mmgr_scrut_eq_ci(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(mmgr_scrut_xor_ci(w, MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

mmgr_scrut_word mmgr_scrut_eq_sel(mmgr_scrut_word w, uint8_t c, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_eq_ci(w, c);
    }
    return mmgr_scrut_eq(w, c);
}

mmgr_scrut_word mmgr_scrut_xor_sel(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_xor_ci(wa, wb);
    }
    return mmgr_scrut_xor(wa, wb);
}
