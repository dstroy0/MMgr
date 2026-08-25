/**
 * @brief Spans over a caller's buffer: the two span types, and the spat dispatch table.
 *
 * @note A span is a region and a cursor into it, with a sticky flag that latches once a walk has run
 *       past the end. It carries no storage of its own and is passed by value.
 * @note Two types, not one: a fill span is written through and latches overflow, a read span is not
 *       and latches err. Their extents are named cap and len, so one cannot be handed where the
 *       other belongs without the compiler saying so.
 * @note The flag is what makes a span worth passing: a caller may walk one through several steps and
 *       test once at the end, rather than after each.
 */
#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief A buffer being filled: its bytes, how far the writer has gone, and whether it ran out.
 *
 * @note Returned by value; this module keeps none of its own.
 * @note overflow is sticky: once a write has run past cap it stays set, and only mmgr_spat_reset
 *       clears it.
 * @warning buf points at the caller's storage, which must outlive every use of the span [BORROWS].
 */
typedef struct
{
    uint8_t *buf;       /**< The buffer the span covers [BORROWS]. */
    size_t cap;         /**< Bytes in buf. */
    size_t pos;         /**< Bytes written so far, which mmgr_spat_from sets to 0. */
    mmgr_bool overflow; /**< Set once a write has run past cap, and never cleared after. */
} mmgr_span;

/**
 * @brief A buffer being read: its bytes, how far the reader has gone, and whether it ran out.
 *
 * @note The read-only counterpart of mmgr_span. buf is const here, so a span handed out for reading
 *       cannot be written through it.
 * @note err is sticky, for the same reason overflow is.
 * @warning buf points at storage that must outlive every use of the span [BORROWS].
 */
typedef struct
{
    const uint8_t *buf; /**< First byte, or NULL when there is nothing to read [BORROWS]. */
    size_t len;         /**< Readable bytes at buf. */
    size_t pos;         /**< Bytes read so far. */
    mmgr_bool err;      /**< Set once a read has run past len, and never cleared after. */
} mmgr_cspan;

/**
 * @brief Type of the spat dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the ten members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note These take spans by value rather than an argument pack: a span is the argument, not one
 *       field of one, and every entry here is a pure function of it.
 * @note There is no len or room entry. mmgr_span is the caller's own value, so s.pos and
 *       s.cap - s.pos are already in hand and a call to fetch them would be a second way to spell
 *       what a member read already says.
 */
typedef struct
{
    mmgr_span (*from)(uint8_t *buf, size_t cap);         /**< Builds a fill span over a buffer. */
    mmgr_cspan (*cfrom)(const uint8_t *buf, size_t len); /**< Builds a read span over a buffer. */
    mmgr_bool (*ok)(mmgr_span s);                        /**< Whether a fill span is still usable. */
    mmgr_bool (*cok)(mmgr_cspan s);                      /**< Whether a read span is still usable. */
    mmgr_bool (*has_storage)(mmgr_span s);               /**< Whether the span covers any bytes at all. */
    void (*reset)(mmgr_span *s);                         /**< Returns pos to 0 and clears overflow. */
    mmgr_span (*after)(mmgr_span s, size_t off);         /**< The span beginning off bytes in. */
    mmgr_span (*first)(mmgr_span s, size_t n);           /**< The span covering only the first n bytes. */
    mmgr_cspan (*produced)(mmgr_span s);                 /**< A read span over everything written. */
    mmgr_cspan (*read)(mmgr_span s, size_t len);         /**< A read span over the first len written. */
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, from, cfrom, ok, cok, has_storage, reset, after, first, produced, read);

/**
 * @brief Builds a fill span over buf, with pos at 0 and overflow clear.
 *
 * @param[in] buf Buffer the span will cover [BORROWS].
 * @param[in] cap Bytes at buf.
 * @return        The span, by value.
 * @note Asserts buf is not NULL and cap is not 0; the default MMGR_ASSERT leaves both unevaluated.
 * @warning The span carries buf away, so that buffer must outlive every use of the span [BORROWS].
 */
mmgr_span mmgr_spat_from(uint8_t *buf, size_t cap);

/**
 * @brief Builds a read span over buf, with pos at 0 and err clear.
 *
 * @param[in] buf Buffer the span will cover [BORROWS].
 * @param[in] len Readable bytes at buf.
 * @return        The span, by value.
 * @warning The span carries buf away, so that buffer must outlive every use of the span [BORROWS].
 */
mmgr_cspan mmgr_spat_cfrom(const uint8_t *buf, size_t len);

/**
 * @brief Returns whether s has storage and has not overflowed.
 *
 * @param[in] s Span to test.
 * @return      MMGR_TRUE when the span is still usable.
 */
mmgr_bool mmgr_spat_ok(mmgr_span s);

/**
 * @brief Returns whether s has storage and has recorded no error.
 *
 * @param[in] s Read span to test.
 * @return      MMGR_TRUE when the span is still usable.
 */
mmgr_bool mmgr_spat_cok(mmgr_cspan s);

/**
 * @brief Returns whether s covers any bytes at all.
 *
 * @param[in] s Span to test.
 * @return      MMGR_TRUE when buf is not NULL and cap is not 0.
 */
mmgr_bool mmgr_spat_has_storage(mmgr_span s);

/**
 * @brief Returns s to its start and clears its overflow.
 *
 * @param[in,out] s Span to rewind [BORROWS].
 * @note The one call that clears overflow, which is otherwise sticky for the span's whole life.
 */
void mmgr_spat_reset(mmgr_span *s);

/**
 * @brief Returns the span beginning off bytes into s.
 *
 * @param[in] s   Span to walk.
 * @param[in] off Bytes to skip.
 * @return        A span over what is left, or a failed span when off is past cap.
 * @note An off of exactly cap gives an empty span that has not failed: nothing is left, but nothing
 *       went wrong either.
 */
mmgr_span mmgr_spat_after(mmgr_span s, size_t off);

/**
 * @brief Returns the span covering only the first n bytes of s.
 *
 * @param[in] s Span to narrow.
 * @param[in] n Bytes to keep.
 * @return      A span over those bytes, or a failed span when n is past cap.
 */
mmgr_span mmgr_spat_first(mmgr_span s, size_t n);

/**
 * @brief Returns a read span over everything written into s.
 *
 * @param[in] s Span to read back.
 * @return      A read span over its first pos bytes, carrying s's overflow as its err.
 * @note The same step as mmgr_spat_read against s's own pos, which is the case that cannot ask for
 *       more than was written and so cannot fail on its own account.
 */
mmgr_cspan mmgr_spat_produced(mmgr_span s);

/**
 * @brief Returns a read span over the first len bytes written into s.
 *
 * @param[in] s   Span to read back.
 * @param[in] len Bytes to cover.
 * @return        A read span over them, marked err when len is past what was written.
 */
mmgr_cspan mmgr_spat_read(mmgr_span s, size_t len);

/**
 * @brief Dispatch table instance named spat; each member calls the matching mmgr_spat_ function.
 */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .from = mmgr_spat_from,
    .cfrom = mmgr_spat_cfrom,
    .ok = mmgr_spat_ok,
    .cok = mmgr_spat_cok,
    .has_storage = mmgr_spat_has_storage,
    .reset = mmgr_spat_reset,
    .after = mmgr_spat_after,
    .first = mmgr_spat_first,
    .produced = mmgr_spat_produced,
    .read = mmgr_spat_read,
};

MMGR_FINIS_DECLS

#endif
