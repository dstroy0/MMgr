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
    const uint8_t **str; /**< Where a string is handed back. */
    uint32_t *slen;      /**< And its length. */
    const uint8_t *m;    /**< The mpint. */
    uint32_t mlen;       /**< Its length. */
    uint8_t *dst;        /**< The fixed width field it goes into. */
    size_t dstlen;       /**< Its width. */
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

    uint64_t v = 0;
    for (size_t i = 0; i < c->n; i++)
    {
        v = (v << 8) | c->p[*c->off + i];
    }
    *c->out = v;
    *c->off += c->n;
}

/**
 * @brief Read a length prefixed string.
 * @param c In/out. The field.
 * @return MMGR_FALSE if the length or the body runs past the end.
 *
 * The length arrives from the wire rather than from the caller, so it is the one quantity here that
 * is not known where the call is written. It is checked.
 */
MMGR_INLINE mmgr_bool octet_rd_str(OctetCtx *c)
{
    const size_t start = *c->off;

    if ((*c->off > c->len) || ((c->len - *c->off) < 4u))
    {
        return MMGR_FALSE;
    }
    const uint32_t n = mmgr_rd32be(c->p + *c->off);
    *c->off += 4u;

    if (n > (c->len - *c->off))
    {
        *c->off = start;
        return MMGR_FALSE;
    }
    *c->str = c->p + *c->off;
    *c->slen = n;
    *c->off += n;
    return MMGR_TRUE;
}

/**
 * @brief Right align an mpint into a fixed width field.
 * @param c In/out. The field.
 * @return MMGR_FALSE if the value does not fit.
 *
 * @c mlen comes off the wire, so the width it needs is checked rather than asserted.
 */
MMGR_INLINE mmgr_bool octet_mpint_to_fixed(OctetCtx *c)
{
    uint32_t off = 0;

    while ((off < c->mlen) && (c->m[off] == 0))
    {
        off++;
    }

    const uint32_t vlen = c->mlen - off;
    if (vlen > c->dstlen)
    {
        return MMGR_FALSE;
    }
    memor.set(c->dst, 0, c->dstlen);
    memor.cpy(c->dst + (c->dstlen - vlen), c->m + off, vlen);
    return MMGR_TRUE;
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
    *out = mmgr_rd32be(p + *off);
    *off += 4;
}

mmgr_bool mmgr_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen)
{
    return MMGR_CALL(octet_rd_str, OctetCtx, .p = p, .len = len, .off = off, .str = out, .slen = slen);
}

mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen)
{
    return MMGR_CALL(octet_mpint_to_fixed, OctetCtx, .m = m, .mlen = mlen, .dst = out, .dstlen = outlen);
}
