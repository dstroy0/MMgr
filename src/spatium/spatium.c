/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Spans over a caller's buffer: the two constructors and the walks over them.
 *
 * @note Holds no storage of its own, and every entry returns by value rather than through a pointer.
 * @note A span travels inside the argument pack rather than being pointed at, so a walk reads the
 *       caller's span and cannot change it. reset is the exception and takes c->at.
 * @note Reaches nothing outside config.
 */
#include "spatium/spatium.h"

/**
 * @brief Arguments for every spat backend, grouped by the calls that read them.
 *
 * @note Mirrors SpatiumCfg without its const qualifiers.
 */
typedef struct
{
    mmgr_span s;         /**< Fill span the walks read. */
    mmgr_cspan cs;       /**< Read span cok tests. */
    mmgr_span *at;       /**< Fill span reset rewinds [BORROWS]. */
    uint8_t *buf;        /**< Buffer from builds over [BORROWS]. */
    const uint8_t *cbuf; /**< Buffer cfrom builds over [BORROWS]. */
    size_t cap;          /**< Bytes at buf, or at cbuf. */
    size_t n;            /**< Count after, first and read take. */
} SpatiumCtx;

/**
 * @brief Builds a fill span over c->buf, with pos at 0 and overflow clear.
 *
 * @param[in] c Buffer buf and its extent cap [BORROWS].
 * @return      The span, by value.
 */
MMGR_INLINE mmgr_span spat_from(const SpatiumCtx *c)
{
    MMGR_ASSERT(c->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(c->cap != 0u, "a span needs a capacity");

    mmgr_span s;

    s.buf = c->buf;
    s.cap = c->cap;
    s.pos = 0u;
    s.overflow = MMGR_FALSE;
    return s;
}

/**
 * @brief Builds a read span over c->cbuf, with pos at 0 and err clear.
 *
 * @param[in] c Buffer cbuf and its extent cap [BORROWS].
 * @return      The span, by value.
 */
MMGR_INLINE mmgr_cspan spat_cfrom(const SpatiumCtx *c)
{
    mmgr_cspan s;

    s.buf = c->cbuf;
    s.len = c->cap;
    s.pos = 0u;
    s.err = MMGR_FALSE;
    return s;
}

/**
 * @brief Returns whether c->s covers any bytes at all.
 *
 * @param[in] c Span to test, as c->s [BORROWS].
 * @return      MMGR_TRUE when buf is not NULL and cap is not 0.
 */
MMGR_INLINE mmgr_bool spat_has_storage(const SpatiumCtx *c)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((c->s.buf != NULL) && (c->s.cap != 0u));
}

/**
 * @brief Returns whether c->s is still usable.
 *
 * @param[in] c Span to test, as c->s [BORROWS].
 * @return      MMGR_TRUE when the span has storage and has not overflowed.
 */
MMGR_INLINE mmgr_bool spat_ok(const SpatiumCtx *c)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)(spat_has_storage(c) && !c->s.overflow);
}

/**
 * @brief Returns whether c->cs is still usable.
 *
 * @param[in] c Read span to test, as c->cs [BORROWS].
 * @return      MMGR_TRUE when the span has storage and has recorded no error.
 * @note The read side of spat_ok. The two cannot share a body: a read span names its extent len and a
 *       fill span names it cap, which is what keeps the two from being mixed up.
 */
MMGR_INLINE mmgr_bool spat_cok(const SpatiumCtx *c)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((c->cs.buf != NULL) && (c->cs.len != 0u) && !c->cs.err);
}

/**
 * @brief Returns the span at c->at to its start and clears its overflow.
 *
 * @param[in,out] c Span to rewind, as c->at [BORROWS].
 */
MMGR_INLINE void spat_reset(const SpatiumCtx *c)
{
    c->at->pos = 0u;
    c->at->overflow = MMGR_FALSE;
}

/**
 * @brief Returns a span that has already failed, for a narrowing that ran past the storage.
 *
 * @return An empty span with overflow set.
 * @note Both narrowings answer a request past the end this way rather than with a shorter span: a
 *       caller that asked for bytes that are not there has a bug, and a span that looked whole
 *       would hide it.
 */
MMGR_INLINE mmgr_span spat_failed(void)
{
    mmgr_span r;

    r.buf = NULL;
    r.cap = 0u;
    r.pos = 0u;
    r.overflow = MMGR_TRUE;
    return r;
}

/**
 * @brief Returns the span beginning c->n bytes into c->s.
 *
 * @param[in] c Span to walk, as c->s, and the bytes to skip as c->n [BORROWS].
 * @return      A span over what is left, or a failed span when c->n is past cap.
 */
MMGR_INLINE mmgr_span spat_after(const SpatiumCtx *c)
{
    mmgr_span r;

    if (c->n > c->s.cap)
    {
        return spat_failed();
    }
    r.buf = (c->s.buf != NULL) ? (c->s.buf + c->n) : NULL;
    r.cap = c->s.cap - c->n;
    r.pos = (c->s.pos > c->n) ? (c->s.pos - c->n) : 0u;
    r.overflow = c->s.overflow;
    return r;
}

/**
 * @brief Returns the span covering only the first c->n bytes of c->s.
 *
 * @param[in] c Span to narrow, as c->s, and the bytes to keep as c->n [BORROWS].
 * @return      A span over those bytes, or a failed span when c->n is past cap.
 */
MMGR_INLINE mmgr_span spat_first(const SpatiumCtx *c)
{
    mmgr_span r;

    if (c->n > c->s.cap)
    {
        return spat_failed();
    }
    r.buf = c->s.buf;
    r.cap = c->n;
    r.pos = (c->s.pos < c->n) ? c->s.pos : c->n;
    r.overflow = c->s.overflow;
    return r;
}

/**
 * @brief Returns a read span over the first c->n bytes written into c->s.
 *
 * @param[in] c Span to read back, as c->s, and the bytes to cover as c->n [BORROWS].
 * @return      A read span over them, marked err when c->n is past what was written.
 */
MMGR_INLINE mmgr_cspan spat_read(const SpatiumCtx *c)
{
    mmgr_cspan r;

    r.buf = c->s.buf;
    r.len = (c->n < c->s.pos) ? c->n : c->s.pos;
    r.pos = 0u;
    // A span that overflowed produced fewer bytes than were asked of it, and the read side is told
    // so rather than being handed a shorter span that looks whole
    // Explicit cast narrows the combined test into the mmgr_bool container
    r.err = (mmgr_bool)(c->s.overflow || (c->n > c->s.pos));
    return r;
}

/**
 * @brief Returns a read span over everything written into c->s.
 *
 * @param[in] c Span to read back, as c->s [BORROWS].
 * @return      A read span over its first pos bytes, carrying s's overflow as its err.
 */
MMGR_INLINE mmgr_cspan spat_produced(const SpatiumCtx *c)
{
    return MMGR_CALL(spat_read, SpatiumCtx, .s = c->s, .n = c->s.pos);
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_spat_ and spat_ prefixes, which the two share.
 */
#define SPAT_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_spat_, spat_, SpatiumCtx, SpatiumCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for the entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_spat_ and spat_ prefixes.
 */
#define SPAT_ENTRY_V(name, ...) GENERIC_ENTRY_V(mmgr_spat_, spat_, SpatiumCtx, SpatiumCfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in spatium.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest.
 * @note from forwards buf and cfrom forwards cbuf, so a buffer that may not be written cannot reach
 *       the fill constructor. Both take their extent from cap.
 */
SPAT_ENTRY(mmgr_span, from, .buf = c->buf, .cap = c->cap)
SPAT_ENTRY(mmgr_cspan, cfrom, .cbuf = c->cbuf, .cap = c->cap)
SPAT_ENTRY(mmgr_bool, ok, .s = c->s)
SPAT_ENTRY(mmgr_bool, cok, .cs = c->cs)
SPAT_ENTRY(mmgr_bool, has_storage, .s = c->s)
SPAT_ENTRY_V(reset, .at = c->at)
SPAT_ENTRY(mmgr_span, after, .s = c->s, .n = c->n)
SPAT_ENTRY(mmgr_span, first, .s = c->s, .n = c->n)
SPAT_ENTRY(mmgr_cspan, produced, .s = c->s)
SPAT_ENTRY(mmgr_cspan, read, .s = c->s, .n = c->n)
