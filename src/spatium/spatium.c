/**
 * @brief Spans over a caller's buffer: the two constructors and the walks over them.
 *
 * @note Holds no storage of its own, and every entry returns by value rather than through a pointer.
 * @note Reaches nothing outside config.
 */
#include "spatium/spatium.h"

/**
 * @brief Builds a fill span over buf, with pos at 0 and overflow clear.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_span mmgr_spat_from(uint8_t *buf, size_t cap)
{
    MMGR_ASSERT(buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(cap != 0u, "a span needs a capacity");

    mmgr_span s;

    s.buf = buf;
    s.cap = cap;
    s.pos = 0u;
    s.overflow = MMGR_FALSE;
    return s;
}

/**
 * @brief Builds a read span over buf, with pos at 0 and err clear.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_cspan mmgr_spat_cfrom(const uint8_t *buf, size_t len)
{
    mmgr_cspan s;

    s.buf = buf;
    s.len = len;
    s.pos = 0u;
    s.err = MMGR_FALSE;
    return s;
}

/**
 * @brief Returns whether s covers any bytes at all.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_bool mmgr_spat_has_storage(mmgr_span s)
{
    return (mmgr_bool)((s.buf != NULL) && (s.cap != 0u));
}

/**
 * @brief Returns whether s is still usable.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_bool mmgr_spat_ok(mmgr_span s)
{
    return (mmgr_bool)(mmgr_spat_has_storage(s) && !s.overflow);
}

/**
 * @brief Returns whether s is still usable.
 *
 * @note The read side of mmgr_spat_ok. The two cannot share a body: a read span names its extent
 *       len and a fill span names it cap, which is what keeps the two from being mixed up.
 * @note Documented at the declaration in spatium.h.
 */
mmgr_bool mmgr_spat_cok(mmgr_cspan s)
{
    return (mmgr_bool)((s.buf != NULL) && (s.len != 0u) && !s.err);
}

/**
 * @brief Returns s to its start and clears its overflow.
 *
 * @note Documented at the declaration in spatium.h.
 */
void mmgr_spat_reset(mmgr_span *s)
{
    s->pos = 0u;
    s->overflow = MMGR_FALSE;
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
 * @brief Returns the span beginning off bytes into s.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_span mmgr_spat_after(mmgr_span s, size_t off)
{
    mmgr_span r;

    if (off > s.cap)
    {
        return spat_failed();
    }
    r.buf = (s.buf != NULL) ? (s.buf + off) : NULL;
    r.cap = s.cap - off;
    r.pos = (s.pos > off) ? (s.pos - off) : 0u;
    r.overflow = s.overflow;
    return r;
}

/**
 * @brief Returns the span covering only the first n bytes of s.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_span mmgr_spat_first(mmgr_span s, size_t n)
{
    mmgr_span r;

    if (n > s.cap)
    {
        return spat_failed();
    }
    r.buf = s.buf;
    r.cap = n;
    r.pos = (s.pos < n) ? s.pos : n;
    r.overflow = s.overflow;
    return r;
}

/**
 * @brief Returns a read span over the first len bytes written into s.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_cspan mmgr_spat_read(mmgr_span s, size_t len)
{
    mmgr_cspan r;

    r.buf = s.buf;
    r.len = (len < s.pos) ? len : s.pos;
    r.pos = 0u;
    // A span that overflowed produced fewer bytes than were asked of it, and the read side is told
    // so rather than being handed a shorter span that looks whole
    r.err = (mmgr_bool)(s.overflow || (len > s.pos));
    return r;
}

/**
 * @brief Returns a read span over everything written into s.
 *
 * @note Documented at the declaration in spatium.h.
 */
mmgr_cspan mmgr_spat_produced(mmgr_span s)
{
    return mmgr_spat_read(s, s.pos);
}
