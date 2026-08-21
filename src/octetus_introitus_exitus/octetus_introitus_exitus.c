// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

/**
 * @file byteio.c
 * @brief Byte field reads and writes, big endian on the wire.
 *
 * Nothing here tests whether the bytes fit. The caller has the buffer and the field width in front
 * of it when it writes the call, so a field that runs past the end is a program that should not
 * have been built, and MMGR_ASSERT is what says so - nothing in a shipping build, an abort in the
 * checks build. What is left is the stores.
 *
 * The internals take one parameter, a pointer to their arguments. Every context is declared here
 * and never leaves.
 */

/** @brief Arguments to put. */
typedef struct
{
    mmgr_spat *w;
    uint8_t b;
} ByteioPutArgs;

/** @brief Arguments to put_be. */
typedef struct
{
    mmgr_spat *w;
    uint64_t val;
    int32_t nbytes;
} ByteioPutBeArgs;

/** @brief Arguments to bytes. */
typedef struct
{
    mmgr_spat *w;
    const void *src;
    size_t n;
} ByteioBytesArgs;

/** @brief Arguments to take_be, and to rd_u32, which is take_be with four. */
typedef struct
{
    const uint8_t *p;
    size_t len;
    size_t *off;
    uint64_t *out;
    size_t nbytes;
} ByteioTakeBeArgs;

/** @brief Arguments to rd_str. */
typedef struct
{
    const uint8_t *p;
    size_t len;
    size_t *off;
    const uint8_t **out;
    uint32_t *slen;
} ByteioRdStrArgs;

/** @brief Arguments to mpint_to_fixed. */
typedef struct
{
    const uint8_t *m;
    uint32_t mlen;
    uint8_t *out;
    size_t outlen;
} ByteioMpintArgs;

/**
 * @brief One byte.
 * @param a Arguments.
 */
MMGR_INLINE void byteio_put(const ByteioPutArgs *a)
{
    mmgr_spat *w = a->w;

    MMGR_ASSERT(w->pos < w->cap, "byte written past the span");
    w->buf[w->pos] = a->b;
    w->pos++;
}

/**
 * @brief A big endian field.
 * @param a Arguments.
 */
MMGR_INLINE void byteio_put_be(const ByteioPutBeArgs *a)
{
    mmgr_spat *w = a->w;

    MMGR_ASSERT((w->cap - w->pos) >= (size_t)a->nbytes, "field written past the span");
    for (int32_t s = (a->nbytes - 1) * 8; s >= 0; s -= 8)
    {
        w->buf[w->pos] = (uint8_t)(a->val >> s);
        w->pos++;
    }
}

/**
 * @brief A run of bytes.
 * @param a Arguments.
 */
MMGR_INLINE void byteio_bytes(const ByteioBytesArgs *a)
{
    mmgr_spat *w = a->w;

    MMGR_ASSERT((w->cap - w->pos) >= a->n, "run written past the span");
    memor.cpy(w->buf + w->pos, a->src, a->n);
    w->pos += a->n;
}

/**
 * @brief Take a big endian field.
 * @param a Arguments.
 */
MMGR_INLINE void byteio_take_be(const ByteioTakeBeArgs *a)
{
    MMGR_ASSERT((a->len - *a->off) >= a->nbytes, "field read past the buffer");

    uint64_t v = 0;
    for (size_t i = 0; i < a->nbytes; i++)
    {
        v = (v << 8) | a->p[*a->off + i];
    }
    *a->out = v;
    *a->off += a->nbytes;
}

/**
 * @brief Read a length prefixed string.
 * @param a Arguments.
 *
 * The length arrives from the wire rather than from the caller, so it is the one quantity here
 * that is not known where the call is written. It is checked.
 */
MMGR_INLINE mmgr_bool byteio_rd_str(const ByteioRdStrArgs *a)
{
    const size_t start = *a->off;
    uint32_t n = 0;

    /* The prefix is wire data. Whether four bytes are there is a fact about what arrived, not a
       promise the caller made, so this one is checked. */
    if ((*a->off > a->len) || ((a->len - *a->off) < 4))
    {
        return MMGR_FALSE;
    }
    n = mmgr_rd32be(a->p + *a->off);
    *a->off += 4;
    if (n > (a->len - *a->off))
    {
        *a->off = start;
        return MMGR_FALSE;
    }
    *a->out = a->p + *a->off;
    *a->slen = n;
    *a->off += n;
    return MMGR_TRUE;
}

/**
 * @brief Right align an mpint into a fixed width field.
 * @param a Arguments.
 *
 * mlen comes off the wire, so the width it needs is checked rather than asserted.
 */
MMGR_INLINE mmgr_bool byteio_mpint_to_fixed(const ByteioMpintArgs *a)
{
    uint32_t off = 0;

    while ((off < a->mlen) && (a->m[off] == 0))
    {
        off++;
    }

    const uint32_t vlen = a->mlen - off;
    if (vlen > a->outlen)
    {
        return MMGR_FALSE;
    }
    memor.set(a->out, 0, a->outlen);
    memor.cpy(a->out + (a->outlen - vlen), a->m + off, vlen);
    return MMGR_TRUE;
}

void mmgr_octet_put(mmgr_spat *w, uint8_t b)
{
    MMGR_CALL(byteio_put, ByteioPutArgs, w, b);
}

void mmgr_octet_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes)
{
    MMGR_CALL(byteio_put_be, ByteioPutBeArgs, w, val, nbytes);
}

void mmgr_octet_bytes(mmgr_spat *w, const void *src, size_t n)
{
    MMGR_CALL(byteio_bytes, ByteioBytesArgs, w, src, n);
}

void mmgr_octet_take_be(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes)
{
    MMGR_CALL(byteio_take_be, ByteioTakeBeArgs, p, len, off, out, nbytes);
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
    return MMGR_CALL(byteio_rd_str, ByteioRdStrArgs, p, len, off, out, slen);
}

mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen)
{
    return MMGR_CALL(byteio_mpint_to_fixed, ByteioMpintArgs, m, mlen, out, outlen);
}
