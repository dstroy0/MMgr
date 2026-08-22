// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include <stdatomic.h>

/**
 * @file confinium_exclusivum_infinitas.c
 * @brief A lock free ring, and the cursors and drains it hands out over it.
 *
 * Every entry below takes one parameter, a pointer to InfinCtx. The ring, whatever is being moved
 * and what that move needs are one context.
 *
 * <stdatomic.h> is this file's problem and nobody else's. A read modify write on the reservation
 * word has to be indivisible rather than merely ordered: deferred work landing between a load and a
 * store loses whatever that work wrote, and one core is enough for that.
 *
 * A cursor is an offset, never a pointer. The base is held once and an address is materialised only
 * at the moment one is handed out, so there is no stored pointer for anything to advance out from
 * under the ring.
 *
 * One bit per segment, all of them in one machine word. That is why the reservations cost a load
 * and a mask however many are outstanding, and it is why the count of segments cannot exceed the
 * width of the word - a bit that does not exist cannot reserve anything.
 *
 * A drain is granted a segment at a time and the grant is the check: the ring works out the bounds
 * and the liveness once, hands back the address, and the worker owns that run until it comes back
 * for the next one. It cannot overrun, because the run it was given is the whole of its permission.
 * NULL is how it learns the frame is finished, and that same NULL is when the ring pulls the
 * reservation and hands the ordinary cursor back to whoever it came from.
 */

/** @brief Acquire load. */
#define MMGR_ATOMIC_LOAD(p) atomic_load_explicit((p), memory_order_acquire)

/** @brief Release store. */
#define MMGR_ATOMIC_STORE(p, v) atomic_store_explicit((p), (v), memory_order_release)

/**
 * @brief Give back exactly @p bits of a reservation word.
 *
 * A read modify write, not a load and a store. Clearing by loading, masking and storing loses any
 * clear that landed in between - which is the whole reason the word is atomic, and it is not a race
 * a single threaded test can find: every claim and retirement passes on its own and the bits leak
 * only when two of them overlap.
 */
#define MMGR_ATOMIC_CLEAR(p, bits) \
    ((void)atomic_fetch_and_explicit((p), (mmgr_word) ~(bits), memory_order_release))

/** @brief A cursor over a ring. The frame is set when it is handed out and does not move again. */
struct MmgrCursor
{
    size_t base; /**< First byte of the frame, as a ring offset. */
    size_t span; /**< How long the frame is. */
    size_t off;  /**< How far into the frame this cursor has reached. */
};

/** @brief One drain: a run of segments, how far through it the worker has got, and which use. */
typedef struct
{
    size_t first; /**< First segment of the run. */
    size_t last;  /**< One past its last. */
    size_t next;  /**< The segment the next grant hands out. */
    size_t gen;   /**< Bumped on retire, so a token from a finished drain stops matching. */
} Drain;

/** @brief A tessera is which record, and which use of it. */
#define MMGR_TESSERA(idx, gen) (((gen) * MMGR_RING_DRAINS) + (idx) + 1u)

/** @brief Which record a tessera names. */
#define MMGR_TESSERA_IDX(t) ((((t) - 1u)) % MMGR_RING_DRAINS)

/** @brief Which use of that record it was issued for. */
#define MMGR_TESSERA_GEN(t) ((((t) - 1u)) / MMGR_RING_DRAINS)

/** @brief What the opaque ring handle holds. */
typedef struct
{
    uint8_t *buf;            /**< The consumer's buffer. */
    size_t cap;              /**< Its capacity, a power of two. */
    size_t nsegs;            /**< Segments it divides into, a power of two. */
    size_t seg;              /**< Bytes per segment. */
    _Atomic mmgr_word *held; /**< One bit per segment, all in one word. */
    _Atomic size_t head;     /**< Producer cursor. */
    _Atomic size_t tail;     /**< Consumer cursor. */
    struct MmgrCursor ord;   /**< The ordinary cursor. */
    const void *owner;       /**< Who it was handed to. */
    int open;                /**< Whether the ordinary cursor is out. */
    _Atomic mmgr_word slots; /**< One bit per drain slot that is taken. */
    Drain drains[MMGR_RING_DRAINS];
} RingState;

MMGR_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

/** @brief The state behind a handle. */
MMGR_INLINE RingState *ring_of(mmgr_ring *r)
{
    return (RingState *)(void *)r->opaque;
}

/** @brief One ring operation. */
typedef struct
{
    RingState *s;           /**< The ring. */
    struct MmgrCursor *cur; /**< The cursor being moved. */
    uint8_t *dst;           /**< Where bytes are taken to. */
    const uint8_t *src;     /**< Where bytes are written from. */
    size_t n;               /**< A byte count, or a most-to-take. */
    size_t off;             /**< An offset, for peek and seek. */
    size_t from;            /**< First byte of a drain. */
    size_t to;              /**< One past its last. */
    size_t *tessera;        /**< In/out. The token the ring issued for this drain. */
    const void *owner;      /**< Who the ordinary cursor goes back to. */
} InfinCtx;

/** @brief How many bytes are readable. */
MMGR_INLINE size_t infin_available(const InfinCtx *c)
{
    return MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->head) - MMGR_ATOMIC_LOAD(&c->s->tail), c->s->cap);
}

/** @brief How many bytes are writable. One is held back so full and empty differ. */
MMGR_INLINE size_t infin_free(const InfinCtx *c)
{
    return (c->s->cap - 1u) - infin_available(c);
}

/** @brief Take one byte. */
MMGR_INLINE mmgr_bool infin_read_byte(const InfinCtx *c)
{
    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    if (t == MMGR_ATOMIC_LOAD(&c->s->head))
    {
        return MMGR_FALSE;
    }
    *c->dst = c->s->buf[t];
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(t + 1u, c->s->cap));
    return MMGR_TRUE;
}

/**
 * @brief A raw read: where @c n readable bytes are, or NULL.
 *
 * It behaves like the other memory entries - it hands back a pointer and changes nothing. The tail
 * does not move here, because the caller is still looking at what the pointer names; consume is
 * what says it is finished.
 *
 * NULL when the bytes cannot be named: the ring is empty, fewer than @c n are readable, or the run
 * asked for wraps the end and no one pointer covers it.
 */
MMGR_INLINE const uint8_t *infin_read(const InfinCtx *c)
{
    const size_t have = infin_available(c);
    if ((have == 0u) || (c->n > have))
    {
        return NULL;
    }

    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    if (c->n > (c->s->cap - t))
    {
        return NULL;
    }
    return &c->s->buf[t];
}

/** @brief Copy bytes without consuming them. */
MMGR_INLINE void infin_peek(const InfinCtx *c)
{
    size_t at = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->off, c->s->cap);

    for (size_t i = 0; i < c->n; i++)
    {
        c->dst[i] = c->s->buf[at];
        at = MMGR_RING_WRAP(at + 1u, c->s->cap);
    }
}

/** @brief Drop @c n bytes. */
MMGR_INLINE void infin_consume(const InfinCtx *c)
{
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->n, c->s->cap));
}

/** @brief Write @c n bytes, wrapping as needed. */
MMGR_INLINE size_t infin_write(const InfinCtx *c)
{
    size_t at = MMGR_ATOMIC_LOAD(&c->s->head);
    const uint8_t *src = c->src;
    size_t left = c->n;

    while (left > 0u)
    {
        size_t chunk = c->s->cap - at;
        if (chunk > left)
        {
            chunk = left;
        }
        proxim.read(&c->s->buf[at], src, chunk);
        at = MMGR_RING_WRAP(at + chunk, c->s->cap);
        src += chunk;
        left -= chunk;
    }
    MMGR_ATOMIC_STORE(&c->s->head, at);
    return c->n;
}

/** @brief Move a cursor inside its frame. Offset zero is the frame's start. */
MMGR_INLINE void infin_seek(const InfinCtx *c)
{
    MMGR_ASSERT(c->off <= c->cur->span, "a cursor cannot be moved outside its frame");
    c->cur->off = c->off;
}

/** @brief The bits for segments @c first through @c last. */
MMGR_INLINE mmgr_word seg_mask(size_t first, size_t last)
{
    mmgr_word m = 0;

    for (size_t i = first; i < last; i++)
    {
        m |= (mmgr_word)((mmgr_word)1 << i);
    }
    return m;
}

/** @brief The drain a tessera entitles the holder to, or NULL if it does not. */
MMGR_INLINE Drain *drain_of(RingState *s, size_t tessera)
{
    if (tessera == 0u)
    {
        return NULL;
    }

    const size_t idx = MMGR_TESSERA_IDX(tessera);
    Drain *const d = &s->drains[idx];

    if ((d->first == d->last) || (d->gen != MMGR_TESSERA_GEN(tessera)))
    {
        return NULL;
    }
    return d;
}

/** @brief Give the ordinary cursor back to whoever it came from, the frame being finished. */
MMGR_INLINE void drain_retire(RingState *s, Drain *d)
{
    const size_t slot = (size_t)(d - &s->drains[0]);

    MMGR_ATOMIC_CLEAR(s->held, seg_mask(d->first, d->last));
    d->first = 0u;
    d->last = 0u;
    d->next = 0u;
    d->gen++;
    MMGR_ATOMIC_CLEAR(&s->slots, (mmgr_word)((mmgr_word)1 << slot));
}

mmgr_bool mmgr_infin_init(mmgr_ring *r, const RingCfg *c)
{
    MMGR_ASSERT(r != NULL, "a ring needs storage");
    MMGR_ASSERT(c->buf != NULL, "a ring needs a buffer");

    if ((c->cap == 0u) || !MMGR_RING_POW2(c->cap))
    {
        return MMGR_FALSE;
    }
    if ((c->nsegs == 0u) || !MMGR_RING_POW2(c->nsegs) || (c->nsegs > c->cap))
    {
        return MMGR_FALSE;
    }
    /* One bit per segment, and the bits are one word. A segment with no bit cannot be reserved. */
    if (c->nsegs > (size_t)MMGR_RING_LOCULI_MAX)
    {
        return MMGR_FALSE;
    }

    RingState *const s = ring_of(r);
    s->buf = c->buf;
    s->cap = c->cap;
    s->nsegs = c->nsegs;
    s->seg = c->cap / c->nsegs;
    s->held = c->held;
    atomic_init(&s->head, 0u);
    atomic_init(&s->tail, 0u);
    MMGR_ATOMIC_STORE(s->held, (mmgr_word)0);
    s->ord.base = 0u;
    s->ord.span = c->cap;
    s->ord.off = 0u;
    s->owner = NULL;
    s->open = 0;
    atomic_init(&s->slots, (mmgr_word)0);
    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        s->drains[i].first = 0u;
        s->drains[i].last = 0u;
        s->drains[i].next = 0u;
        s->drains[i].gen = 0u;
    }
    return MMGR_TRUE;
}

struct MmgrCursor *mmgr_infin_open(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);

    /* One accessor. A second exists only while a drain does, and that one is not this. */
    if (s->open != 0)
    {
        return NULL;
    }
    s->ord.off = 0u;
    s->open = 1;
    s->owner = c->owner;
    return &s->ord;
}

/**
 * @brief Grant the next segment of a drain.
 *
 * With no @c ptr this is a new drain over [from, to): the segments it covers are reserved in one
 * read modify write, and the first address comes back. With a @c ptr it is the run just finished,
 * so the ring works out which drain that was, releases nothing yet, and hands over the next.
 *
 * NULL means the same as everywhere else here - nothing to do, or not available. The frame is
 * finished, or the segments were already spoken for, or there is no drain slot left. When it is the
 * frame finishing, the reservation is dropped and the ordinary cursor goes back to its owner in the
 * same breath.
 */
const uint8_t *mmgr_infin_drain(const InfinCfg *c)
{
#if !MMGR_ENABLE_KEEPOUT
    (void)c;
    return NULL;
#else
    RingState *const s = ring_of(c->r);

    if ((c->tessera != NULL) && (*c->tessera != 0u))
    {
        Drain *const d = drain_of(s, *c->tessera);
        if (d == NULL)
        {
            return NULL;
        }
        if (d->next >= d->last)
        {
            drain_retire(s, d);
            *c->tessera = 0u;
            return NULL;
        }
        const size_t give = d->next;
        d->next++;
        return &s->buf[give * s->seg];
    }

    if (c->to <= c->from)
    {
        return NULL;
    }

    const size_t first = c->from / s->seg;
    const size_t last = (c->to + s->seg - 1u) / s->seg;
    if (last > s->nsegs)
    {
        return NULL;
    }

    const mmgr_word want = seg_mask(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);
    if ((prev & want) != 0)
    {
        /* Someone already holds part of it. Put back only what this call set. */
        MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return NULL;
    }

    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        const mmgr_word bit = (mmgr_word)((mmgr_word)1 << i);
        const mmgr_word had = atomic_fetch_or_explicit(&s->slots, bit, memory_order_acquire);
        if ((had & bit) != 0)
        {
            continue;
        }
        Drain *const d = &s->drains[i];
        d->first = first;
        d->last = last;
        d->next = first + 1u;
        if (c->tessera != NULL)
        {
            *c->tessera = MMGR_TESSERA(i, d->gen);
        }
        return &s->buf[first * s->seg];
    }

    MMGR_ATOMIC_CLEAR(s->held, want);
    return NULL;
#endif
}

size_t mmgr_infin_available(const InfinCfg *c)
{
    return MMGR_CALL(infin_available, InfinCtx, .s = ring_of(c->r));
}

size_t mmgr_infin_free(const InfinCfg *c)
{
    return MMGR_CALL(infin_free, InfinCtx, .s = ring_of(c->r));
}

mmgr_bool mmgr_infin_read_byte(const InfinCfg *c)
{
    return MMGR_CALL(infin_read_byte, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .dst = c->dst);
}

const uint8_t *mmgr_infin_read(const InfinCfg *c)
{
    return MMGR_CALL(infin_read, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .n = c->n);
}

void mmgr_infin_peek(const InfinCfg *c)
{
    MMGR_CALL(infin_peek, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .dst = c->dst, .n = c->n, .off = c->off);
}

void mmgr_infin_consume(const InfinCfg *c)
{
    MMGR_CALL(infin_consume, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .n = c->n);
}

size_t mmgr_infin_write(const InfinCfg *c)
{
    return MMGR_CALL(infin_write, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .src = c->src, .n = c->n);
}

void mmgr_infin_seek(const InfinCfg *c)
{
    MMGR_CALL(infin_seek, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .off = c->off);
}
