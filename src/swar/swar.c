// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/swar/swar.h"

protocore_swar_word protocore_swar_ge(protocore_swar_word a, protocore_swar_word v)
{
    return ((a | PROTOCORE_SWAR_HIGH) - v * PROTOCORE_SWAR_ONES) & PROTOCORE_SWAR_HIGH;
}

protocore_swar_word protocore_swar_le(protocore_swar_word a, protocore_swar_word v)
{
    return ((v * PROTOCORE_SWAR_ONES | PROTOCORE_SWAR_HIGH) - a) & PROTOCORE_SWAR_HIGH;
}

protocore_swar_word protocore_swar_spread(protocore_swar_word m)
{
    return m + (m - (m >> 7));
}

protocore_swar_word protocore_swar_sub7(protocore_swar_word a, protocore_swar_word lo)
{
    return ((a | PROTOCORE_SWAR_HIGH) - lo * PROTOCORE_SWAR_ONES) & PROTOCORE_SWAR_LOW7;
}

protocore_swar_word protocore_swar_has_zero(protocore_swar_word w)
{
    return ~(((w & PROTOCORE_SWAR_LOW7) + PROTOCORE_SWAR_LOW7) | w) & PROTOCORE_SWAR_HIGH;
}

protocore_swar_word protocore_swar_eq(protocore_swar_word w, uint8_t c)
{
    return protocore_swar_has_zero(w ^ (PROTOCORE_SWAR_ONES * (protocore_swar_word)c));
}

size_t protocore_swar_zero_lane(protocore_swar_word m)
{
#if PROTOCORE_HW_BIG_ENDIAN

    return (size_t)((PROTOCORE_SWAR_CLZ(m) - (PROTOCORE_SWAR_CLZ_WIDTH - PROTO_SWAR_BITS)) >> 3);
#else
    return (size_t)(PROTOCORE_SWAR_CTZ(m) >> 3);
#endif
}

protocore_swar_word protocore_swar_load(const char *p)
{
    return (protocore_swar_word)proto_raw_load(p, PROTOCORE_SWAR_BYTES);
}

protocore_swar_word protocore_swar_load_al(const char *p)
{
    return (protocore_swar_word)proto_al_load(p, PROTOCORE_SWAR_BYTES);
}

protocore_swar_word protocore_swar_xor(protocore_swar_word wa, protocore_swar_word wb)
{
    return wa ^ wb;
}

protocore_swar_word protocore_swar_xor_ci(protocore_swar_word wa, protocore_swar_word wb)
{
    protocore_swar_word x = wa ^ wb;
    protocore_swar_word lo = wa | (PROTOCORE_SWAR_ONES * 0x20u);
    protocore_swar_word alpha = protocore_swar_ge(lo, 'a') & protocore_swar_le(lo, 'z') & ~lo;
    return x & ~(alpha >> 2);
}

protocore_swar_word protocore_swar_eq_ci(protocore_swar_word w, uint8_t c)
{
    return protocore_swar_has_zero(protocore_swar_xor_ci(w, PROTOCORE_SWAR_ONES * (protocore_swar_word)c));
}

protocore_swar_word protocore_swar_eq_sel(protocore_swar_word w, uint8_t c, proto_bool ci)
{
    if (ci)
    {
        return protocore_swar_eq_ci(w, c);
    }
    return protocore_swar_eq(w, c);
}

protocore_swar_word protocore_swar_xor_sel(protocore_swar_word wa, protocore_swar_word wb, proto_bool ci)
{
    if (ci)
    {
        return protocore_swar_xor_ci(wa, wb);
    }
    return protocore_swar_xor(wa, wb);
}
