/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Byte verbs over a span: the appends, the take, and the room test all four share.
 *
 * @note Every append asks the same question first - is there room, and is the span still good - so
 *       that question is one call and each append is what it does after it.
 * @note A whole value moves a word at a time. endian reverses it once, and the count then selects
 *       stores or loads of eight, four, two and one byte, so nothing walks a byte at a time except
 *       the odd byte at the end of an odd count.
 */
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @brief Arguments for the byteio backends.
 *
 * @note Mirrors OctetusCfg without its const qualifiers.
 */
typedef struct
{
    mmgr_span *w;       /**< Span an append writes into [BORROWS]. */
    mmgr_cspan *r;      /**< Span a take reads from [BORROWS]. */
    const uint8_t *src; /**< Bytes raw appends [BORROWS]. */
    uint64_t *out;      /**< Where take_be stores the value it read [BORROWS]. */
    const uint8_t **blob; /**< Where rd_str points at the run it found [BORROWS]. */
    size_t *blen;       /**< Where rd_str stores that run's length [BORROWS]. */
    uint64_t val;       /**< Value put_be writes. */
    size_t bytes;       /**< Bytes the call moves. */
    uint8_t byte;       /**< The single byte put appends. */
} ByteioCtx;

/**
 * @brief Claims n bytes at the span's cursor, or latches its overflow.
 *
 * @param[in,out] w Span to append into [BORROWS].
 * @param[in]     n Bytes wanted.
 * @return          Where to write them, or NULL when they do not fit [BORROWS].
 * @note Every append reaches this, so the room test, the latch and the cursor all live in one place.
 * @note pos advances by n whether or not the bytes fit, so a span that overran reports how far past
 *       the end the run went rather than stopping at cap. That is a number for reading in a
 *       post-mortem, not a sizing pass to build on - see the warning.
 * @warning An append that does not fit is a build failure. What a writer emits and how big its buffer
 *          is are both fixed before the build, so the two are either compatible or the program is
 *          wrong. The assert says so in the checks build; in a shipping build the latch is damage
 *          control, keeping a wrong program from writing past the end, and not a path to design for.
 */
MMGR_INLINE uint8_t *byteio_claim(mmgr_span *w, size_t n)
{
    const size_t at = w->pos;

    MMGR_ASSERT((w->buf != NULL) && !w->overflow && (at <= w->cap) && (n <= (w->cap - at)),
                "append runs past the end of the span");

    w->pos += n;
    if ((w->buf == NULL) || w->overflow || (n > (w->cap - at)) || (at > w->cap))
    {
        w->overflow = MMGR_TRUE;
        return NULL;
    }
    return w->buf + at;
}

/**
 * @brief Takes n bytes at the read span's cursor and advances past them.
 *
 * @param[in,out] r Span to read from [BORROWS].
 * @param[in]     n Bytes wanted.
 * @return          Where they start, or NULL when the span is short [BORROWS].
 * @note The cursor moves only when the bytes were there. A failed read leaves it where it was, so a
 *       caller that keeps going still knows where it is.
 * @note No assert here, unlike byteio_claim, and the difference is not an oversight. How much a
 *       writer emits is settled before the build, so an append that does not fit is a wrong program.
 *       How much a reader is handed is settled by whatever sent it, so a short read is a fact about
 *       the input and nothing was built wrong. That is why every take answers and no append does.
 */
MMGR_INLINE const uint8_t *byteio_take(mmgr_cspan *r, size_t n)
{
    const size_t at = r->pos;

    if ((r->buf == NULL) || r->err || (at > r->len) || (n > (r->len - at)))
    {
        r->err = MMGR_TRUE;
        return NULL;
    }
    r->pos = at + n;
    return r->buf + at;
}

/**
 * @brief Appends c->byte to the span.
 *
 * @param[in,out] c Span and the byte [BORROWS].
 */
MMGR_INLINE void byteio_put(const ByteioCtx *c)
{
    uint8_t *const at = byteio_claim(c->w, 1u);

    if (at != NULL)
    {
        *at = c->byte;
    }
}

/**
 * @brief Appends c->bytes from c->src as they are.
 *
 * @param[in,out] c Span, source and count [BORROWS].
 * @note Reaches memor.cpy rather than walking bytes here, so there is one mover rather than a second.
 */
MMGR_INLINE void byteio_raw(const ByteioCtx *c)
{
    uint8_t *const at = byteio_claim(c->w, c->bytes);

    if (at != NULL)
    {
        MMGR_CALL(memor.cpy, MemoriaCfg, .dst = at, .src = c->src, .bytes = c->bytes);
    }
}

/**
 * @brief Appends the low c->bytes of c->val, most significant byte first.
 *
 * @param[in,out] c Span, value and count [BORROWS].
 * @note magna_extremitas.rev right-aligns the reversed value into its low c->bytes, so storing those
 *       in the target's own order lays the bytes out most significant first.
 * @note The count selects the stores: eight is one, seven is three, and only an odd final byte is
 *       ever written alone.
 */
MMGR_INLINE void byteio_put_be(const ByteioCtx *c)
{
    uint8_t *at = byteio_claim(c->w, c->bytes);

    if (at == NULL)
    {
        return;
    }

    uint64_t v = MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = c->val, .width = (mmgr_endian_width)c->bytes);

    // A count of eight takes the first branch alone, so the shifts below never reach the full width
    if ((c->bytes & 8u) != 0u)
    {
        MMGR_CALL(proxim.put64, ProximusCfg, .dst = at, .val = v);
        return;
    }
    if ((c->bytes & 4u) != 0u)
    {
        MMGR_CALL(proxim.put32, ProximusCfg, .dst = at, .val = v);
        at += 4;
        v >>= 32;
    }
    if ((c->bytes & 2u) != 0u)
    {
        MMGR_CALL(proxim.put16, ProximusCfg, .dst = at, .val = v);
        at += 2;
        v >>= 16;
    }
    if ((c->bytes & 1u) != 0u)
    {
        *at = (uint8_t)v;
    }
}

/**
 * @brief Reads a big endian value of c->bytes at the cursor and advances past it.
 *
 * @param[in,out] c Span, count and where to store the value [BORROWS].
 * @return          MMGR_TRUE when the bytes were there.
 * @note The mirror of the append: the bytes are gathered in the target's own order at the widest
 *       step the count allows, and reversed once at the end.
 */
MMGR_INLINE mmgr_bool byteio_take_be(const ByteioCtx *c)
{
    const uint8_t *at = byteio_take(c->r, c->bytes);

    if (at == NULL)
    {
        return MMGR_FALSE;
    }

    uint64_t v = 0u;
    size_t sh = 0u;

    if ((c->bytes & 8u) != 0u)
    {
        v = MMGR_CALL(proxim.load64, ProximusCfg, .at = at);
    }
    if ((c->bytes & 4u) != 0u)
    {
        v |= (uint64_t)MMGR_CALL(proxim.load32, ProximusCfg, .at = at) << sh;
        at += 4;
        sh += 32u;
    }
    if ((c->bytes & 2u) != 0u)
    {
        v |= (uint64_t)MMGR_CALL(proxim.load16, ProximusCfg, .at = at) << sh;
        at += 2;
        sh += 16u;
    }
    if ((c->bytes & 1u) != 0u)
    {
        v |= (uint64_t)(*at) << sh;
    }

    *c->out = MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)c->bytes);
    return MMGR_TRUE;
}

/**
 * @brief Reads a length-prefixed run at the cursor and points c->blob at it.
 *
 * @param[in,out] c Span, and where to report the run [BORROWS].
 * @return          MMGR_TRUE when the length and its run both lay within the span.
 * @note The cursor is put back when the run does not fit. A length read that is then not followed by
 *       its payload is not a read at all, and leaving the cursor between the two would give a caller
 *       a position that means nothing.
 */
MMGR_INLINE mmgr_bool byteio_rd_str(const ByteioCtx *c)
{
    const size_t was = c->r->pos;
    uint64_t n = 0u;

    if (!MMGR_CALL(byteio_take_be, ByteioCtx, .r = c->r, .out = &n, .bytes = 4u))
    {
        return MMGR_FALSE;
    }

    const uint8_t *const at = byteio_take(c->r, (size_t)n);

    if (at == NULL)
    {
        c->r->pos = was;
        return MMGR_FALSE;
    }
    *c->blob = at;
    *c->blen = (size_t)n;
    return MMGR_TRUE;
}

/**
 * @brief Right-aligns the integer at c->src into c->w's whole buffer, zero filling ahead of it.
 *
 * @param[in,out] c The integer and its length, and the field [BORROWS].
 * @return          MMGR_TRUE when the integer fits the field.
 * @note Leading zero bytes are skipped before the width is tested, so a value carrying a sign byte
 *       still fits a field of its own size.
 */
MMGR_INLINE mmgr_bool byteio_mpint_fixed(const ByteioCtx *c)
{
    mmgr_span *const w = c->w;
    size_t off = 0u;

    while ((off < c->bytes) && (c->src[off] == 0u))
    {
        off++;
    }

    const size_t vlen = c->bytes - off;

    if ((w->buf == NULL) || (vlen > w->cap))
    {
        w->overflow = MMGR_TRUE;
        return MMGR_FALSE;
    }
    // Explicit cast matches MemoriaCfg: val is a single byte, bytes is a size_t count
    MMGR_CALL(memor.set, MemoriaCfg, .dst = w->buf, .val = (uint8_t)0, .bytes = w->cap);
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = w->buf + (w->cap - vlen), .src = c->src + off, .bytes = vlen);
    // The field is written whole rather than appended to, so the cursor ends at its end
    w->pos = w->cap;
    return MMGR_TRUE;
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_byteio_ and byteio_ prefixes, which the two share.
 */
#define BYTEIO_ENTRY(ret, name, ...)                                                                  \
    GENERIC_ENTRY(mmgr_byteio_, byteio_, ByteioCtx, OctetusCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_byteio_ and byteio_ prefixes, which the two share.
 */
#define BYTEIO_ENTRY_V(name, ...) GENERIC_ENTRY_V(mmgr_byteio_, byteio_, ByteioCtx, OctetusCfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in octetus_introitus_exitus.h.
 */
BYTEIO_ENTRY_V(put, .w = c->w, .byte = c->byte)
BYTEIO_ENTRY_V(put_be, .w = c->w, .val = c->val, .bytes = c->bytes)
BYTEIO_ENTRY_V(raw, .w = c->w, .src = c->src, .bytes = c->bytes)
BYTEIO_ENTRY(mmgr_bool, take_be, .r = c->r, .bytes = c->bytes, .out = c->out)
BYTEIO_ENTRY(mmgr_bool, rd_str, .r = c->r, .blob = c->blob, .blen = c->blen)
BYTEIO_ENTRY(mmgr_bool, mpint_fixed, .w = c->w, .src = c->src, .bytes = c->bytes)
