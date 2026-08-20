// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memoria_operor/memoria_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @file memoria_operor.c
 * @brief Bulk memory work, a word at a time.
 *
 * cpy, move and set write whole words and mask the tail. cmp and chr count words and mask the last
 * one. Neither shape has an alignment peel or a byte remainder.
 */

#define MMGR_MEM_MASK ((uintptr_t)(MMGR_RAW_WORD - 1u))

/**
 * @brief Byte mask keeping the low @p nbytes lanes.
 * @param nbytes Lane count. At or above the word size keeps everything.
 * @return Byte mask.
 */
static mmgr_migro_word lo_lanes(size_t nbytes)
{
    if (nbytes >= MMGR_RAW_WORD)
    {
        return (mmgr_migro_word) ~(mmgr_migro_word)0;
    }
    return (mmgr_migro_word)(((mmgr_migro_word)1 << (nbytes * 8u)) - (mmgr_migro_word)1);
}

/**
 * @brief Byte mask keeping lanes @p from through @p to.
 * @param from First lane kept.
 * @param to One past the last lane kept.
 * @return Byte mask, in address order on either byte order.
 */
static mmgr_migro_word span_lanes(size_t from, size_t to)
{
#if MMGR_HW_BIG_ENDIAN
    return (mmgr_migro_word)(~lo_lanes(MMGR_RAW_WORD - to) & lo_lanes(MMGR_RAW_WORD - from));
#else
    return (mmgr_migro_word)(lo_lanes(to) & ~lo_lanes(from));
#endif
}

/**
 * @brief One word of source, assembled across an alignment boundary.
 * @param p Source, any alignment.
 * @param avail How many bytes remain, so the second load is skipped when it is not needed.
 * @return The word.
 */
static mmgr_migro_word src_word(const unsigned char *p, size_t avail)
{
    const size_t off = (size_t)((uintptr_t)p & MMGR_MEM_MASK);
    const unsigned char *sa = p - off;
    const mmgr_migro_word w0 = proxim.mv_load(sa);

    if (off == 0u)
    {
        return w0;
    }

    const unsigned lo = (unsigned)(off * 8u);
    const unsigned hi = (unsigned)(MMGR_MV_BITS - lo);
    mmgr_migro_word w1 = 0;
    if (avail > MMGR_RAW_WORD - off)
    {
        w1 = proxim.mv_load(sa + MMGR_RAW_WORD);
    }
#if MMGR_HW_BIG_ENDIAN
    return (mmgr_migro_word)((w0 << lo) | (w1 >> hi));
#else
    return (mmgr_migro_word)((w0 >> lo) | (w1 << hi));
#endif
}

/**
 * @brief Copy @p n bytes. Regions must not overlap.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 *
 * Whole words, then one masked read-modify-write for the tail. The tail never writes outside @p n.
 */
void mmgr_memor_cpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    size_t i = 0;

    while (i + MMGR_RAW_WORD <= n)
    {
        proxim.mv_put(d + i, src_word(s + i, n - i));
        i += MMGR_RAW_WORD;
    }
    if (i < n)
    {
        const mmgr_migro_word keep = span_lanes(0u, n - i);
        proxim.mv_put(d + i, (mmgr_migro_word)((src_word(s + i, n - i) & keep) | (proxim.mv_load(d + i) & ~keep)));
    }
}

/**
 * @brief Copy @p n bytes. Regions may overlap.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 *
 * Forward when the regions do not overlap or dst is below src, backward otherwise.
 */
void mmgr_memor_move(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0u)
    {
        return;
    }
    if (d < s || d >= s + n)
    {
        mmgr_memor_cpy(dst, src, n);
        return;
    }

    size_t i = n & ~(size_t)MMGR_MEM_MASK;
    if (i < n)
    {
        const mmgr_migro_word keep = span_lanes(0u, n - i);
        proxim.mv_put(d + i, (mmgr_migro_word)((src_word(s + i, n - i) & keep) | (proxim.mv_load(d + i) & ~keep)));
    }
    while (i >= MMGR_RAW_WORD)
    {
        i -= MMGR_RAW_WORD;
        proxim.mv_put(d + i, src_word(s + i, n - i));
    }
}

/**
 * @brief Compare @p n bytes.
 * @param a First region.
 * @param b Second region.
 * @param n Byte count.
 * @return Difference of the first bytes that differ, or 0.
 *
 * has_zero of the xor marks the lanes that agree, so its complement over the lane bits is where
 * they differ. The tail mask keeps that to the lanes inside @p n.
 */
int mmgr_memor_cmp(const void *a, const void *b, size_t n)
{
    const char *x = (const char *)a;
    const char *y = (const char *)b;
    const size_t nw = mmgr_scrut_words(n);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word d = scrut.load(x + at) ^ scrut.load(y + at);
        mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(n, wi));
        if (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            return (int)(unsigned char)x[k] - (int)(unsigned char)y[k];
        }
    }
    return 0;
}

/**
 * @brief Find @p c in the first @p n bytes.
 * @param p Region.
 * @param n Byte count.
 * @param c Byte to find.
 * @return Pointer to it, or NULL.
 */
const void *mmgr_memor_chr(const void *p, size_t n, uint8_t c)
{
    const char *s = (const char *)p;
    const size_t nw = mmgr_scrut_words(n);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word m =
            (mmgr_scrut_word)(scrut.eq(scrut.load(s + at), c, MMGR_FALSE) & mmgr_scrut_tail_mask(n, wi));
        if (m != 0)
        {
            return s + at + scrut.zero_lane(m);
        }
    }
    return NULL;
}

/**
 * @brief Fill @p n bytes with @p v.
 * @param dst Destination.
 * @param v Byte to write.
 * @param n Byte count.
 */
void mmgr_memor_set(void *dst, unsigned char v, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    size_t i = 0;

    const mmgr_migro_word ones = (mmgr_migro_word)((mmgr_migro_word) ~(mmgr_migro_word)0 / 0xFFu);
    const mmgr_migro_word w = (mmgr_migro_word)(ones * (mmgr_migro_word)v);

    while (i + MMGR_RAW_WORD <= n)
    {
        proxim.mv_put(d + i, w);
        i += MMGR_RAW_WORD;
    }
    if (i < n)
    {
        const mmgr_migro_word keep = span_lanes(0u, n - i);
        proxim.mv_put(d + i, (mmgr_migro_word)((w & keep) | (proxim.mv_load(d + i) & ~keep)));
    }
}

/**
 * @brief Zero @p n bytes.
 * @param dst Destination.
 * @param n Byte count.
 */
void mmgr_memor_zero(void *dst, size_t n)
{
    mmgr_memor_set(dst, 0u, n);
}
