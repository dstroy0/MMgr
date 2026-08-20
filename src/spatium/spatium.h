// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file spatium.h
 * @brief Bounded views over caller memory. A span owns nothing and never allocates.
 *
 * Overflow latches. Once a write runs out of room the flag stays set, so a long run of appends is
 * checked once at the end rather than after every call.
 */

/**
 * @brief Writable span.
 *
 * @c pos is how much has been written. @c overflow latches and is never cleared.
 */
typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
    mmgr_bool overflow;
} mmgr_spat;

/**
 * @brief Read-only span.
 *
 * @c pos is how much has been consumed. @c err latches on a short read.
 */
typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;
    mmgr_bool err;
} mmgr_fspat;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_spat (*from)(uint8_t *p, size_t cap);
    mmgr_bool (*ok)(mmgr_spat s);
    mmgr_bool (*has_storage)(mmgr_spat s);
    size_t (*len)(mmgr_spat s);
    size_t (*room)(mmgr_spat s);
    void (*reset)(mmgr_spat *s);
    mmgr_spat (*after)(mmgr_spat s, size_t off);
    mmgr_spat (*first)(mmgr_spat s, size_t n);
    mmgr_fspat (*produced)(mmgr_spat s);
    mmgr_fspat (*read)(mmgr_spat s, size_t len);
    mmgr_fspat (*cfrom)(const uint8_t *p, size_t len);
    mmgr_bool (*cok)(mmgr_fspat s);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, from, ok, has_storage, len, room, reset, after, first, produced, read, cfrom, cok);

/**
 * @brief Wrap a buffer.
 * @param p Buffer. May be NULL for a sizing pass.
 * @param cap Its size.
 * @return The span, empty.
 */
mmgr_spat mmgr_spat_from(uint8_t *p, size_t cap);
/**
 * @brief Has everything written so far fit.
 * @param s Span.
 * @return MMGR_FALSE once overflow has latched.
 */
mmgr_bool mmgr_spat_ok(mmgr_spat s);
/**
 * @brief Does the span point at real memory.
 * @param s Span.
 * @return MMGR_FALSE for a sizing pass.
 */
mmgr_bool mmgr_spat_has_storage(mmgr_spat s);
/**
 * @brief How much has been written.
 * @param s Span.
 * @return Byte count.
 */
size_t mmgr_spat_len(mmgr_spat s);
/**
 * @brief How much is left.
 * @param s Span.
 * @return Byte count.
 */
size_t mmgr_spat_room(mmgr_spat s);
/**
 * @brief Rewind to empty and clear overflow.
 * @param s Span.
 */
void mmgr_spat_reset(mmgr_spat *s);
/**
 * @brief The span starting @p off bytes in.
 * @param s Span.
 * @param off Offset.
 * @return The tail.
 */
mmgr_spat mmgr_spat_after(mmgr_spat s, size_t off);
/**
 * @brief The first @p n bytes of the span.
 * @param s Span.
 * @param n Byte count.
 * @return The head.
 */
mmgr_spat mmgr_spat_first(mmgr_spat s, size_t n);
/**
 * @brief A reader over what has been written.
 * @param s Span.
 * @return Read-only span.
 */
mmgr_fspat mmgr_spat_produced(mmgr_spat s);
/**
 * @brief A reader over the first @p len bytes.
 * @param s Span.
 * @param len Byte count.
 * @return Read-only span.
 */
mmgr_fspat mmgr_spat_read(mmgr_spat s, size_t len);
/**
 * @brief Wrap a read-only buffer.
 * @param p Buffer.
 * @param len Its length.
 * @return The span, unconsumed.
 */
mmgr_fspat mmgr_fspat_from(const uint8_t *p, size_t len);
/**
 * @brief Has every read so far fit.
 * @param s Span.
 * @return MMGR_FALSE once a short read has latched.
 */
mmgr_bool mmgr_fspat_ok(mmgr_fspat s);

/** @brief Module namespace. */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {.from = mmgr_spat_from,
                                                       .ok = mmgr_spat_ok,
                                                       .has_storage = mmgr_spat_has_storage,
                                                       .len = mmgr_spat_len,
                                                       .room = mmgr_spat_room,
                                                       .reset = mmgr_spat_reset,
                                                       .after = mmgr_spat_after,
                                                       .first = mmgr_spat_first,
                                                       .produced = mmgr_spat_produced,
                                                       .read = mmgr_spat_read,
                                                       .cfrom = mmgr_fspat_from,
                                                       .cok = mmgr_fspat_ok};

MMGR_END_DECLS

#endif
