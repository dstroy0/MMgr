// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/protomem/protomem.h"
#include "mmgr/swar/swar.h"

#define PROTOCORE_MEM_MASK ((uintptr_t)(PROTO_RAW_WORD - 1u))

static proto_mv_word lo_lanes(size_t nbytes)
{
    if (nbytes >= PROTO_RAW_WORD)
    {
        return (proto_mv_word) ~(proto_mv_word)0;
    }
    return (proto_mv_word)(((proto_mv_word)1 << (nbytes * 8u)) - (proto_mv_word)1);
}

static proto_mv_word span_lanes(size_t from, size_t to)
{
#if PROTOCORE_HW_BIG_ENDIAN
    return (proto_mv_word)(~lo_lanes(PROTO_RAW_WORD - to) & lo_lanes(PROTO_RAW_WORD - from));
#else
    return (proto_mv_word)(lo_lanes(to) & ~lo_lanes(from));
#endif
}

static proto_mv_word src_word(const unsigned char *p, size_t avail)
{
    const size_t off = (size_t)((uintptr_t)p & PROTOCORE_MEM_MASK);
    const unsigned char *sa = p - off;
    const proto_mv_word w0 = raw.mv_load(sa);

    if (off == 0u)
    {
        return w0;
    }

    const unsigned lo = (unsigned)(off * 8u);
    const unsigned hi = (unsigned)(PROTO_MV_BITS - lo);
    proto_mv_word w1 = 0;
    if (avail > PROTO_RAW_WORD - off)
    {
        w1 = raw.mv_load(sa + PROTO_RAW_WORD);
    }
#if PROTOCORE_HW_BIG_ENDIAN
    return (proto_mv_word)((w0 << lo) | (w1 >> hi));
#else
    return (proto_mv_word)((w0 >> lo) | (w1 << hi));
#endif
}

void protocore_mem_cpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    size_t i = 0;

    while (i + PROTO_RAW_WORD <= n)
    {
        raw.mv_put(d + i, src_word(s + i, n - i));
        i += PROTO_RAW_WORD;
    }
    if (i < n)
    {
        const proto_mv_word keep = span_lanes(0u, n - i);
        raw.mv_put(d + i, (proto_mv_word)((src_word(s + i, n - i) & keep) | (raw.mv_load(d + i) & ~keep)));
    }
}

void protocore_mem_move(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0u)
    {
        return;
    }
    if (d < s || d >= s + n)
    {
        protocore_mem_cpy(dst, src, n);
        return;
    }

    size_t i = n & ~(size_t)PROTOCORE_MEM_MASK;
    if (i < n)
    {
        const proto_mv_word keep = span_lanes(0u, n - i);
        raw.mv_put(d + i, (proto_mv_word)((src_word(s + i, n - i) & keep) | (raw.mv_load(d + i) & ~keep)));
    }
    while (i >= PROTO_RAW_WORD)
    {
        i -= PROTO_RAW_WORD;
        raw.mv_put(d + i, src_word(s + i, n - i));
    }
}

int protocore_mem_cmp(const void *a, const void *b, size_t n)
{
    const char *x = (const char *)a;
    const char *y = (const char *)b;
    size_t i = 0;

    while (i + PROTOCORE_SWAR_BYTES <= n)
    {
        protocore_swar_word d = swar.load(x + i) ^ swar.load(y + i);
        if (d != 0)
        {

            i += swar.zero_lane(PROTOCORE_SWAR_HIGH & ~swar.has_zero(d));
            return (int)(unsigned char)x[i] - (int)(unsigned char)y[i];
        }
        i += PROTOCORE_SWAR_BYTES;
    }
    while (i < n)
    {
        if (x[i] != y[i])
        {
            return (int)(unsigned char)x[i] - (int)(unsigned char)y[i];
        }
        ++i;
    }
    return 0;
}

const void *protocore_mem_chr(const void *p, size_t n, uint8_t c)
{
    const char *s = (const char *)p;
    size_t i = 0;

    while (i < n && ((uintptr_t)(s + i) & (PROTOCORE_SWAR_BYTES - 1u)) != 0u)
    {
        if ((uint8_t)s[i] == c)
        {
            return s + i;
        }
        ++i;
    }
    while (i + PROTOCORE_SWAR_BYTES <= n)
    {
        protocore_swar_word m = swar.eq(swar.load_al(s + i), c, PROTO_FALSE);
        if (m != 0)
        {
            return s + i + swar.zero_lane(m);
        }
        i += PROTOCORE_SWAR_BYTES;
    }

    while (i < n)
    {
        if ((uint8_t)s[i] == c)
        {
            return s + i;
        }
        ++i;
    }
    return NULL;
}

void protocore_mem_set(void *dst, unsigned char v, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    size_t i = 0;

    const proto_mv_word ones = (proto_mv_word)((proto_mv_word) ~(proto_mv_word)0 / 0xFFu);
    const proto_mv_word w = (proto_mv_word)(ones * (proto_mv_word)v);

    while (i + PROTO_RAW_WORD <= n)
    {
        raw.mv_put(d + i, w);
        i += PROTO_RAW_WORD;
    }
    if (i < n)
    {
        const proto_mv_word keep = span_lanes(0u, n - i);
        raw.mv_put(d + i, (proto_mv_word)((w & keep) | (raw.mv_load(d + i) & ~keep)));
    }
}

void protocore_mem_zero(void *dst, size_t n)
{
    protocore_mem_set(dst, 0u, n);
}
