// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/protostr/protostr.h"
#include "mmgr/swar/swar.h"

static inline size_t len(const char *s, size_t nul_cap)
{

    size_t i = 0;
    while (i < nul_cap && ((uintptr_t)(s + i) & (PROTOCORE_SWAR_BYTES - 1u)) != 0u)
    {
        if (s[i] == '\0')
        {
            return i;
        }
        ++i;
    }
    while (i + PROTOCORE_SWAR_BYTES <= nul_cap)
    {
        protocore_swar_word m = swar.has_zero(swar.load_al(s + i));
        if (m != 0)
        {
            return i + swar.zero_lane(m);
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    while (i < nul_cap && s[i] != '\0')
    {
        ++i;
    }
    return i;
}

PROTOCORE_INLINE int byte_same_ci(uint8_t a, uint8_t b)
{
    if (a == b)
    {
        return 1;
    }
    uint8_t la = (uint8_t)(a | 0x20u);
    uint8_t lb = (uint8_t)(b | 0x20u);
    if (la != lb)
    {
        return 0;
    }
    if (la < 'a')
    {
        return 0;
    }
    if (la > 'z')
    {
        return 0;
    }
    return 1;
}

static inline size_t diff_cs(const char *a, const char *b, size_t read_cap)
{
    size_t i = 0;
    while (i + PROTOCORE_SWAR_BYTES <= read_cap)
    {
        protocore_swar_word d = swar.load(a + i) ^ swar.load(b + i);
        if (d != 0)
        {

            return i + swar.zero_lane(PROTOCORE_SWAR_HIGH & ~swar.has_zero(d));
        }
        i += PROTOCORE_SWAR_BYTES;
    }
    while (i < read_cap && a[i] == b[i])
    {
        ++i;
    }
    return i;
}

static inline size_t diff_ci(const char *a, const char *b, size_t read_cap)
{
    size_t i = 0;
    while (i + PROTOCORE_SWAR_BYTES <= read_cap)
    {
        protocore_swar_word d = swar.xor_(swar.load(a + i), swar.load(b + i), PROTO_TRUE);
        if (d != 0)
        {
            return i + swar.zero_lane(PROTOCORE_SWAR_HIGH & ~swar.has_zero(d));
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    while (i < read_cap && swar.xor_((protocore_swar_word)(unsigned char)a[i], (protocore_swar_word)(unsigned char)b[i],
                                     PROTO_TRUE) == 0)
    {
        ++i;
    }
    return i;
}

PROTOCORE_INLINE int step_word_cs(protocore_swar_word wa, protocore_swar_word wb, int end_wins)
{
    protocore_swar_word x = wa ^ wb;
    protocore_swar_word z = swar.has_zero(wa);
    if ((x | z) == 0)
    {
        return PROTOCORE_SWAR_GO;
    }
    size_t dl = PROTOCORE_SWAR_BYTES;
    if (x != 0)
    {
        dl = swar.zero_lane(PROTOCORE_SWAR_HIGH & ~swar.has_zero(x));
    }
    size_t el = PROTOCORE_SWAR_BYTES;
    if (z != 0)
    {
        el = swar.zero_lane(z);
    }
    if (end_wins)
    {
        if (el <= dl)
        {
            return PROTOCORE_SWAR_YES;
        }
        return PROTOCORE_SWAR_NO;
    }
    if (el < dl)
    {
        return PROTOCORE_SWAR_YES;
    }
    return PROTOCORE_SWAR_NO;
}

PROTOCORE_INLINE int step_word_ci(protocore_swar_word wa, protocore_swar_word wb, int end_wins)
{
    protocore_swar_word x = swar.xor_(wa, wb, PROTO_TRUE);
    protocore_swar_word z = swar.has_zero(wa);
    if ((x | z) == 0)
    {
        return PROTOCORE_SWAR_GO;
    }
    size_t dl = PROTOCORE_SWAR_BYTES;
    if (x != 0)
    {
        dl = swar.zero_lane(PROTOCORE_SWAR_HIGH & ~swar.has_zero(x));
    }
    size_t el = PROTOCORE_SWAR_BYTES;
    if (z != 0)
    {
        el = swar.zero_lane(z);
    }
    if (end_wins)
    {
        if (el <= dl)
        {
            return PROTOCORE_SWAR_YES;
        }
        return PROTOCORE_SWAR_NO;
    }
    if (el < dl)
    {
        return PROTOCORE_SWAR_YES;
    }
    return PROTOCORE_SWAR_NO;
}

PROTOCORE_INLINE int step_byte_cs(unsigned char ca, unsigned char cb, int end_wins)
{
    if (ca == 0)
    {
        if (ca == cb)
        {
            return PROTOCORE_SWAR_YES;
        }
        if (end_wins != 0)
        {
            return PROTOCORE_SWAR_YES;
        }
        return PROTOCORE_SWAR_NO;
    }
    if (ca != cb)
    {
        return PROTOCORE_SWAR_NO;
    }
    return PROTOCORE_SWAR_GO;
}

PROTOCORE_INLINE int step_byte_ci(unsigned char ca, unsigned char cb, int end_wins)
{
    protocore_swar_word d = swar.xor_((protocore_swar_word)ca, (protocore_swar_word)cb, PROTO_TRUE);
    if (ca == 0)
    {
        if (d == 0)
        {
            return PROTOCORE_SWAR_YES;
        }
        if (end_wins != 0)
        {
            return PROTOCORE_SWAR_YES;
        }
        return PROTOCORE_SWAR_NO;
    }
    if (d != 0)
    {
        return PROTOCORE_SWAR_NO;
    }
    return PROTOCORE_SWAR_GO;
}

static inline proto_bool agree_cs(const char *a, const char *b, size_t read_cap, int end_wins)
{

    size_t i = 0;
    while (i < read_cap)
    {
        if (((uintptr_t)(a + i) & (PROTOCORE_SWAR_BYTES - 1u)) == 0u && i + PROTOCORE_SWAR_BYTES <= read_cap)
        {
            protocore_swar_word wa = swar.load_al(a + i);
            protocore_swar_word wb = swar.load(b + i);
            protocore_swar_word x = wa ^ wb;
            protocore_swar_word z = swar.has_zero(wa);
            if ((x | z) != 0)
            {

                protocore_swar_word xm = PROTOCORE_SWAR_HIGH & ~swar.has_zero(x);
                protocore_swar_word zl = (z - (protocore_swar_word)1) & ~z;
                protocore_swar_word xl = (xm - (protocore_swar_word)1) & ~xm;
#if PROTOCORE_HW_BIG_ENDIAN

                if (end_wins)
                {
                    return zl >= xl;
                }
                return zl > xl;
#else
                if (end_wins)
                {
                    return zl <= xl;
                }
                return zl < xl;
#endif
            }
            i += PROTOCORE_SWAR_BYTES;
            continue;
        }
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca == 0)
        {
            return (ca == cb) || (end_wins != 0);
        }
        if (ca != cb)
        {
            return PROTO_FALSE;
        }
        ++i;
    }

    return end_wins != 0;
}

static inline proto_bool agree_ci(const char *a, const char *b, size_t read_cap, int end_wins)
{
    size_t i = 0;
    while (i < read_cap)
    {
        if (((uintptr_t)(a + i) & (PROTOCORE_SWAR_BYTES - 1u)) == 0u && i + PROTOCORE_SWAR_BYTES <= read_cap)
        {
            protocore_swar_word wa = swar.load_al(a + i);
            protocore_swar_word wb = swar.load(b + i);
            protocore_swar_word x = swar.xor_(wa, wb, PROTO_TRUE);
            protocore_swar_word z = swar.has_zero(wa);
            if ((x | z) != 0)
            {
                protocore_swar_word xm = PROTOCORE_SWAR_HIGH & ~swar.has_zero(x);
                protocore_swar_word zl = (z - (protocore_swar_word)1) & ~z;
                protocore_swar_word xl = (xm - (protocore_swar_word)1) & ~xm;
#if PROTOCORE_HW_BIG_ENDIAN
                if (end_wins)
                {
                    return zl >= xl;
                }
                return zl > xl;
#else
                if (end_wins)
                {
                    return zl <= xl;
                }
                return zl < xl;
#endif
            }
            i += PROTOCORE_SWAR_BYTES;
            continue;
        }
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        protocore_swar_word d = swar.xor_((protocore_swar_word)ca, (protocore_swar_word)cb, PROTO_TRUE);
        if (ca == 0)
        {
            return (d == 0) || (end_wins != 0);
        }
        if (d != 0)
        {
            return PROTO_FALSE;
        }
        ++i;
    }
    return end_wins != 0;
}

static inline proto_bool eq_cs(const char *a, const char *b, size_t read_cap)
{
    return agree_cs(a, b, read_cap, 0);
}
static inline proto_bool eq_ci(const char *a, const char *b, size_t read_cap)
{
    return agree_ci(a, b, read_cap, 0);
}
static inline proto_bool starts_cs(const char *s, const char *pre, size_t read_cap)
{
    return agree_cs(pre, s, read_cap, 1);
}
static inline proto_bool starts_ci(const char *s, const char *pre, size_t read_cap)
{
    return agree_ci(pre, s, read_cap, 1);
}

PROTOCORE_INLINE size_t lane_of(protocore_swar_word m, int rev)
{
#if PROTOCORE_HW_BIG_ENDIAN
    if (rev)
    {
        return (size_t)((PROTO_SWAR_BITS - 1u - (unsigned)PROTOCORE_SWAR_CTZ(m)) >> 3);
    }
#else
    if (rev)
    {
        return (size_t)((PROTOCORE_SWAR_CLZ_WIDTH - 1u - (unsigned)PROTOCORE_SWAR_CLZ(m)) >> 3);
    }
#endif
    return swar.zero_lane(m);
}

PROTOCORE_INLINE protocore_swar_word drop_lane(protocore_swar_word m, int rev)
{

#if PROTOCORE_HW_BIG_ENDIAN
    const int clear_low = (rev != 0);
#else
    const int clear_low = (rev == 0);
#endif
    if (clear_low)
    {
        return (protocore_swar_word)(m & (m - (protocore_swar_word)1));
    }
    return (protocore_swar_word)(m & ~((protocore_swar_word)1
                                       << (PROTOCORE_SWAR_CLZ_WIDTH - 1u - (unsigned)PROTOCORE_SWAR_CLZ(m))));
}

static const char *find_cs(const char *hay, size_t read_cap, const char *needle, size_t needle_cap)
{

    size_t w = 1u;
    if (needle_cap >= PROTOCORE_SWAR_BYTES)
    {
        w = PROTOCORE_SWAR_BYTES;
    }
    else if (needle_cap >= 4u)
    {
        w = 4u;
    }
    else if (needle_cap >= 2u)
    {
        w = 2u;
    }

    const protocore_swar_word n_raw = (protocore_swar_word)raw.load(needle, w);
    const protocore_swar_word nz = swar.has_zero(n_raw);

    size_t j0 = PROTOCORE_SWAR_BYTES;
    if (nz != 0)
    {
        j0 = swar.zero_lane(nz);
    }
    size_t nlen = j0;
    if (j0 >= w)
    {
        if (needle_cap - w == 1u)
        {
            nlen = w;
        }
        else
        {
            nlen = len(needle, needle_cap);
        }
    }
    if (nlen == 0)
    {
        return hay;
    }

    size_t take = w;
    if (nlen < w)
    {
        take = nlen;
    }

    const protocore_swar_word all = (protocore_swar_word) ~(protocore_swar_word)0;
#if PROTOCORE_HW_BIG_ENDIAN
    const protocore_swar_word nm =
        (protocore_swar_word)((all >> (PROTOCORE_SWAR_BYTES * 8u - take * 8u)) << ((w - take) * 8u));
#else
    const protocore_swar_word nm = (protocore_swar_word)(all >> (PROTOCORE_SWAR_BYTES * 8u - take * 8u));
#endif
    const protocore_swar_word nw = n_raw & nm;

    const uint8_t c_first = (uint8_t)needle[0];

    size_t ka = PROTOCORE_SWAR_BYTES - 1u;
    if ((nlen / 2u) < PROTOCORE_SWAR_BYTES)
    {
        ka = nlen / 2u;
    }
    const uint8_t c_anchor = (uint8_t)needle[ka];

    size_t i = 0;

    while (i < read_cap && ((uintptr_t)(hay + i) & (PROTOCORE_SWAR_BYTES - 1u)) != 0u)
    {
        if (hay[i] == '\0')
        {
            return NULL;
        }
        if ((uint8_t)hay[i] == c_first)
        {
            size_t j = 0;
            while (j < nlen && i + j < read_cap && hay[i + j] == needle[j])
            {
                ++j;
            }
            if (j == nlen)
            {
                return hay + i;
            }
        }
        ++i;
    }

    if (nlen == 1u)
    {
        while (i + PROTOCORE_SWAR_BYTES <= read_cap)
        {
            protocore_swar_word w0 = swar.load_al(hay + i);
            protocore_swar_word z = swar.has_zero(w0);
            protocore_swar_word m = swar.eq(w0, c_first, PROTO_FALSE);
            if ((m | z) != 0)
            {

                size_t km = PROTOCORE_SWAR_BYTES;
                if (m != 0)
                {
                    km = swar.zero_lane(m);
                }
                size_t kz = PROTOCORE_SWAR_BYTES;
                if (z != 0)
                {
                    kz = swar.zero_lane(z);
                }
                if (km < kz)
                {
                    return hay + i + km;
                }
                return NULL;
            }
            i += PROTOCORE_SWAR_BYTES;
        }
    }

    while (nlen >= 2u && nlen <= 3u && nlen <= PROTOCORE_SWAR_BYTES && i + (2u * PROTOCORE_SWAR_BYTES) <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word w1 = swar.load_al(hay + i + PROTOCORE_SWAR_BYTES);
        protocore_swar_word m = swar.eq(w0, c_first, PROTO_FALSE);
        for (size_t k = 1u; k < nlen; ++k)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            protocore_swar_word fk = (protocore_swar_word)((w0 << (8u * k)) | (w1 >> (PROTO_SWAR_BITS - 8u * k)));
#else
            protocore_swar_word fk = (protocore_swar_word)((w0 >> (8u * k)) | (w1 << (PROTO_SWAR_BITS - 8u * k)));
#endif
            m &= swar.eq(fk, (uint8_t)needle[k], PROTO_FALSE);
        }
        protocore_swar_word z = swar.has_zero(w0);
#if PROTOCORE_HW_BIG_ENDIAN
        size_t zend = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            zend = swar.zero_lane(z);
        }
        if (zend != PROTOCORE_SWAR_BYTES)
        {
            m &= (protocore_swar_word)(all << (PROTO_SWAR_BITS - 8u * zend));
        }
#else
        m &= (protocore_swar_word)((z - 1u) & ~z);
#endif
        if (m != 0)
        {
            return hay + i + swar.zero_lane(m);
        }
        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    while ((nlen > 3u || nlen > PROTOCORE_SWAR_BYTES) && nlen >= 2u && i + (2u * PROTOCORE_SWAR_BYTES) <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word w1 = swar.load_al(hay + i + PROTOCORE_SWAR_BYTES);
        protocore_swar_word z = swar.has_zero(w0);

        protocore_swar_word wa = w0;
        if (ka != 0u)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            wa = (protocore_swar_word)((w0 << (8u * ka)) | (w1 >> (PROTO_SWAR_BITS - 8u * ka)));
#else
            wa = (protocore_swar_word)((w0 >> (8u * ka)) | (w1 << (PROTO_SWAR_BITS - 8u * ka)));
#endif
        }
        protocore_swar_word m = swar.eq(wa, c_anchor, PROTO_FALSE);
        size_t end = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            end = swar.zero_lane(z);
        }

        while (m != 0)
        {
            size_t k = swar.zero_lane(m);
            if (k >= end)
            {
                break;
            }

            protocore_swar_word wk = w0;
            if (k != 0)
            {
#if PROTOCORE_HW_BIG_ENDIAN
                wk = (protocore_swar_word)((w0 << (8u * k)) | (w1 >> (PROTO_SWAR_BITS - 8u * k)));
#else
                wk = (protocore_swar_word)((w0 >> (8u * k)) | (w1 << (PROTO_SWAR_BITS - 8u * k)));
#endif
            }
            protocore_swar_word syn = (protocore_swar_word)(wk ^ nw);
            size_t rest = nlen - take;
            if ((syn & nm) == 0 && (take == nlen || (i + k + nlen <= read_cap &&
                                                     diff_cs(hay + i + k + take, needle + take, rest) == rest)))
            {
                return hay + i + k;
            }
            m = drop_lane(m, 0);
        }

        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    if (nlen <= PROTOCORE_SWAR_BYTES && i + PROTOCORE_SWAR_BYTES <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word z = swar.has_zero(w0);
        protocore_swar_word m = swar.eq(w0, c_first, PROTO_FALSE);
        for (size_t k = 1u; k < nlen; ++k)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            protocore_swar_word fk = (protocore_swar_word)(w0 << (8u * k));
#else
            protocore_swar_word fk = (protocore_swar_word)(w0 >> (8u * k));
#endif
            m &= swar.eq(fk, (uint8_t)needle[k], PROTO_FALSE);
        }
#if PROTOCORE_HW_BIG_ENDIAN
        size_t zend = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            zend = swar.zero_lane(z);
        }
        if (zend != PROTOCORE_SWAR_BYTES)
        {
            m &= (protocore_swar_word)(all << (PROTO_SWAR_BITS - 8u * zend));
        }
#else
        m &= (protocore_swar_word)((z - 1u) & ~z);
#endif
        if (m != 0)
        {
            return hay + i + swar.zero_lane(m);
        }
        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES - nlen + 1u;
    }

    while (i < read_cap && hay[i] != '\0')
    {
        if ((uint8_t)hay[i] == c_first)
        {
            size_t j = 1u;
            while (j < nlen && i + j < read_cap && hay[i + j] == needle[j])
            {
                ++j;
            }
            if (j == nlen)
            {
                return hay + i;
            }
        }
        ++i;
    }
    return NULL;
}

static const char *find_ci(const char *hay, size_t read_cap, const char *needle, size_t needle_cap)
{
    size_t w = 1u;
    if (needle_cap >= PROTOCORE_SWAR_BYTES)
    {
        w = PROTOCORE_SWAR_BYTES;
    }
    else if (needle_cap >= 4u)
    {
        w = 4u;
    }
    else if (needle_cap >= 2u)
    {
        w = 2u;
    }

    const protocore_swar_word n_raw = (protocore_swar_word)raw.load(needle, w);
    const protocore_swar_word nz = swar.has_zero(n_raw);
    size_t j0 = PROTOCORE_SWAR_BYTES;
    if (nz != 0)
    {
        j0 = swar.zero_lane(nz);
    }
    size_t nlen = j0;
    if (j0 >= w)
    {
        if (needle_cap - w == 1u)
        {
            nlen = w;
        }
        else
        {
            nlen = len(needle, needle_cap);
        }
    }
    if (nlen == 0)
    {
        return hay;
    }

    size_t take = w;
    if (nlen < w)
    {
        take = nlen;
    }

    const protocore_swar_word all = (protocore_swar_word) ~(protocore_swar_word)0;
#if PROTOCORE_HW_BIG_ENDIAN
    const protocore_swar_word nm =
        (protocore_swar_word)((all >> (PROTOCORE_SWAR_BYTES * 8u - take * 8u)) << ((w - take) * 8u));
#else
    const protocore_swar_word nm = (protocore_swar_word)(all >> (PROTOCORE_SWAR_BYTES * 8u - take * 8u));
#endif
    const protocore_swar_word nw = n_raw & nm;

    const uint8_t c_first = (uint8_t)needle[0];
    size_t ka = PROTOCORE_SWAR_BYTES - 1u;
    if ((nlen / 2u) < PROTOCORE_SWAR_BYTES)
    {
        ka = nlen / 2u;
    }
    const uint8_t c_anchor = (uint8_t)needle[ka];

    size_t i = 0;

    while (i < read_cap && ((uintptr_t)(hay + i) & (PROTOCORE_SWAR_BYTES - 1u)) != 0u)
    {
        if (hay[i] == '\0')
        {
            return NULL;
        }
        if (byte_same_ci((uint8_t)hay[i], c_first))
        {
            size_t j = 0;
            while (j < nlen && i + j < read_cap && byte_same_ci((uint8_t)hay[i + j], (uint8_t)needle[j]))
            {
                ++j;
            }
            if (j == nlen)
            {
                return hay + i;
            }
        }
        ++i;
    }

    if (nlen == 1u)
    {
        while (i + PROTOCORE_SWAR_BYTES <= read_cap)
        {
            protocore_swar_word w0 = swar.load_al(hay + i);
            protocore_swar_word z = swar.has_zero(w0);
            protocore_swar_word m = swar.eq(w0, c_first, PROTO_TRUE);
            if ((m | z) != 0)
            {
                size_t km = PROTOCORE_SWAR_BYTES;
                if (m != 0)
                {
                    km = swar.zero_lane(m);
                }
                size_t kz = PROTOCORE_SWAR_BYTES;
                if (z != 0)
                {
                    kz = swar.zero_lane(z);
                }
                if (km < kz)
                {
                    return hay + i + km;
                }
                return NULL;
            }
            i += PROTOCORE_SWAR_BYTES;
        }
    }

    while (nlen >= 2u && nlen <= 3u && nlen <= PROTOCORE_SWAR_BYTES && i + (2u * PROTOCORE_SWAR_BYTES) <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word w1 = swar.load_al(hay + i + PROTOCORE_SWAR_BYTES);
        protocore_swar_word m = swar.eq(w0, c_first, PROTO_TRUE);
        for (size_t k = 1u; k < nlen; ++k)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            protocore_swar_word fk = (protocore_swar_word)((w0 << (8u * k)) | (w1 >> (PROTO_SWAR_BITS - 8u * k)));
#else
            protocore_swar_word fk = (protocore_swar_word)((w0 >> (8u * k)) | (w1 << (PROTO_SWAR_BITS - 8u * k)));
#endif
            m &= swar.eq(fk, (uint8_t)needle[k], PROTO_TRUE);
        }
        protocore_swar_word z = swar.has_zero(w0);
#if PROTOCORE_HW_BIG_ENDIAN
        size_t zend = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            zend = swar.zero_lane(z);
        }
        if (zend != PROTOCORE_SWAR_BYTES)
        {
            m &= (protocore_swar_word)(all << (PROTO_SWAR_BITS - 8u * zend));
        }
#else
        m &= (protocore_swar_word)((z - 1u) & ~z);
#endif
        if (m != 0)
        {
            return hay + i + swar.zero_lane(m);
        }
        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    while ((nlen > 3u || nlen > PROTOCORE_SWAR_BYTES) && nlen >= 2u && i + (2u * PROTOCORE_SWAR_BYTES) <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word w1 = swar.load_al(hay + i + PROTOCORE_SWAR_BYTES);
        protocore_swar_word z = swar.has_zero(w0);
        protocore_swar_word wa = w0;
        if (ka != 0u)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            wa = (protocore_swar_word)((w0 << (8u * ka)) | (w1 >> (PROTO_SWAR_BITS - 8u * ka)));
#else
            wa = (protocore_swar_word)((w0 >> (8u * ka)) | (w1 << (PROTO_SWAR_BITS - 8u * ka)));
#endif
        }
        protocore_swar_word m = swar.eq(wa, c_anchor, PROTO_TRUE);
        size_t end = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            end = swar.zero_lane(z);
        }

        while (m != 0)
        {
            size_t k = swar.zero_lane(m);
            if (k >= end)
            {
                break;
            }
            protocore_swar_word wk = w0;
            if (k != 0)
            {
#if PROTOCORE_HW_BIG_ENDIAN
                wk = (protocore_swar_word)((w0 << (8u * k)) | (w1 >> (PROTO_SWAR_BITS - 8u * k)));
#else
                wk = (protocore_swar_word)((w0 >> (8u * k)) | (w1 << (PROTO_SWAR_BITS - 8u * k)));
#endif
            }
            protocore_swar_word syn = swar.xor_(wk, nw, PROTO_TRUE);
            size_t rest = nlen - take;
            if ((syn & nm) == 0 && (take == nlen || (i + k + nlen <= read_cap &&
                                                     diff_ci(hay + i + k + take, needle + take, rest) == rest)))
            {
                return hay + i + k;
            }
            m = drop_lane(m, 0);
        }

        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    if (nlen <= PROTOCORE_SWAR_BYTES && i + PROTOCORE_SWAR_BYTES <= read_cap)
    {
        protocore_swar_word w0 = swar.load_al(hay + i);
        protocore_swar_word z = swar.has_zero(w0);
        protocore_swar_word m = swar.eq(w0, c_first, PROTO_TRUE);
        for (size_t k = 1u; k < nlen; ++k)
        {
#if PROTOCORE_HW_BIG_ENDIAN
            protocore_swar_word fk = (protocore_swar_word)(w0 << (8u * k));
#else
            protocore_swar_word fk = (protocore_swar_word)(w0 >> (8u * k));
#endif
            m &= swar.eq(fk, (uint8_t)needle[k], PROTO_TRUE);
        }
#if PROTOCORE_HW_BIG_ENDIAN
        size_t zend = PROTOCORE_SWAR_BYTES;
        if (z != 0)
        {
            zend = swar.zero_lane(z);
        }
        if (zend != PROTOCORE_SWAR_BYTES)
        {
            m &= (protocore_swar_word)(all << (PROTO_SWAR_BITS - 8u * zend));
        }
#else
        m &= (protocore_swar_word)((z - 1u) & ~z);
#endif
        if (m != 0)
        {
            return hay + i + swar.zero_lane(m);
        }
        if (z != 0)
        {
            return NULL;
        }
        i += PROTOCORE_SWAR_BYTES - nlen + 1u;
    }

    while (i < read_cap && hay[i] != '\0')
    {
        if (byte_same_ci((uint8_t)hay[i], c_first))
        {
            size_t j = 1u;
            while (j < nlen && i + j < read_cap && byte_same_ci((uint8_t)hay[i + j], (uint8_t)needle[j]))
            {
                ++j;
            }
            if (j == nlen)
            {
                return hay + i;
            }
        }
        ++i;
    }
    return NULL;
}

static inline size_t copy(char *dst, const char *src, size_t dst_cap)
{
    if (dst_cap == 0)
    {
        return 0;
    }

    size_t n = len(src, dst_cap - 1);
    raw.read(dst, src, n);
    dst[n] = '\0';
    return n;
}

static size_t diff(const char *a, const char *b, size_t read_cap, proto_bool ci)
{
    if (ci)
    {
        return diff_ci(a, b, read_cap);
    }
    return diff_cs(a, b, read_cap);
}

static proto_bool eq(const char *a, const char *b, size_t read_cap, proto_bool ci)
{
    if (ci)
    {
        return eq_ci(a, b, read_cap);
    }
    return eq_cs(a, b, read_cap);
}

static proto_bool starts(const char *s, const char *pre, size_t read_cap, proto_bool ci)
{
    if (ci)
    {
        return starts_ci(s, pre, read_cap);
    }
    return starts_cs(s, pre, read_cap);
}

static const char *find(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, proto_bool ci)
{
    if (ci)
    {
        return find_ci(hay, read_cap, needle, needle_cap);
    }
    return find_cs(hay, read_cap, needle, needle_cap);
}

static proto_bool has(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, proto_bool ci)
{
    if (ci)
    {
        return find_ci(hay, read_cap, needle, needle_cap) != NULL;
    }
    return find_cs(hay, read_cap, needle, needle_cap) != NULL;
}

static int step_word(protocore_swar_word wa, protocore_swar_word wb, proto_bool ci, int end_wins)
{
    if (ci)
    {
        return step_word_ci(wa, wb, end_wins);
    }
    return step_word_cs(wa, wb, end_wins);
}

static int step_byte(unsigned char ca, unsigned char cb, proto_bool ci, int end_wins)
{
    if (ci)
    {
        return step_byte_ci(ca, cb, end_wins);
    }
    return step_byte_cs(ca, cb, end_wins);
}

static proto_bool ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static proto_bool digit(char c)
{
    return c >= '0' && c <= '9';
}

static long to_long(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    proto_bool neg = PROTO_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p++ == '-');
    }
    const char *ds = p;
    unsigned long v = 0;
    while (digit(*p))
    {
        v = v * 10UL + (unsigned long)(*p++ - '0');
    }
    if (end)
    {
        *end = s;
        if (p != ds)
        {
            *end = p;
        }
    }
    if (neg)
    {
        return (long)(0UL - v);
    }
    return (long)v;
}

static unsigned long to_ulong(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }
    const char *ds = p;
    unsigned long v = 0;
    while (digit(*p))
    {
        v = v * 10UL + (unsigned long)(*p++ - '0');
    }
    if (end)
    {
        *end = s;
        if (p != ds)
        {
            *end = p;
        }
    }
    return v;
}

static void frac(const char **p, double *val, proto_bool *any)
{
    (*p)++;
    double scale = 1.0;
    while (digit(**p))
    {
        scale *= 10.0;
        *val += (double)(*(*p)++ - '0') / scale;
        *any = PROTO_TRUE;
    }
}

static void expo(const char **p, double *val)
{
    (*p)++;
    proto_bool eneg = PROTO_FALSE;
    if (**p == '+' || **p == '-')
    {
        eneg = (*(*p)++ == '-');
    }
    int ex = 0;
    while (digit(**p))
    {
        if (ex < 400)
        {
            ex = ex * 10 + (**p - '0');
        }
        (*p)++;
    }
    double m = 1.0;
    for (int k = 0; k < ex; k++)
    {
        m *= 10.0;
    }
    if (eneg)
    {
        *val = *val / m;
        return;
    }
    *val = *val * m;
}

static double to_double(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    proto_bool neg = PROTO_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p++ == '-');
    }
    proto_bool any = PROTO_FALSE;
    double val = 0.0;
    while (digit(*p))
    {
        val = val * 10.0 + (*p++ - '0');
        any = PROTO_TRUE;
    }
    if (*p == '.')
    {
        frac(&p, &val, &any);
    }
    if (any && (*p == 'e' || *p == 'E'))
    {
        expo(&p, &val);
    }
    if (end)
    {
        *end = s;
        if (any)
        {
            *end = p;
        }
    }
    if (neg)
    {
        return -val;
    }
    return val;
}

static float to_float(const char *s, const char **end)
{
    return (float)to_double(s, end);
}

const StrNs str = {len,       diff, eq,    starts,  find,     has,       copy,    step_word,
                   step_byte, ws,   digit, to_long, to_ulong, to_double, to_float};
