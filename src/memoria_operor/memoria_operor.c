// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memoria_operor/memoria_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @file memoria_operor.c
 * @brief Bulk memory work, a word at a time.
 *
 * Every entry below takes one parameter, a pointer to MemorCtx. Two regions, a count and a cursor
 * are what all six entries work on, so they are one context.
 *
 * cpy, move and set write whole words and mask the tail. cmp and chr count words and mask the last
 * one. Neither shape has an alignment peel or a byte remainder.
 */

#define MMGR_MEM_MASK ((uintptr_t)(MMGR_RAW_WORD - 1u))

/** @brief Two regions, a count, and where the work has reached. */
typedef struct
{
    unsigned char *d;       /**< Destination, when writing. */
    const unsigned char *s; /**< Source, when reading. */
    const unsigned char *b; /**< The other source, when comparing two. */
    size_t n;               /**< Byte count. */
    size_t i;               /**< How far along. */
    unsigned char v;        /**< The byte, for set and chr. */
    mmgr_migro_word word;   /**< The word being put down. */

    /* the lane mask being built */
    size_t from; /**< First lane kept. */
    size_t to;   /**< One past the last lane kept. */
} MemorCtx;

/**
 * @brief Byte mask keeping the low @c to lanes.
 * @param c The work. @c to is the lane count; at or above the word size keeps everything.
 * @return Byte mask.
 */
MMGR_INLINE mmgr_migro_word memor_lo_lanes(const MemorCtx *c)
{
    /* GCOVR_EXCL_START - only memor_span_lanes calls this, and only its big endian arm asks for a
       whole word: on little endian the count is always the ragged tail, which is one to word size
       minus one. There is no big endian environment in MMGR_ENVIRONMENTS to run it on. */
    if (c->to >= MMGR_RAW_WORD)
    {
        return (mmgr_migro_word) ~(mmgr_migro_word)0;
    }
    /* GCOVR_EXCL_STOP */
    return (mmgr_migro_word)(((mmgr_migro_word)1 << (c->to * 8u)) - (mmgr_migro_word)1);
}

/**
 * @brief Byte mask keeping lanes @c from through @c to.
 * @param c In/out. The work.
 * @return Byte mask, in address order on either byte order.
 */
MMGR_INLINE mmgr_migro_word memor_span_lanes(MemorCtx *c)
{
    const size_t from = c->from;
    const size_t to = c->to;

#if MMGR_HW_BIG_ENDIAN
    c->to = MMGR_RAW_WORD - to;
    const mmgr_migro_word hi = memor_lo_lanes(c);
    c->to = MMGR_RAW_WORD - from;
    const mmgr_migro_word lo = memor_lo_lanes(c);
    c->to = to;
    return (mmgr_migro_word)(~hi & lo);
#else
    const mmgr_migro_word hi = memor_lo_lanes(c);
    c->to = from;
    const mmgr_migro_word lo = memor_lo_lanes(c);
    c->to = to;
    return (mmgr_migro_word)(hi & ~lo);
#endif
}

/**
 * @brief The tail mask for what is left after the last whole word.
 * @param c In/out. The work.
 * @return Byte mask.
 */
MMGR_INLINE mmgr_migro_word memor_tail_keep(MemorCtx *c)
{
    c->from = 0u;
    c->to = c->n - c->i;
    return memor_span_lanes(c);
}

/**
 * @brief One word of source at @c i, assembled across an alignment boundary.
 * @param c The work.
 * @return The word.
 *
 * The second load is skipped when what remains cannot reach into it.
 */
MMGR_INLINE mmgr_migro_word memor_src_word(const MemorCtx *c)
{
    const unsigned char *p = c->s + c->i;
    const size_t avail = c->n - c->i;
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
 * @brief Write one word at @c i, keeping whatever is outside the tail.
 * @param c In/out. The work. @c word is what goes down.
 */
MMGR_INLINE void memor_put_tail(MemorCtx *c)
{
    const mmgr_migro_word keep = memor_tail_keep(c);

    proxim.mv_put(c->d + c->i, (mmgr_migro_word)((c->word & keep) | (proxim.mv_load(c->d + c->i) & ~keep)));
}

/**
 * @brief Copy, forward. Regions must not overlap.
 * @param c In/out. The work.
 */
MMGR_INLINE void memor_cpy(MemorCtx *c)
{
    while ((c->i + MMGR_RAW_WORD) <= c->n)
    {
        proxim.mv_put(c->d + c->i, memor_src_word(c));
        c->i += MMGR_RAW_WORD;
    }
    if (c->i < c->n)
    {
        c->word = memor_src_word(c);
        memor_put_tail(c);
    }
}

/**
 * @brief Copy, either direction. Regions may overlap.
 * @param c In/out. The work.
 *
 * Forward when the regions do not overlap or the destination is below the source, backward
 * otherwise.
 */
MMGR_INLINE void memor_move(MemorCtx *c)
{
    if ((c->d == c->s) || (c->n == 0u))
    {
        return;
    }
    if ((c->d < c->s) || (c->d >= (c->s + c->n)))
    {
        memor_cpy(c);
        return;
    }

    c->i = c->n & ~(size_t)MMGR_MEM_MASK;
    if (c->i < c->n)
    {
        c->word = memor_src_word(c);
        memor_put_tail(c);
    }
    while (c->i >= MMGR_RAW_WORD)
    {
        c->i -= MMGR_RAW_WORD;
        proxim.mv_put(c->d + c->i, memor_src_word(c));
    }
}

/**
 * @brief Compare.
 * @param c In/out. The work.
 * @return Difference of the first bytes that differ, or 0.
 *
 * has_zero of the xor marks the lanes that agree, so its complement over the lane bits is where
 * they differ. The tail mask keeps that to the lanes inside the count.
 */
MMGR_INLINE int memor_cmp(MemorCtx *c)
{
    const char *x = (const char *)c->s;
    const char *y = (const char *)c->b;
    const size_t nw = mmgr_scrut_words(c->n);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word d = scrut.load(x + at) ^ scrut.load(y + at);
        const mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(c->n, wi));
        if (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            return (int)(unsigned char)x[k] - (int)(unsigned char)y[k];
        }
    }
    return 0;
}

/**
 * @brief Find @c v.
 * @param c In/out. The work.
 * @return Pointer to it, or NULL.
 */
MMGR_INLINE const void *memor_chr(MemorCtx *c)
{
    const char *s = (const char *)c->s;
    const size_t nw = mmgr_scrut_words(c->n);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word m =
            (mmgr_scrut_word)(scrut.eq(scrut.load(s + at), c->v, MMGR_FALSE) & mmgr_scrut_tail_mask(c->n, wi));
        if (m != 0)
        {
            return s + at + scrut.zero_lane(m);
        }
    }
    return NULL;
}

/**
 * @brief Fill with @c v.
 * @param c In/out. The work.
 */
MMGR_INLINE void memor_set(MemorCtx *c)
{
    const mmgr_migro_word ones = (mmgr_migro_word)((mmgr_migro_word) ~(mmgr_migro_word)0 / 0xFFu);
    const mmgr_migro_word w = (mmgr_migro_word)(ones * (mmgr_migro_word)c->v);

    while ((c->i + MMGR_RAW_WORD) <= c->n)
    {
        proxim.mv_put(c->d + c->i, w);
        c->i += MMGR_RAW_WORD;
    }
    if (c->i < c->n)
    {
        c->word = w;
        memor_put_tail(c);
    }
}

void mmgr_memor_cpy(void *dst, const void *src, size_t n)
{
    MMGR_CALL(memor_cpy, MemorCtx, .d = (unsigned char *)dst, .s = (const unsigned char *)src, .n = n);
}

void mmgr_memor_move(void *dst, const void *src, size_t n)
{
    MMGR_CALL(memor_move, MemorCtx, .d = (unsigned char *)dst, .s = (const unsigned char *)src, .n = n);
}

int mmgr_memor_cmp(const void *a, const void *b, size_t n)
{
    return MMGR_CALL(memor_cmp, MemorCtx, .s = (const unsigned char *)a, .b = (const unsigned char *)b, .n = n);
}

const void *mmgr_memor_chr(const void *p, size_t n, uint8_t c)
{
    return MMGR_CALL(memor_chr, MemorCtx, .s = (const unsigned char *)p, .n = n, .v = c);
}

void mmgr_memor_set(void *dst, unsigned char v, size_t n)
{
    MMGR_CALL(memor_set, MemorCtx, .d = (unsigned char *)dst, .n = n, .v = v);
}

void mmgr_memor_zero(void *dst, size_t n)
{
    MMGR_CALL(memor_set, MemorCtx, .d = (unsigned char *)dst, .n = n, .v = 0u);
}
