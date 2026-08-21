// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H
#define MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H

#include "proximus_operor/proximus_operor.h"
#include "spatium/spatium.h"

#include "config/mmgr_config.h"

#include <stdatomic.h>

MMGR_INCIPE_DECLS

/**
 * @file confinium_exclusivum_infinitas.h
 * @brief Lock free rings, segment queues and loculus bitmaps.
 *
 * Single producer, single consumer. Capacities are powers of two so a wrap is a mask.
 *
 * The tables are the whole surface. There are no free functions to call.
 *
 * Three tables, not one, because this header holds three separate things: a byte ring, a queue of
 * segments over one, and a bitmap of loculi. They share a file because they share the lock free
 * discipline, not because a caller of one is a caller of the others - and the twenty four entries
 * between them are exactly what one MMGR_NS_LAYOUT can hold, which is no margin at all.
 *
 * Every entry is defined here rather than in a .c because each is a handful of atomic operations
 * and a call frame costs more than the body. They call each other by name: a table is not formed
 * until below, so a body above it has nothing to dispatch through yet.
 */

/** @brief Acquire load. */
#define MMGR_ATOMIC_LOAD(p) atomic_load_explicit((p), memory_order_acquire)

/** @brief Release store. */
#define MMGR_ATOMIC_STORE(p, v) atomic_store_explicit((p), (v), memory_order_release)

/** @brief Is a capacity a power of two. */
#define MMGR_RING_POW2(cap) (((cap) & ((cap) - 1)) == 0)

/** @brief Wrap an index. Capacity must be a power of two. */
#define MMGR_RING_WRAP(i, cap) ((i) & ((cap) - 1))

/**
 * @brief How many bytes are readable.
 * @param head Producer cursor.
 * @param tail Consumer cursor.
 * @param cap Ring capacity.
 * @return Byte count.
 */
MMGR_INLINE size_t mmgr_infin_available(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap)
{
    return MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(head) - MMGR_ATOMIC_LOAD(tail), cap);
}

/**
 * @brief Take one byte.
 * @param buf Ring.
 * @param cap Its capacity.
 * @param head Producer cursor.
 * @param tail Consumer cursor.
 * @param out Out. The byte.
 * @return MMGR_FALSE if the ring is empty.
 */
MMGR_INLINE mmgr_bool mmgr_infin_read_byte(const uint8_t *buf, size_t cap, const _Atomic size_t *head,
                                             _Atomic size_t *tail, uint8_t *out)
{
    size_t t = MMGR_ATOMIC_LOAD(tail);
    if (t == MMGR_ATOMIC_LOAD(head))
    {
        return MMGR_FALSE;
    }
    *out = buf[t];
    MMGR_ATOMIC_STORE(tail, MMGR_RING_WRAP(t + 1, cap));
    return MMGR_TRUE;
}

/**
 * @brief Take up to @p maxn bytes.
 * @param buf Ring.
 * @param cap Its capacity.
 * @param head Producer cursor.
 * @param tail Consumer cursor.
 * @param dst Destination.
 * @param maxn Most to take.
 * @return How many were taken.
 */
MMGR_INLINE size_t mmgr_infin_read(const uint8_t *buf, size_t cap, const _Atomic size_t *head, _Atomic size_t *tail,
                                     uint8_t *dst, size_t maxn)
{
    size_t h = MMGR_ATOMIC_LOAD(head);
    size_t t = MMGR_ATOMIC_LOAD(tail);
    size_t n = 0;
    while (n < maxn && t != h)
    {
        dst[n] = buf[t];
        n++;
        t = MMGR_RING_WRAP(t + 1, cap);
    }
    MMGR_ATOMIC_STORE(tail, t);
    return n;
}

/**
 * @brief Copy bytes without consuming them.
 * @param buf Ring.
 * @param cap Its capacity.
 * @param tail Consumer cursor.
 * @param off Where to start, from the tail.
 * @param dst Destination.
 * @param n Byte count.
 */
MMGR_INLINE void mmgr_infin_peek(const uint8_t *buf, size_t cap, const _Atomic size_t *tail, size_t off, uint8_t *dst,
                                   size_t n)
{
    size_t idx = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(tail) + off, cap);
    for (size_t i = 0; i < n; i++)
    {
        dst[i] = buf[idx];
        idx = MMGR_RING_WRAP(idx + 1, cap);
    }
}

/**
 * @brief Drop @p n bytes.
 * @param tail Consumer cursor.
 * @param cap Ring capacity.
 * @param n Byte count.
 */
MMGR_INLINE void mmgr_infin_consume(_Atomic size_t *tail, size_t cap, size_t n)
{
    MMGR_ATOMIC_STORE(tail, MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(tail) + n, cap));
}

/**
 * @brief How many bytes are writable.
 * @param head Producer cursor.
 * @param tail Consumer cursor.
 * @param cap Ring capacity.
 * @return Byte count. One loculus is always held back so full and empty differ.
 */
MMGR_INLINE size_t mmgr_infin_free(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap)
{
    size_t used = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(head) - MMGR_ATOMIC_LOAD(tail), cap);
    return (cap - 1) - used;
}

/**
 * @brief Write @p len bytes, wrapping as needed.
 * @param buf Ring.
 * @param cap Its capacity.
 * @param head Producer cursor.
 * @param src Source.
 * @param len Byte count.
 * @return The new head.
 */
MMGR_INLINE size_t mmgr_infin_write_span(uint8_t *buf, size_t cap, size_t head, const uint8_t *src, size_t len)
{
    while (len > 0)
    {
        size_t chunk = cap - head;
        if (chunk > len)
        {
            chunk = len;
        }
        proxim.read(&buf[head], src, chunk);
        head = MMGR_RING_WRAP(head + chunk, cap);
        src += chunk;
        len -= chunk;
    }
    return head;
}

/**
 * @brief How many segments are claimed and not yet released.
 * @param claim Claim cursor.
 * @param rel Release cursor.
 * @return Segment count.
 */
MMGR_INLINE size_t mmgr_seg_inflight(const _Atomic size_t *claim, const _Atomic size_t *rel)
{
    return MMGR_ATOMIC_LOAD(claim) - MMGR_ATOMIC_LOAD(rel);
}

/**
 * @brief Claim the next segment.
 * @param claim Claim cursor.
 * @param rel Release cursor.
 * @param nsegs Segment count, a power of two.
 * @param idx Out. Which segment.
 * @return MMGR_FALSE if all segments are in flight.
 */
MMGR_INLINE mmgr_bool mmgr_seg_next(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs, size_t *idx)
{
    size_t c = MMGR_ATOMIC_LOAD(claim);
    if ((c - MMGR_ATOMIC_LOAD(rel)) >= nsegs)
    {
        return MMGR_FALSE;
    }
    *idx = c & (nsegs - 1);
    return MMGR_TRUE;
}

/**
 * @brief Make the claimed segment visible to the consumer.
 * @param claim Claim cursor.
 */
MMGR_INLINE void mmgr_seg_publish(_Atomic size_t *claim)
{
    MMGR_ATOMIC_STORE(claim, MMGR_ATOMIC_LOAD(claim) + 1);
}

/**
 * @brief The oldest unreleased segment.
 * @param claim Claim cursor.
 * @param rel Release cursor.
 * @param nsegs Segment count, a power of two.
 * @param idx Out. Which segment.
 * @return MMGR_FALSE if there is none.
 */
MMGR_INLINE mmgr_bool mmgr_seg_front(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs,
                                       size_t *idx)
{
    size_t r = MMGR_ATOMIC_LOAD(rel);
    if (MMGR_ATOMIC_LOAD(claim) == r)
    {
        return MMGR_FALSE;
    }
    *idx = r & (nsegs - 1);
    return MMGR_TRUE;
}

/**
 * @brief Release the oldest segment.
 * @param rel Release cursor.
 */
MMGR_INLINE void mmgr_seg_release(_Atomic size_t *rel)
{
    MMGR_ATOMIC_STORE(rel, MMGR_ATOMIC_LOAD(rel) + 1);
}

/**
 * @brief Where segment @p idx starts.
 * @param buf Segment store.
 * @param seg_size Bytes per segment.
 * @param idx Segment index.
 * @return Pointer to it.
 */
MMGR_INLINE uint8_t *mmgr_seg_at(uint8_t *buf, size_t seg_size, size_t idx)
{
    return &buf[idx * seg_size];
}

/** @brief Most loculi a bitmap can track. */
#define MMGR_RING_LOCULI_MAX 32

/**
 * @brief Bit for loculus @p idx.
 * @param idx Loculus index.
 * @return The bit, or 0 if out of range.
 */
MMGR_INLINE uint32_t mmgr_loculus_bit(size_t idx)
{
    if (idx >= MMGR_RING_LOCULI_MAX)
    {
        return 0u;
    }
    return 1u << idx;
}

/**
 * @brief Mask of the low @p count loculi.
 * @param count Loculus count.
 * @return The mask.
 */
MMGR_INLINE uint32_t mmgr_loculus_all(size_t count)
{
    if (count >= MMGR_RING_LOCULI_MAX)
    {
        return 0xFFFFFFFFu;
    }
    return (1u << count) - 1u;
}

/**
 * @brief Claim a loculus.
 * @param held Held bitmap.
 * @param idx Loculus index.
 * @return MMGR_FALSE if it was already held.
 */
MMGR_INLINE mmgr_bool mmgr_loculus_take(_Atomic uint32_t *held, size_t idx)
{
    const uint32_t bit = mmgr_loculus_bit(idx);
    if (bit == 0u)
    {
        return MMGR_FALSE;
    }
    uint32_t prev = atomic_fetch_or_explicit(held, bit, memory_order_acquire);
    return (prev & bit) == 0u;
}

/** @brief A region a loculus is holding off. Two values, which is what was ever written or read. */
typedef struct
{
    const uint8_t *buf;
    size_t len;
} mmgr_keepout;

/**
 * @brief Claim a loculus and bind a region to it.
 * @param held Held bitmap.
 * @param keepout Region per loculus.
 * @param idx Loculus index.
 * @param ptr Data.
 * @param len Its length.
 * @return MMGR_FALSE if the loculus was already held.
 */
MMGR_INLINE mmgr_bool mmgr_loculus_hold(_Atomic uint32_t *held, mmgr_keepout *keepout, size_t idx,
                                       const uint8_t *ptr, size_t len)
{
    if (!mmgr_loculus_take(held, idx))
    {
        return MMGR_FALSE;
    }
    keepout[idx].buf = ptr;
    keepout[idx].len = len;
    return MMGR_TRUE;
}

/**
 * @brief The span bound to a loculus.
 * @param keepout Region per loculus.
 * @param idx Loculus index.
 * @return The region.
 */
MMGR_INLINE const mmgr_keepout *mmgr_loculus_keepout(const mmgr_keepout *keepout, size_t idx)
{
    return &keepout[idx];
}

/**
 * @brief Release a loculus.
 * @param held Held bitmap.
 * @param idx Loculus index.
 */
MMGR_INLINE void mmgr_loculus_drop(_Atomic uint32_t *held, size_t idx)
{
    atomic_fetch_and_explicit(held, ~mmgr_loculus_bit(idx), memory_order_release);
}

/**
 * @brief Mark a loculus ready.
 * @param mask Ready bitmap.
 * @param idx Loculus index.
 */
MMGR_INLINE void mmgr_loculus_mark(_Atomic uint32_t *mask, size_t idx)
{
    atomic_fetch_or_explicit(mask, mmgr_loculus_bit(idx), memory_order_release);
}

/**
 * @brief Unmark a loculus.
 * @param mask Ready bitmap.
 * @param idx Loculus index.
 */
MMGR_INLINE void mmgr_loculus_clear(_Atomic uint32_t *mask, size_t idx)
{
    atomic_fetch_and_explicit(mask, ~mmgr_loculus_bit(idx), memory_order_release);
}

/**
 * @brief Loculi that are ready and not held.
 * @param mask Ready bitmap.
 * @param held Held bitmap.
 * @param count Loculus count.
 * @return The mask.
 */
MMGR_INLINE uint32_t mmgr_loculus_ready(const _Atomic uint32_t *mask, const _Atomic uint32_t *held, size_t count)
{
    return MMGR_ATOMIC_LOAD(mask) & ~MMGR_ATOMIC_LOAD(held) & mmgr_loculus_all(count);
}

/**
 * @brief Index of the lowest set bit.
 * @param m Bitmap, non-zero.
 * @return Bit index.
 *
 * (m - 1) & ~m is the trailing zero run, so its population count is the index. The fold is not a
 * concession: __builtin_popcount is a call to __popcountdi2 on baseline x86-64, measured at 10.392
 * cycles against 5.731 for this, and it does not link at all on a freestanding target.
 */
MMGR_INLINE int32_t mmgr_loculus_ctz(uint32_t m)
{
    uint32_t v = (m - 1u) & ~m;
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    v = (v + (v >> 4)) & 0x0F0F0F0Fu;
    return (int32_t)((v * 0x01010101u) >> 24);
}

/**
 * @brief Lowest set loculus.
 * @param m Bitmap.
 * @return Loculus index, or -1 if none.
 */
MMGR_INLINE int32_t mmgr_loculus_next(uint32_t m)
{
    if (m == 0u)
    {
        return -1;
    }
    return mmgr_loculus_ctz(m);
}

/** @brief Ring dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*available)(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap);
    mmgr_bool (*read_byte)(const uint8_t *buf, size_t cap, const _Atomic size_t *head, _Atomic size_t *tail,
                           uint8_t *out);
    size_t (*read)(const uint8_t *buf, size_t cap, const _Atomic size_t *head, _Atomic size_t *tail, uint8_t *dst,
                   size_t maxn);
    void (*peek)(const uint8_t *buf, size_t cap, const _Atomic size_t *tail, size_t off, uint8_t *dst, size_t n);
    void (*consume)(_Atomic size_t *tail, size_t cap, size_t n);
    size_t (*free_)(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap);
    size_t (*write_span)(uint8_t *buf, size_t cap, size_t head, const uint8_t *src, size_t len);
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, available, read_byte, read, peek, consume, free_, write_span);

/**
 * @brief Ring namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 *
 * free_ carries the underscore for the reason xor_ does: spelled bare, a member access reading
 * `infin.free(...)` is a call to whatever a freestanding libc defined free as, and a
 * function-like macro expands wherever its name is followed by an open parenthesis - the member
 * access in front does not stop it.
 */
MMGR_NS InfinitasNs infin MMGR_UNUSED = {
    .available = mmgr_infin_available,
    .read_byte = mmgr_infin_read_byte,
    .read = mmgr_infin_read,
    .peek = mmgr_infin_peek,
    .consume = mmgr_infin_consume,
    .free_ = mmgr_infin_free,
    .write_span = mmgr_infin_write_span,
};

/** @brief Segment dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*inflight)(const _Atomic size_t *claim, const _Atomic size_t *rel);
    mmgr_bool (*next)(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs, size_t *idx);
    void (*publish)(_Atomic size_t *claim);
    mmgr_bool (*front)(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs, size_t *idx);
    void (*release)(_Atomic size_t *rel);
    uint8_t *(*at)(uint8_t *buf, size_t seg_size, size_t idx);
} SegmentumNs;
MMGR_NS_LAYOUT(SegmentumNs, inflight, next, publish, front, release, at);

/** @brief Segment namespace. static const, for the same reason the ring's is. */
MMGR_NS SegmentumNs seg MMGR_UNUSED = {
    .inflight = mmgr_seg_inflight,
    .next = mmgr_seg_next,
    .publish = mmgr_seg_publish,
    .front = mmgr_seg_front,
    .release = mmgr_seg_release,
    .at = mmgr_seg_at,
};

/** @brief Loculus dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    uint32_t (*bit)(size_t idx);
    uint32_t (*all)(size_t count);
    mmgr_bool (*take)(_Atomic uint32_t *held, size_t idx);
    mmgr_bool (*hold)(_Atomic uint32_t *held, mmgr_keepout *keepout, size_t idx, const uint8_t *ptr, size_t len);
    const mmgr_keepout *(*keepout)(const mmgr_keepout *keepout, size_t idx);
    void (*drop)(_Atomic uint32_t *held, size_t idx);
    void (*mark)(_Atomic uint32_t *mask, size_t idx);
    void (*clear)(_Atomic uint32_t *mask, size_t idx);
    uint32_t (*ready)(const _Atomic uint32_t *mask, const _Atomic uint32_t *held, size_t count);
    int32_t (*ctz)(uint32_t m);
    int32_t (*next)(uint32_t m);
} LoculusNs;
MMGR_NS_LAYOUT(LoculusNs, bit, all, take, hold, keepout, drop, mark, clear, ready, ctz, next);

/** @brief Loculus namespace. static const, for the same reason the ring's is. */
MMGR_NS LoculusNs loculus MMGR_UNUSED = {
    .bit = mmgr_loculus_bit,
    .all = mmgr_loculus_all,
    .take = mmgr_loculus_take,
    .hold = mmgr_loculus_hold,
    .keepout = mmgr_loculus_keepout,
    .drop = mmgr_loculus_drop,
    .mark = mmgr_loculus_mark,
    .clear = mmgr_loculus_clear,
    .ready = mmgr_loculus_ready,
    .ctz = mmgr_loculus_ctz,
    .next = mmgr_loculus_next,
};

MMGR_FINIS_DECLS

#endif
