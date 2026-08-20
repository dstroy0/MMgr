// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/swar/swar.h"

mmgr_swar_word mmgr_swar_ge(mmgr_swar_word a, mmgr_swar_word v)
{
    return ((a | MMGR_SWAR_HIGH) - v * MMGR_SWAR_ONES) & MMGR_SWAR_HIGH;
}

mmgr_swar_word mmgr_swar_le(mmgr_swar_word a, mmgr_swar_word v)
{
    return ((v * MMGR_SWAR_ONES | MMGR_SWAR_HIGH) - a) & MMGR_SWAR_HIGH;
}

mmgr_swar_word mmgr_swar_spread(mmgr_swar_word m)
{
    return m + (m - (m >> 7));
}

mmgr_swar_word mmgr_swar_sub7(mmgr_swar_word a, mmgr_swar_word lo)
{
    return ((a | MMGR_SWAR_HIGH) - lo * MMGR_SWAR_ONES) & MMGR_SWAR_LOW7;
}

mmgr_swar_word mmgr_swar_has_zero(mmgr_swar_word w)
{
    return ~(((w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | w) & MMGR_SWAR_HIGH;
}

mmgr_swar_word mmgr_swar_eq(mmgr_swar_word w, uint8_t c)
{
    return mmgr_swar_has_zero(w ^ (MMGR_SWAR_ONES * (mmgr_swar_word)c));
}

size_t mmgr_swar_zero_lane(mmgr_swar_word m)
{
#if MMGR_HW_BIG_ENDIAN

    return (size_t)((MMGR_SWAR_CLZ(m) - (MMGR_SWAR_CLZ_WIDTH - MMGR_SWAR_BITS)) >> 3);
#else
    return (size_t)(MMGR_SWAR_CTZ(m) >> 3);
#endif
}

mmgr_swar_word mmgr_swar_load(const char *p)
{
    return (mmgr_swar_word)mmgr_raw_load(p, MMGR_SWAR_BYTES);
}

mmgr_swar_word mmgr_swar_load_al(const char *p)
{
    return (mmgr_swar_word)mmgr_al_load(p, MMGR_SWAR_BYTES);
}

mmgr_swar_word mmgr_swar_xor(mmgr_swar_word wa, mmgr_swar_word wb)
{
    return wa ^ wb;
}

mmgr_swar_word mmgr_swar_xor_ci(mmgr_swar_word wa, mmgr_swar_word wb)
{
    mmgr_swar_word x = wa ^ wb;
    mmgr_swar_word lo = wa | (MMGR_SWAR_ONES * 0x20u);
    mmgr_swar_word alpha = mmgr_swar_ge(lo, 'a') & mmgr_swar_le(lo, 'z') & ~lo;
    return x & ~(alpha >> 2);
}

mmgr_swar_word mmgr_swar_eq_ci(mmgr_swar_word w, uint8_t c)
{
    return mmgr_swar_has_zero(mmgr_swar_xor_ci(w, MMGR_SWAR_ONES * (mmgr_swar_word)c));
}

mmgr_swar_word mmgr_swar_eq_sel(mmgr_swar_word w, uint8_t c, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_swar_eq_ci(w, c);
    }
    return mmgr_swar_eq(w, c);
}

mmgr_swar_word mmgr_swar_xor_sel(mmgr_swar_word wa, mmgr_swar_word wb, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_swar_xor_ci(wa, wb);
    }
    return mmgr_swar_xor(wa, wb);
}
