// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

/**
 * @file octetus_introitus_exitus.c
 * @brief Byte field reads and writes, big endian on the wire.
 *
 * Every entry below takes one parameter, a pointer to OctetCtx. A field on the wire is a buffer, a
 * bound, a cursor and a value, whichever direction it is going, so they are one context.
 *
 * Nothing here tests whether the bytes fit. The caller has the buffer and the field width in front
 * of it when it writes the call, so a field that runs past the end is a program that should not
 * have been built, and MMGR_ASSERT is what says so - nothing in a shipping build, an abort in the
 * checks build. What is left is the stores.
 *
 * That is the whole rule, and it is why rd_str and mpint_fixed are not here any more. A length
 * prefixed string takes its length from the data, so it is the one thing that cannot be settled
 * where the call is written; it needed a run time bound and a way to fail, and it was the only
 * entry that had either. An mpint does not move bytes, it reinterprets them. Both went to
 * cellularum_laboro, where parsing lives. The context lost six of its fifteen fields going with
 * them, which is the measure of how much they did not belong.
 */

/** @brief A field on the wire, either direction. */
typedef struct
{
    mmgr_spat *w;        /**< The span, when writing through one. */
    const uint8_t *p;    /**< The buffer, when reading. */
    size_t len;          /**< How far it may go. */
    size_t *off;         /**< Cursor into it. */
    uint64_t val;        /**< The value, either direction. */
    uint64_t *out;       /**< Where a read value goes. */
    size_t n;            /**< Bytes wide, or a run length. */
    uint8_t b;           /**< One byte, for put. */
    const void *src;     /**< A run of bytes, for raw. */
} OctetCtx;

/**
 * @brief One byte.
 * @param c In/out. The field.
 */
MMGR_INLINE void octet_put(OctetCtx *c)
{
    MMGR_ASSERT(c->w->pos < c->w->cap, "byte written past the span");
    c->w->buf[c->w->pos] = c->b;
    c->w->pos++;
}

/**
 * @brief A big endian field of @c n bytes.
 * @param c In/out. The field.
 */
MMGR_INLINE void octet_put_be(OctetCtx *c)
{
    MMGR_ASSERT((c->w->cap - c->w->pos) >= c->n, "field written past the span");
    if ((c->n == 2u) || (c->n == 4u) || (c->n == 8u))
    {
        magna_extremitas.wr(&(EndianCfg){c->w->buf + c->w->pos, 0, c->val, (mmgr_endian_width)c->n});
        c->w->pos += c->n;
        return;
    }
    for (size_t i = 0; i < c->n; i++)
    {
        c->w->buf[c->w->pos] = (uint8_t)(c->val >> (8u * (c->n - 1u - i)));
        c->w->pos++;
    }
}

/**
 * @brief A run of bytes, verbatim.
 * @param c In/out. The field.
 */
MMGR_INLINE void octet_bytes(OctetCtx *c)
{
    MMGR_ASSERT((c->w->cap - c->w->pos) >= c->n, "run written past the span");
    memor.cpy(c->w->buf + c->w->pos, c->src, c->n);
    c->w->pos += c->n;
}

/**
 * @brief Take a big endian field of @c n bytes.
 * @param c In/out. The field.
 */
MMGR_INLINE void octet_take_be(OctetCtx *c)
{
    MMGR_ASSERT((c->len - *c->off) >= c->n, "field read past the buffer");

    if ((c->n == 2u) || (c->n == 4u) || (c->n == 8u))
    {
        *c->out = magna_extremitas.rd(&(EndianCfg){0, c->p + *c->off, 0, (mmgr_endian_width)c->n});
        *c->off += c->n;
        return;
    }

    uint64_t v = 0;
    for (size_t i = 0; i < c->n; i++)
    {
        v = (v << 8) | c->p[*c->off + i];
    }
    *c->out = v;
    *c->off += c->n;
}

void mmgr_octet_put(mmgr_spat *w, uint8_t b)
{
    MMGR_CALL(octet_put, OctetCtx, .w = w, .b = b);
}

void mmgr_octet_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes)
{
    MMGR_CALL(octet_put_be, OctetCtx, .w = w, .val = val, .n = (size_t)nbytes);
}

void mmgr_octet_bytes(mmgr_spat *w, const void *src, size_t n)
{
    MMGR_CALL(octet_bytes, OctetCtx, .w = w, .src = src, .n = n);
}

void mmgr_octet_take_be(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes)
{
    MMGR_CALL(octet_take_be, OctetCtx, .p = p, .len = len, .off = off, .out = out, .n = nbytes);
}

void mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out)
{
    /* take_be with four, except endian already has the four byte load, so it does not go round
       through a wider accumulator to get there. */
    MMGR_ASSERT((len - *off) >= 4, "field read past the buffer");
    *out = (uint32_t)magna_extremitas.rd(&(EndianCfg){0, p + *off, 0, MMGR_ENDIAN_32});
    *off += 4;
}

