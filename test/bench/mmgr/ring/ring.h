// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef PROTOCORE_RING_H
#define PROTOCORE_RING_H

#include "mmgr/rawmemcpy/rawmemcpy.h"
#include "mmgr/span/span.h"
#include <stdatomic.h>

#define PROTO_ATOMIC_LOAD(p) atomic_load_explicit((p), memory_order_acquire)

#define PROTO_ATOMIC_STORE(p, v) atomic_store_explicit((p), (v), memory_order_release)

#define PROTOCORE_RING_POW2(cap) (((cap) & ((cap) - 1)) == 0)

#define PROTOCORE_RING_WRAP(i, cap) ((i) & ((cap) - 1))

static inline size_t protocore_ring_available(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap)
{
    return PROTOCORE_RING_WRAP(PROTO_ATOMIC_LOAD(head) - PROTO_ATOMIC_LOAD(tail), cap);
}

static inline proto_bool protocore_ring_read_byte(const uint8_t *buf, size_t cap, const _Atomic size_t *head,
                                                  _Atomic size_t *tail, uint8_t *out)
{
    size_t t = PROTO_ATOMIC_LOAD(tail);
    if (t == PROTO_ATOMIC_LOAD(head))
    {
        return PROTO_FALSE;
    }
    *out = buf[t];
    PROTO_ATOMIC_STORE(tail, PROTOCORE_RING_WRAP(t + 1, cap));
    return PROTO_TRUE;
}

static inline size_t protocore_ring_read(const uint8_t *buf, size_t cap, const _Atomic size_t *head,
                                         _Atomic size_t *tail, uint8_t *dst, size_t maxn)
{
    size_t h = PROTO_ATOMIC_LOAD(head);
    size_t t = PROTO_ATOMIC_LOAD(tail);
    size_t n = 0;
    while (n < maxn && t != h)
    {
        dst[n] = buf[t];
        n++;
        t = PROTOCORE_RING_WRAP(t + 1, cap);
    }
    PROTO_ATOMIC_STORE(tail, t);
    return n;
}

static inline void protocore_ring_peek(const uint8_t *buf, size_t cap, const _Atomic size_t *tail, size_t off,
                                       uint8_t *dst, size_t n)
{
    size_t idx = PROTOCORE_RING_WRAP(PROTO_ATOMIC_LOAD(tail) + off, cap);
    for (size_t i = 0; i < n; i++)
    {
        dst[i] = buf[idx];
        idx = PROTOCORE_RING_WRAP(idx + 1, cap);
    }
}

static inline void protocore_ring_consume(_Atomic size_t *tail, size_t cap, size_t n)
{
    PROTO_ATOMIC_STORE(tail, PROTOCORE_RING_WRAP(PROTO_ATOMIC_LOAD(tail) + n, cap));
}

static inline size_t protocore_ring_free(const _Atomic size_t *head, const _Atomic size_t *tail, size_t cap)
{
    size_t used = PROTOCORE_RING_WRAP(PROTO_ATOMIC_LOAD(head) - PROTO_ATOMIC_LOAD(tail), cap);
    return (cap - 1) - used;
}

static inline size_t protocore_ring_write_span(uint8_t *buf, size_t cap, size_t head, const uint8_t *src, size_t len)
{
    while (len > 0)
    {
        size_t chunk = cap - head;
        if (chunk > len)
        {
            chunk = len;
        }
        proto_raw_read(&buf[head], src, chunk);
        head = PROTOCORE_RING_WRAP(head + chunk, cap);
        src += chunk;
        len -= chunk;
    }
    return head;
}

static inline size_t protocore_seg_inflight(const _Atomic size_t *claim, const _Atomic size_t *rel)
{
    return PROTO_ATOMIC_LOAD(claim) - PROTO_ATOMIC_LOAD(rel);
}

static inline proto_bool protocore_seg_next(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs,
                                            size_t *idx)
{
    size_t c = PROTO_ATOMIC_LOAD(claim);
    if ((c - PROTO_ATOMIC_LOAD(rel)) >= nsegs)
    {
        return PROTO_FALSE;
    }
    *idx = c & (nsegs - 1);
    return PROTO_TRUE;
}

static inline void protocore_seg_publish(_Atomic size_t *claim)
{
    PROTO_ATOMIC_STORE(claim, PROTO_ATOMIC_LOAD(claim) + 1);
}

static inline proto_bool protocore_seg_front(const _Atomic size_t *claim, const _Atomic size_t *rel, size_t nsegs,
                                             size_t *idx)
{
    size_t r = PROTO_ATOMIC_LOAD(rel);
    if (PROTO_ATOMIC_LOAD(claim) == r)
    {
        return PROTO_FALSE;
    }
    *idx = r & (nsegs - 1);
    return PROTO_TRUE;
}

static inline void protocore_seg_release(_Atomic size_t *rel)
{
    PROTO_ATOMIC_STORE(rel, PROTO_ATOMIC_LOAD(rel) + 1);
}

static inline uint8_t *protocore_seg_at(uint8_t *buf, size_t seg_size, size_t idx)
{
    return &buf[idx * seg_size];
}

#define PROTOCORE_RING_SLOTS_MAX 32

static inline uint32_t protocore_slot_bit(size_t idx)
{
    if (idx >= PROTOCORE_RING_SLOTS_MAX)
    {
        return 0u;
    }
    return 1u << idx;
}

static inline uint32_t protocore_slot_all(size_t count)
{
    if (count >= PROTOCORE_RING_SLOTS_MAX)
    {
        return 0xFFFFFFFFu;
    }
    return (1u << count) - 1u;
}

static inline proto_bool protocore_slot_take(_Atomic uint32_t *held, size_t idx)
{
    const uint32_t bit = protocore_slot_bit(idx);
    if (bit == 0u)
    {
        return PROTO_FALSE;
    }
    uint32_t prev = atomic_fetch_or_explicit(held, bit, memory_order_acquire);
    return (prev & bit) == 0u;
}

static inline proto_bool protocore_slot_hold(_Atomic uint32_t *held, protocore_cspan *keepout, size_t idx,
                                             const uint8_t *ptr, size_t len)
{
    if (!protocore_slot_take(held, idx))
    {
        return PROTO_FALSE;
    }
    keepout[idx].buf = ptr;
    keepout[idx].len = len;
    keepout[idx].pos = 0;
    keepout[idx].err = PROTO_FALSE;
    return PROTO_TRUE;
}

static inline const protocore_cspan *protocore_slot_keepout(const protocore_cspan *keepout, size_t idx)
{
    return &keepout[idx];
}

static inline void protocore_slot_drop(_Atomic uint32_t *held, size_t idx)
{
    atomic_fetch_and_explicit(held, ~protocore_slot_bit(idx), memory_order_release);
}

static inline void protocore_slot_mark(_Atomic uint32_t *mask, size_t idx)
{
    atomic_fetch_or_explicit(mask, protocore_slot_bit(idx), memory_order_release);
}

static inline void protocore_slot_clear(_Atomic uint32_t *mask, size_t idx)
{
    atomic_fetch_and_explicit(mask, ~protocore_slot_bit(idx), memory_order_release);
}

static inline uint32_t protocore_slot_ready(const _Atomic uint32_t *mask, const _Atomic uint32_t *held, size_t count)
{
    return PROTO_ATOMIC_LOAD(mask) & ~PROTO_ATOMIC_LOAD(held) & protocore_slot_all(count);
}

static inline int32_t protocore_slot_next(uint32_t m)
{
    if (m == 0u)
    {
        return -1;
    }
    return (int32_t)__builtin_ctz(m);
}

#endif
