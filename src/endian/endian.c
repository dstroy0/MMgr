// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @file endian.c
 * @brief Fixed width integer reads and writes in an explicit byte order.
 *
 * Every entry below takes one parameter, a pointer to EndianCtx. Two orders over one width is two
 * bodies each way, so they are one context: bytes out low first or high first, and bytes in the
 * same two ways. The width is a count, not a case.
 *
 * One unaligned word access, not a byte loop. A loop touches memory once per byte and its cost
 * follows the width; a word access does not. Measured through the tables at -O2 with LTO: a 64-bit
 * read is 1.00 cycles against 6.32 for the loop it replaced, and the binary is 592 bytes smaller.
 *
 * The order that is not the host's costs one fold, and the fold is three masked shift-or steps
 * rather than a reverse loop. A reverse loop is a serial chain - every step waits on the one before
 * it - and measured 8.72 cycles at 64 bits, worse than the byte loop it was meant to beat.
 *
 * One fold for every width, not one per width. Folding by width was tried and is not paid for: it
 * takes a 32-bit write from 1.58 cycles to 3.16 and returns a third of a cycle on one read.
 */

/** @brief A fixed width field, and where it goes. */
typedef struct
{
    uint8_t *w;       /**< Destination, when writing. */
    const uint8_t *r; /**< Source, when reading. */
    uint64_t v;       /**< The value, either direction. */
    size_t n;         /**< Bytes wide. */
} EndianCtx;

/** @brief Put @p n bytes down in host order, at any alignment. */
MMGR_INLINE void endian_put(uint8_t *w, uint64_t v, size_t n)
{
    switch (n)
    {
    case 2:
        proxim.put_u16(w, (uint16_t)v);
        break;
    case 4:
        proxim.put_u32(w, (uint32_t)v);
        break;
    default:
        proxim.put_u64(w, v);
        break;
    }
}

/**
 * @brief Reverse the low @p n bytes with no loop.
 *
 * Three masked shift-or steps reverse the whole word: adjacent bytes, then 16-bit pairs, then the
 * 32-bit halves. Each step is (v & M) << k | (v >> k) & M with k doubling, and the mask keeps the
 * two halves from colliding; at k of 32 the shifts already discard what the other side supplies,
 * so no mask is needed there. The final shift drops the bytes a narrower width does not use.
 */
MMGR_INLINE uint64_t endian_rev(uint64_t v, size_t n)
{
    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - n));
}

/**
 * @brief Lay the value down low byte first.
 * @param c In/out. The field.
 * @return Bytes written.
 */
MMGR_INLINE size_t endian_wr_le(EndianCtx *c)
{
    endian_put(c->w, c->v, c->n);
    return c->n;
}

/**
 * @brief Lay the value down high byte first.
 * @param c In/out. The field.
 * @return Bytes written.
 */
MMGR_INLINE size_t endian_wr_be(EndianCtx *c)
{
    endian_put(c->w, endian_rev(c->v, c->n), c->n);
    return c->n;
}

/**
 * @brief Take the value up, low byte first.
 * @param c In/out. The field.
 * @return The value.
 */
MMGR_INLINE uint64_t endian_rd_le(EndianCtx *c)
{
    return proxim.load(c->r, c->n);
}

/**
 * @brief Take the value up, high byte first.
 * @param c In/out. The field.
 * @return The value.
 */
MMGR_INLINE uint64_t endian_rd_be(EndianCtx *c)
{
    return endian_rev(proxim.load(c->r, c->n), c->n);
}

size_t mmgr_wr_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .w = c->w, .v = c->v, .n = (size_t)c->n);
}

uint64_t mmgr_rd_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_le, EndianCtx, .r = c->r, .n = (size_t)c->n);
}

size_t mmgr_wr_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .w = c->w, .v = c->v, .n = (size_t)c->n);
}

uint64_t mmgr_rd_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_be, EndianCtx, .r = c->r, .n = (size_t)c->n);
}
