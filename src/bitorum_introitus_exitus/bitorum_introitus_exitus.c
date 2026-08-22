// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

/**
 * @file bitorum_introitus_exitus.c
 * @brief Bit writer. Bits accumulate from the low end and flush a byte at a time.
 *
 * The entry below takes one parameter, a pointer to the BitorumCfg the caller built and carries from
 * one put to the next. The writer and the bits going into it are one operation, and BitorCtx is that.
 *
 * The first bits put land in the low bits of the first output byte: acc |= bits << nbits, and the
 * flush takes acc & 0xFF.
 */

/**
 * @brief One put, with the writer's state in it rather than behind a pointer.
 *
 * File local and staying that way. BitorumCfg in the header is what the caller hands over; this is
 * what the body works with. The state arrives by value so the arithmetic runs in registers, and
 * the entry writes back what changed - out and cap are not among them.
 */
typedef struct
{
    uint8_t *const out; /**< The buffer. */
    const size_t cap;   /**< Its size. */
    size_t cnt;         /**< Whole bytes written. */
    uint32_t acc;       /**< The bits, low end first. */
    int nbits;          /**< How many of them. */
    mmgr_bool overflow; /**< The buffer filled. Latches. */
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
    if (c->overflow)
    {
        return;
    }

    const size_t whole = (size_t)c->nbits / 8u;

    if (whole > (c->cap - c->cnt))
    {
        c->overflow = MMGR_TRUE;
        c->nbits = 0;
        c->acc = 0;
        return;
    }

    uint32_t acc = c->acc;

    for (size_t i = 0; i < whole; i++)
    {
        c->out[c->cnt + i] = (uint8_t)(acc & 0xFFu);
        acc >>= 8;
    }

    c->cnt += whole;
    c->nbits -= (int)(whole * 8u);
    c->acc = acc;
}

/* The namespace is a table of function pointers with the caller's config in their types, so this is
   what it points at. It builds the context and hands it to the body above.

   Parenthesised because the header defines a macro of this name for the call site, and the
   identifier here would otherwise sit immediately before a parenthesis and be taken for an
   invocation of it.

   It is nameable rather than file local because a static const table in the header has to be able
   to point at it, and a static const table is what gcc devirtualizes. Through an extern one every
   call from another translation unit is a load of the table, a load of the entry, and an indirect
   call it cannot see through. */

void (mmgr_bitor_put)(BitorumCfg *c)
{
    BitorCtx x = {.out = c->out,
                  .cap = c->cap,
                  .cnt = c->cnt,
                  .acc = c->acc,
                  .nbits = c->nbits,
                  .overflow = c->overflow};

    bitor_put(&x);

    c->cnt = x.cnt;
    c->acc = x.acc;
    c->nbits = x.nbits;
    c->overflow = x.overflow;
}

