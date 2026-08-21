// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "endian/endian.h"

/**
 * @file endian.c
 * @brief Fixed width integer reads and writes in an explicit byte order.
 *
 * Every entry below takes one parameter, a pointer to EndianCtx. Twelve entries are one operation
 * with three widths and two orders, so they are one context and two bodies: bytes out low first or
 * high first, and bytes in the same two ways. The width is a count, not a case.
 */

/** @brief A fixed width field, and where it goes. */
typedef struct
{
    uint8_t *w;       /**< Destination, when writing. */
    const uint8_t *r; /**< Source, when reading. */
    uint64_t v;       /**< The value, either direction. */
    size_t n;         /**< Bytes wide. */
} EndianCtx;

/**
 * @brief Lay the value down low byte first.
 * @param c In/out. The field.
 * @return Bytes written.
 */
MMGR_INLINE size_t endian_wr_le(EndianCtx *c)
{
    for (size_t i = 0; i < c->n; i++)
    {
        c->w[i] = (uint8_t)(c->v >> (8u * i));
    }
    return c->n;
}

/**
 * @brief Lay the value down high byte first.
 * @param c In/out. The field.
 * @return Bytes written.
 */
MMGR_INLINE size_t endian_wr_be(EndianCtx *c)
{
    for (size_t i = 0; i < c->n; i++)
    {
        c->w[i] = (uint8_t)(c->v >> (8u * (c->n - 1u - i)));
    }
    return c->n;
}

/**
 * @brief Take the value up, low byte first.
 * @param c In/out. The field.
 * @return The value.
 */
MMGR_INLINE uint64_t endian_rd_le(EndianCtx *c)
{
    uint64_t v = 0;

    for (size_t i = 0; i < c->n; i++)
    {
        v |= (uint64_t)c->r[i] << (8u * i);
    }
    return v;
}

/**
 * @brief Take the value up, high byte first.
 * @param c In/out. The field.
 * @return The value.
 */
MMGR_INLINE uint64_t endian_rd_be(EndianCtx *c)
{
    uint64_t v = 0;

    for (size_t i = 0; i < c->n; i++)
    {
        v = (v << 8) | c->r[i];
    }
    return v;
}

size_t mmgr_wr16le(uint8_t *p, uint16_t v)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .w = p, .v = v, .n = 2u);
}

size_t mmgr_wr32le(uint8_t *p, uint32_t v)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .w = p, .v = v, .n = 4u);
}

size_t mmgr_wr64le(uint8_t *p, uint64_t v)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .w = p, .v = v, .n = 8u);
}

uint16_t mmgr_rd16le(const uint8_t *p)
{
    return (uint16_t)MMGR_CALL(endian_rd_le, EndianCtx, .r = p, .n = 2u);
}

uint32_t mmgr_rd32le(const uint8_t *p)
{
    return (uint32_t)MMGR_CALL(endian_rd_le, EndianCtx, .r = p, .n = 4u);
}

uint64_t mmgr_rd64le(const uint8_t *p)
{
    return MMGR_CALL(endian_rd_le, EndianCtx, .r = p, .n = 8u);
}

size_t mmgr_wr16be(uint8_t *p, uint16_t v)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .w = p, .v = v, .n = 2u);
}

size_t mmgr_wr32be(uint8_t *p, uint32_t v)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .w = p, .v = v, .n = 4u);
}

size_t mmgr_wr64be(uint8_t *p, uint64_t v)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .w = p, .v = v, .n = 8u);
}

uint16_t mmgr_rd16be(const uint8_t *p)
{
    return (uint16_t)MMGR_CALL(endian_rd_be, EndianCtx, .r = p, .n = 2u);
}

uint32_t mmgr_rd32be(const uint8_t *p)
{
    return (uint32_t)MMGR_CALL(endian_rd_be, EndianCtx, .r = p, .n = 4u);
}

uint64_t mmgr_rd64be(const uint8_t *p)
{
    return MMGR_CALL(endian_rd_be, EndianCtx, .r = p, .n = 8u);
}
