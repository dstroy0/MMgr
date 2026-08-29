/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Spans over a caller's buffer: the two constructors and the walks over them.
 *
 * @note Holds no storage of its own, and every entry returns by value rather than through a pointer.
 * @note A span only points at the caller's bytes [BORROWS]. Every span cut from a buffer, and every
 *       span cut from that one, is good for exactly as long as the buffer is, and none of them frees
 *       it.
 * @note A span travels inside the argument pack rather than being pointed at, so a walk reads the
 *       caller's span and cannot change it. reset is the exception and takes args->at.
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
 * @brief Builds a fill span over args->buf, with pos at 0 and overflow clear.
 *
 * @param[in] args Buffer buf and its extent cap [BORROWS].
 * @return      The span, by value, still aimed at args->buf [BORROWS].
 * @warning cap is taken as given: the caller promises cap writable bytes at buf, and the two asserts
 *          only catch a null buffer and an empty one. A cap larger than the buffer is not caught here
 *          and the walks will hand out spans past its end.
 */
MMGR_INLINE mmgr_span spat_from(const SpatiumCtx *args)
{
    MMGR_ASSERT(args->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(args->cap != 0u, "a span needs a capacity");

    mmgr_span s;

    s.buf = args->buf;
    s.cap = args->cap;
    s.pos = 0u;
    s.overflow = MMGR_FALSE;
    return s;
}

/**
 * @brief Builds a read span over args->cbuf, with pos at 0 and err clear.
 *
 * @param[in] args Buffer cbuf and its extent cap [BORROWS].
 * @return      The span, by value, still aimed at args->cbuf [BORROWS].
 * @warning Takes cbuf and cap exactly as handed over, with none of the asserts spat_from makes. A
 *          null cbuf or a zero cap still builds, and it is cok that later reports it unusable. A cap
 *          reaching past the end of the buffer is the one cok cannot see, and it reads as usable.
 */
MMGR_INLINE mmgr_cspan spat_cfrom(const SpatiumCtx *args)
{
    mmgr_cspan s;

    s.buf = args->cbuf;
    s.len = args->cap;
    s.pos = 0u;
    s.err = MMGR_FALSE;
    return s;
}

/**
 * @brief Returns whether args->s covers any bytes at all.
 *
 * @param[in] args Span to test, as args->s [BORROWS].
 * @return      MMGR_TRUE when buf is not NULL and cap is not 0.
 */
MMGR_INLINE mmgr_bool spat_has_storage(const SpatiumCtx *args)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((args->s.buf != NULL) && (args->s.cap != 0u));
}

/**
 * @brief Returns whether args->s is still usable.
 *
 * @param[in] args Span to test, as args->s [BORROWS].
 * @return      MMGR_TRUE when the span has storage and has not overflowed.
 */
MMGR_INLINE mmgr_bool spat_ok(const SpatiumCtx *args)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)(spat_has_storage(args) && !args->s.overflow);
}

/**
 * @brief Returns whether args->cs is still usable.
 *
 * @param[in] args Read span to test, as args->cs [BORROWS].
 * @return      MMGR_TRUE when the span has storage and has recorded no error.
 * @note The read side of spat_ok. The two cannot share a body: a read span names its extent len and a
 *       fill span names it cap, which is what keeps the two from being mixed up.
 */
MMGR_INLINE mmgr_bool spat_cok(const SpatiumCtx *args)
{
    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((args->cs.buf != NULL) && (args->cs.len != 0u) && !args->cs.err);
}

/**
 * @brief Returns the span at args->at to its start and clears its overflow.
 *
 * @param[in,out] args Span to rewind, as args->at [BORROWS].
 * @warning at is written through with no check of its own, so it has to point at a live span. The
 *          const on the pack covers the pack, not the span at the far end of at.
 * @note Rewinds the span only. The bytes it covers are left exactly as they were, so a reset span
 *       hands out storage that still holds whatever the last fill wrote.
 */
MMGR_INLINE void spat_reset(const SpatiumCtx *args)
{
    args->at->pos = 0u;
    args->at->overflow = MMGR_FALSE;
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
 * @brief Returns the span beginning args->n bytes into args->s.
 *
 * @param[in] args Span to walk, as args->s, and the bytes to skip as args->n [BORROWS].
 * @return      A span over what is left, or a failed span when args->n is past cap.
 * @note The span that comes back starts inside the same buffer args->s covers [BORROWS]. It is a
 *       second view of those bytes, not a copy of them, and writes through either one are seen by
 *       both. pos comes forward with it, rebased to the new start and resting at 0 once the skip
 *       has passed it.
 */
MMGR_INLINE mmgr_span spat_after(const SpatiumCtx *args)
{
    mmgr_span r;

    if (args->n > args->s.cap)
    {
        return spat_failed();
    }
    r.buf = (args->s.buf != NULL) ? (args->s.buf + args->n) : NULL;
    r.cap = args->s.cap - args->n;
    r.pos = (args->s.pos > args->n) ? (args->s.pos - args->n) : 0u;
    r.overflow = args->s.overflow;
    return r;
}

/**
 * @brief Returns the span covering only the first args->n bytes of args->s.
 *
 * @param[in] args Span to narrow, as args->s, and the bytes to keep as args->n [BORROWS].
 * @return      A span over those bytes, or a failed span when args->n is past cap.
 * @note The span that comes back starts on the same byte args->s starts on [BORROWS], holding a
 *       shorter cap over the same storage. Both cover the opening bytes, so a fill through one is
 *       read by the other. pos comes forward, held down to the shorter cap when it sat past it.
 */
MMGR_INLINE mmgr_span spat_first(const SpatiumCtx *args)
{
    mmgr_span r;

    if (args->n > args->s.cap)
    {
        return spat_failed();
    }
    r.buf = args->s.buf;
    r.cap = args->n;
    r.pos = (args->s.pos < args->n) ? args->s.pos : args->n;
    r.overflow = args->s.overflow;
    return r;
}

/**
 * @brief Returns a read span over the first args->n bytes written into args->s.
 *
 * @param[in] args Span to read back, as args->s, and the bytes to cover as args->n [BORROWS].
 * @return      A read span over them, marked err when args->n is past what was written.
 * @warning The read span points at the fill span's own bytes [BORROWS], and the const on it binds
 *          this view, not the storage. Filling args->s again moves what the reader sees, so read it
 *          back before the next fill or copy the bytes out.
 */
MMGR_INLINE mmgr_cspan spat_read(const SpatiumCtx *args)
{
    mmgr_cspan r;

    r.buf = args->s.buf;
    r.len = (args->n < args->s.pos) ? args->n : args->s.pos;
    r.pos = 0u;
    // A span that overflowed produced fewer bytes than were asked of it, and the read side is told
    // so rather than being handed a shorter span that looks whole
    // Explicit cast narrows the combined test into the mmgr_bool container
    r.err = (mmgr_bool)(args->s.overflow || (args->n > args->s.pos));
    return r;
}

/**
 * @brief Returns a read span over everything written into args->s.
 *
 * @param[in] args Span to read back, as args->s [BORROWS].
 * @return      A read span over its first pos bytes, carrying s's overflow as its err.
 * @warning Hands back spat_read's span, so it points at args->s's own bytes [BORROWS] and the same
 *          caution holds: what it covers is whatever the next fill leaves there.
 */
MMGR_INLINE mmgr_cspan spat_produced(const SpatiumCtx *args)
{
    return MMGR_CALL(spat_read, SpatiumCtx, .s = args->s, .n = args->s.pos);
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_spat_ and spat_ prefixes, which the two share.
 * @param[in] ...  Initializers for the SpatiumCtx literal, written in terms of args. MMGR_CALL zeroes
 *                 every field left out.
 */
#define SPAT_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_spat_, spat_, SpatiumCtx, SpatiumCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for the entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_spat_ and spat_ prefixes.
 * @param[in] ...  The same initializers SPAT_ENTRY takes. No ret here, since the entry this builds
 *                 returns nothing.
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
SPAT_ENTRY(mmgr_span, from, .buf = args->buf, .cap = args->cap)
SPAT_ENTRY(mmgr_cspan, cfrom, .cbuf = args->cbuf, .cap = args->cap)
SPAT_ENTRY(mmgr_bool, ok, .s = args->s)
SPAT_ENTRY(mmgr_bool, cok, .cs = args->cs)
SPAT_ENTRY(mmgr_bool, has_storage, .s = args->s)
SPAT_ENTRY_V(reset, .at = args->at)
SPAT_ENTRY(mmgr_span, after, .s = args->s, .n = args->n)
SPAT_ENTRY(mmgr_span, first, .s = args->s, .n = args->n)
SPAT_ENTRY(mmgr_cspan, produced, .s = args->s)
SPAT_ENTRY(mmgr_cspan, read, .s = args->s, .n = args->n)
