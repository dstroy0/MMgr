// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "spatium/spatium.h"

/**
 * @file spatium.c
 * @brief Bounded views over caller memory. A span owns nothing.
 *
 * Every entry below takes one parameter, a pointer to SpatCtx. Making a span is a buffer and how
 * big it is, so those are one context.
 *
 * The context is file local. What a span is built from is nobody else's type: the header declares
 * the entry with the caller's own argument list, and the aggregate exists only between the entry
 * and the body, where the compound literal in MMGR_CALL folds it away.
 */

/** @brief One span being built. */
typedef struct
{
    uint8_t *buf; /**< The memory the span will view. */
    size_t cap;   /**< Its size. */
} SpatCtx;

/**
 * @brief Check the context is one a span can be made from.
 * @param c The span being built.
 *
 * A null buffer or a zero capacity is a caller that has not decided what it is writing into, which
 * is a program that should not have been built rather than a state to hand back. MMGR_ASSERT says
 * so: nothing in a shipping build, an abort in the checks build.
 */
MMGR_INLINE void spat_check(const SpatCtx *c)
{
    MMGR_ASSERT(c->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(c->cap != 0, "a span needs a capacity");
}

/**
 * @brief Build the span.
 * @param c The span being built.
 * @return The span, empty.
 */
MMGR_INLINE mmgr_spat spat_from(const SpatCtx *c)
{
    spat_check(c);

    mmgr_spat s;
    s.buf = c->buf;
    s.cap = c->cap;
    s.pos = 0;
    return s;
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each builds the context and hands it to the body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

mmgr_spat mmgr_spat_from(uint8_t *buf, size_t cap)
{
    return MMGR_CALL(spat_from, SpatCtx, .buf = buf, .cap = cap);
}
