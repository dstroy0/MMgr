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
 * @brief Put @c n bits, low end first, and write every byte they complete.
 * @param c In/out. The write.
 *
 * The count is settled before a byte moves. nbits and n say how many bits there will be, so how
 * many whole bytes this put produces is (nbits + n) / 8, and whether they fit is one comparison
 * against what is left of cap. Asking that per byte, which is what a flush helper did, is asking a
 * question the caller already answered.
 *
 * A put that does not fit writes nothing and latches. Writing the bytes that did fit would leave
 * the caller a buffer whose contents are half of what it asked for and no way to tell which half.
 */
MMGR_INLINE void bitor_put(BitorCtx *c)
{
    mmgr_bitor_writer *w = c->w;

    if (w->overflow)
    {
        return;
    }

    const uint32_t low = (c->n >= 32) ? c->bits : (c->bits & ((1u << c->n) - 1u));
    const int total = w->nbits + c->n;
    const size_t whole = (size_t)total / 8u;

    if (whole > (w->cap - w->cnt))
    {
        w->overflow = MMGR_TRUE;
        w->nbits = 0;
        w->acc = 0;
        return;
    }

    uint32_t acc = w->acc | (low << w->nbits);

    for (size_t i = 0; i < whole; i++)
    {
        w->out[w->cnt + i] = (uint8_t)(acc & 0xFFu);
        acc >>= 8;
    }

    w->cnt += whole;
    w->nbits = total - (int)(whole * 8u);
    w->acc = acc;
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each builds the context and hands it to the body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

void mmgr_bitor_put(mmgr_bitor_writer *w, uint32_t bits, int n)
{
    MMGR_CALL(bitor_put, BitorCtx, .w = w, .bits = bits, .n = n);
}

