// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

/**
 * @file bitorum_introitus_exitus.c
 * @brief Bit writer. Bits accumulate from the low end and flush a byte at a time.
 *
 * Every entry below takes one parameter, a pointer to BitorCtx. The writer and the bits going into
 * it are one operation.
 *
 * The first bits put land in the low bits of the first output byte: acc |= bits << nbits, and the
 * flush takes acc & 0xFF.
 */

/** @brief The writer, and what is going into it. */
typedef struct
{
    mmgr_bitor_writer *w; /**< The writer. */
    uint32_t bits;        /**< The bits to put. */
    int n;                /**< How many of them. */
} BitorCtx;

/**
 * @brief Push one whole byte out of the accumulator.
 * @param c In/out. The write.
 * @return MMGR_FALSE when there was no room, which latches.
 */
MMGR_INLINE mmgr_bool bitor_flush(BitorCtx *c)
{
    mmgr_bitor_writer *w = c->w;

    if (w->cnt >= w->cap)
    {
        w->overflow = MMGR_TRUE;
        return MMGR_FALSE;
    }
    w->out[w->cnt] = (uint8_t)(w->acc & 0xFF);
    w->cnt++;
    w->acc >>= 8;
    w->nbits -= 8;
    return MMGR_TRUE;
}

/**
 * @brief Put @c n bits.
 * @param c In/out. The write.
 */
MMGR_INLINE void bitor_put(BitorCtx *c)
{
    mmgr_bitor_writer *w = c->w;

    if (w->overflow)
    {
        return;
    }

    const uint32_t low = (c->n >= 32) ? c->bits : (c->bits & ((1u << c->n) - 1u));
    w->acc |= low << w->nbits;
    w->nbits += c->n;

    while (w->nbits >= 8)
    {
        if (!bitor_flush(c))
        {
            w->nbits = 0;
            w->acc = 0;
            return;
        }
    }
}

/**
 * @brief Pad to the next byte boundary.
 * @param w In/out. The writer.
 *
 * No context. Nothing is going in, so there is no argument list to group.
 */
MMGR_INLINE void bitor_align(mmgr_bitor_writer *w)
{
    if (w->nbits > 0)
    {
        if (w->cnt >= w->cap)
        {
            w->overflow = MMGR_TRUE;
            return;
        }
        w->out[w->cnt++] = (uint8_t)(w->acc & 0xFF);
        w->acc = 0;
        w->nbits = 0;
    }
}

void mmgr_bitor_put(mmgr_bitor_writer *w, uint32_t bits, int n)
{
    MMGR_CALL(bitor_put, BitorCtx, .w = w, .bits = bits, .n = n);
}

void mmgr_bitor_align(mmgr_bitor_writer *w)
{
    bitor_align(w);
}
