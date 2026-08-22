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
 * Ingestion is one path and one cursor, and it is the mirror of a drain reflected through the head:
 * a drain claims what has arrived, singularitas claims what is free. Both are denied by the same
 * word and neither had to be told about the other. Where they meet is the segment the head is in,
 * and one bit settles it.
 *
 * Every count on that path is in the unit it was opened with, so the head only ever moves by whole
 * units and an address handed out is aligned for the same reason the last one was. Nothing rounds,
 * nothing decays, and no arithmetic here is in bytes until the arbitration has finished with it.
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

/**
 * @brief How many records a tessera may name: the drains, and the one ingestion path after them.
 *
 * One numbering for both sides rather than two, so a drain's token and a grant's token cannot
 * collide and cannot be swapped. A token that names the ingestion slot will not open a drain and a
 * token that names a drain will not publish an ingest, and neither needs a check of its own to say
 * so - the arithmetic already did.
 */
#define MMGR_TESSERA_SLOTS (MMGR_RING_DRAINS + 1u)

/** @brief The record the ingestion path issues its tesserae against. */
#define MMGR_TESSERA_SING MMGR_RING_DRAINS

/** @brief A tessera is which record, and which use of it. */
#define MMGR_TESSERA(idx, gen) (((gen) * MMGR_TESSERA_SLOTS) + (idx) + 1u)

/** @brief Which record a tessera names. */
#define MMGR_TESSERA_IDX(t) ((((t) - 1u)) % MMGR_TESSERA_SLOTS)

/** @brief Which use of that record it was issued for. */
#define MMGR_TESSERA_GEN(t) ((((t) - 1u)) / MMGR_TESSERA_SLOTS)

/**
 * @brief The one ingestion path, and the one grant it may have out.
 *
 * @c span of zero is the whole of "no grant is outstanding". There is no separate flag, because a
 * grant of no units is not a grant.
 */
typedef struct
{
    const void *owner; /**< Who opened it. A second owner is refused. */
    size_t gran;       /**< The unit, in bytes. Fixed when the path opened. */
    size_t at;         /**< Where the grant starts, as a ring offset. */
    size_t span;       /**< How long it is, in bytes. Zero when nothing is out. */
    size_t gen;        /**< Bumped on retire, so a token from a finished grant stops matching. */
    int open;          /**< Whether the path has been opened at all. */
} Singularitas;

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
    Singularitas sing;       /**< The one ingestion path. */
    Drain drains[MMGR_RING_DRAINS];
} RingState;

MMGR_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

/* A status is one 16-bit scalar on every target, split an octet each. The segment index has to fit
   the upper octet for that to hold, and it does because a segment needs a bit of the machine word
   and the widest machine word here is far narrower than 255 segments. */
MMGR_STATIC_ASSERT((size_t)MMGR_RING_LOCULI_MAX <= (size_t)0xFF,
                   "a segment index has to fit the upper octet of a status");
MMGR_STATIC_ASSERT(MMGR_SING_REFUSED < ((mmgr_u16)1 << MMGR_SING_FLAG_BITS),
                   "a status flag has to fit the lower octet of a status");

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
    size_t *tessera;        /**< In/out. The token the ring issued for this drain or grant. */
    const void *owner;      /**< Who the ordinary cursor goes back to. */
    size_t *units;          /**< Out. How many units a grant covers. */
    int rearm;              /**< Whether a commit should hand back the next grant. */
} InfinCtx;

/** @brief How many bytes have arrived and not been consumed. */
MMGR_INLINE size_t ring_arrived(const RingState *s)
{
    return MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&s->head) - MMGR_ATOMIC_LOAD(&s->tail), s->cap);
}

/**
 * @brief How many bytes may still be written. One is held back so full and empty differ.
 *
 * A grant that is out is not writable. It sits ahead of the head and the head has not reached it,
 * so it is neither arrived nor free, and counting it free would tell the one caller who can act on
 * this number the one thing it must not believe.
 */
MMGR_INLINE size_t ring_vacant(const RingState *s)
{
    return (s->cap - 1u) - ring_arrived(s) - s->sing.span;
}

/** @brief How many bytes are readable. */
MMGR_INLINE size_t infin_available(const InfinCtx *c)
{
    return ring_arrived(c->s);
}

/**
 * @brief How many bytes are writable. One is held back so full and empty differ.
 *
 * A grant that is out is not writable. It sits ahead of the head and the head has not reached it,
 * so it is neither arrived nor free, and reporting it as free would tell the one caller who can act
 * on this number the one thing it must not believe.
 */
MMGR_INLINE size_t infin_free(const InfinCtx *c)
{
    return (c->s->cap - 1u) - infin_available(c) - c->s->sing.span;
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

/**
 * @brief How many of @p want bytes from @p at the ordinary accessor may have.
 *
 * A reservation keeps everyone out, not only other claimants. Without this the mask stops a second
 * drain and does nothing about the writer, so a run handed to an egress is a run the producer is
 * still filling - measured, and it happened on every one of four hundred thousand segments.
 *
 * One load of the word answers it however many reservations are outstanding. With none held the
 * whole run is available and this is a compare against zero.
 */
MMGR_INLINE size_t infin_room(const RingState *s, size_t at, size_t want)
{
    const mmgr_word bits = MMGR_ATOMIC_LOAD(s->held);

    if (bits == 0u)
    {
        return want;
    }

    size_t room = 0;
    while (room < want)
    {
        const size_t seg = at / s->seg;
        if ((bits & (mmgr_word)((mmgr_word)1 << seg)) != 0u)
        {
            break;
        }
        size_t step = s->seg - (at % s->seg);
        if (step > (want - room))
        {
            step = want - room;
        }
        room += step;
        at = MMGR_RING_WRAP(at + step, s->cap);
    }
    return room;
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
    if (idx >= MMGR_RING_DRAINS)
    {
        return NULL; /* it names the ingestion path, which is not a drain and never will be */
    }

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

/**
 * @brief Reserve the segments a run of bytes touches, or refuse without disturbing the holder.
 *
 * The mask is the whole of the arbitration. A run is denied because a bit was already set, not
 * because anything looked at who set it - which is why one word answers a drain asking about the
 * producer and the producer asking about a drain, in the same load.
 */
MMGR_INLINE mmgr_bool segs_claim(RingState *s, size_t at, size_t len)
{
    const size_t first = at / s->seg;
    const size_t last = (at + len + s->seg - 1u) / s->seg;
    const mmgr_word want = seg_mask(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);

    if ((prev & want) != 0)
    {
        /* Put back only what this call set. The holder's bits are the holder's. */
        MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return MMGR_FALSE;
    }
    return MMGR_TRUE;
}

/** @brief Give back the segments a run of bytes touches. */
MMGR_INLINE void segs_drop(RingState *s, size_t at, size_t len)
{
    MMGR_ATOMIC_CLEAR(s->held, seg_mask(at / s->seg, (at + len + s->seg - 1u) / s->seg));
}

/**
 * @brief Hand out a run of @p units to fill, or NULL.
 *
 * Bounded four ways, in this order, because each one can only ever shrink what the one before it
 * allowed: what is free, what the caller asked for, what fits before the buffer's end, and what no
 * reservation holds. The clip at the end is not an optimisation - a channel is given one base and
 * one length, so a run that wraps is a run nothing can be told to fill.
 *
 * The head is where the run starts, always. There is one ingestion cursor and it does not choose.
 */
MMGR_INLINE uint8_t *sing_claim(RingState *s, size_t units, size_t *given)
{
    if (s->sing.span != 0u)
    {
        return NULL; /* one grant at a time: the head cannot move under two of them */
    }
    if ((units == 0u) && (given == NULL))
    {
        return NULL; /* asking for whatever fits, with nowhere to be told what that was */
    }

    const size_t head = MMGR_ATOMIC_LOAD(&s->head);
    size_t len = ((s->cap - 1u) - MMGR_RING_WRAP(head - MMGR_ATOMIC_LOAD(&s->tail), s->cap));

    if (units != 0u)
    {
        const size_t want = units * s->sing.gran;
        if (want > len)
        {
            return NULL; /* all of it or none of it */
        }
        len = want;
    }
    if (len > (s->cap - head))
    {
        len = s->cap - head;
    }
    len = infin_room(s, head, len);
    len -= (len % s->sing.gran);

    if ((len == 0u) || ((units != 0u) && (len != (units * s->sing.gran))))
    {
        return NULL;
    }
    if (!segs_claim(s, head, len))
    {
        return NULL;
    }
    s->sing.at = head;
    s->sing.span = len;
    if (given != NULL)
    {
        *given = len / s->sing.gran;
    }
    return &s->buf[head];
}

/** @brief Publish @p bytes of the outstanding grant and give the ground back. */
MMGR_INLINE void sing_commit(RingState *s, size_t bytes)
{
    if (bytes > s->sing.span)
    {
        bytes = s->sing.span; /* nothing may be published that was never granted */
    }
    MMGR_ATOMIC_STORE(&s->head, MMGR_RING_WRAP(s->sing.at + bytes, s->cap));
    segs_drop(s, s->sing.at, s->sing.span);
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen++;
}

/**
 * @brief Copy @c n units in and publish them, or refuse.
 *
 * The synchronous producer, and the only entry here that may wrap: it holds the bytes itself, so it
 * can lay them down in two runs and move the head once. A reader sees all of it or none of it,
 * because the head is stored after the last byte lands and the head is the only thing it reads.
 *
 * No reservation is taken. A reservation covers a window in which someone else is filling ground
 * the ring has promised them, and there is no window here - the copy has already happened by the
 * time this returns.
 */
MMGR_INLINE uint8_t *sing_put(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t n = c->n * s->sing.gran;
    size_t at = MMGR_ATOMIC_LOAD(&s->head);

    if ((n == 0u) || (s->sing.span != 0u) || (n > ((s->cap - 1u) - infin_available(c))))
    {
        return NULL;
    }
    if (infin_room(s, at, n) < n)
    {
        return NULL;
    }

    uint8_t *const first = &s->buf[at];
    const uint8_t *src = c->src;
    size_t left = n;

    while (left > 0u)
    {
        size_t chunk = s->cap - at;
        if (chunk > left)
        {
            chunk = left;
        }
        proxim.read(&s->buf[at], src, chunk);
        at = MMGR_RING_WRAP(at + chunk, s->cap);
        src += chunk;
        left -= chunk;
    }
    MMGR_ATOMIC_STORE(&s->head, at);
    return first;
}

/**
 * @brief The one ingestion path: put, claim, or commit.
 *
 * Which of the three is settled by what the caller filled in and nothing else. The arbitration is
 * above this, in the entry - by here the path is open, the owner is the owner, and the counts are
 * bytes.
 */
MMGR_INLINE uint8_t *sing_body(const InfinCtx *c)
{
    RingState *const s = c->s;

    if (c->src != NULL)
    {
        return sing_put(c);
    }

    if ((c->tessera != NULL) && (*c->tessera != 0u))
    {
        sing_commit(s, c->off * s->sing.gran);
        *c->tessera = 0u;
        if (c->rearm == 0)
        {
            return NULL;
        }
    }

    uint8_t *const at = sing_claim(s, c->n, c->units);
    if ((at != NULL) && (c->tessera != NULL))
    {
        *c->tessera = MMGR_TESSERA(MMGR_TESSERA_SING, s->sing.gen);
    }
    return at;
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
    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen = 0u;
    s->sing.open = 0;
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

    /* Behind the head, always. This is an ingestion path: a drain takes what has arrived and is
     * waiting to go out, so a reservation covers bytes the producer has already written and moved
     * past. Refusing anything else is not only a contract - it is what makes the reservation safe
     * without a lock. A claim over ground the producer is still filling would have to be ordered
     * against a write already in flight, because the writer reads the mask and then stores, and a
     * claim landing between those two lands in the middle of a copy. Measured: with a claim edge in
     * play a reserved run was disturbed constantly; with the claim confined to arrived bytes there
     * is no edge to fall through. */
    {
        const size_t tail = MMGR_ATOMIC_LOAD(&s->tail);
        const size_t arrived = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&s->head) - tail, s->cap);
        const size_t rel = MMGR_RING_WRAP(c->from - tail, s->cap);

        if ((arrived == 0u) || (rel >= arrived) || ((c->to - c->from) > (arrived - rel)))
        {
            return NULL;
        }
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

/**
 * @brief Open the one ingestion path, or say whether the caller asking is the one that holds it.
 *
 * A unit is a power of two and no wider than a machine word, because it has to be one store. Wider
 * than the ring is refused for the reason the rest of the arithmetic depends on: the head advances
 * in whole units, so a unit the buffer cannot hold a single one of makes every later bound a lie.
 */
MMGR_INLINE mmgr_bool sing_admit(RingState *s, const SingularitasCfg *cfg)
{
    if (s->sing.open != 0)
    {
        return (mmgr_bool)(cfg->owner == s->sing.owner);
    }
    if ((cfg->gran == 0u) || !MMGR_RING_POW2(cfg->gran) || (cfg->gran > MMGR_SING_GRANULE_MAX) || (cfg->gran > s->cap))
    {
        return MMGR_FALSE;
    }
    s->sing.owner = cfg->owner;
    s->sing.gran = cfg->gran;
    s->sing.open = 1;
    return MMGR_TRUE;
}

/** @brief Does a token still name the grant it was issued for. */
MMGR_INLINE mmgr_bool sing_token_live(const RingState *s, size_t tessera)
{
    return (mmgr_bool)((s->sing.span != 0u) && (MMGR_TESSERA_IDX(tessera) == MMGR_TESSERA_SING) &&
                       (MMGR_TESSERA_GEN(tessera) == s->sing.gen));
}

/**
 * @brief What the path is, and where it is, in one sixteen bit scalar.
 *
 * Constants below the octet, address space above it. The same seven bits mean the same seven things
 * whether the machine word is sixteen or sixty four, which is the point: a producer's handling of a
 * refusal is written once and does not change shape with the target.
 */
MMGR_INLINE mmgr_u16 sing_state(const RingState *s, mmgr_u16 why)
{
    const size_t at = (s->sing.span != 0u) ? s->sing.at : MMGR_ATOMIC_LOAD(&s->head);
    mmgr_u16 st = why | ((s->sing.open != 0) ? MMGR_SING_ATTACHED : MMGR_SING_READY);

    if (s->sing.span != 0u)
    {
        st |= MMGR_SING_GRANTED;
    }
    if (ring_vacant(s) < s->sing.gran)
    {
        st |= MMGR_SING_FULL;
    }
    return (mmgr_u16)(st | (mmgr_u16)((at / s->seg) << MMGR_SING_FLAG_BITS));
}

/**
 * @brief The one ingestion path.
 *
 * This is the arbitration, and it is all of it: whether there is a path, whether this caller is the
 * one that holds it, and whether the token presented still names the grant it was issued for. What
 * goes past here is a context in bytes with nothing left to decide.
 *
 * A tessera that no longer matches is not an error to report - it is a completion that arrived
 * after the ring stopped waiting for it, which is what a torn-down channel does on real silicon.
 * The answer is to publish nothing, because whatever it wrote went into ground the ring has already
 * given away. The status says which of the refusals it was, since the address cannot.
 *
 * A call that asks for nothing is not a refusal. Presenting a cfg and no ask attaches the path;
 * presenting neither reads the status and touches nothing. Both are how a producer finds out
 * whether the ring is ready for it without committing to anything.
 */
uint8_t *mmgr_infin_singularitas(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);
    const int live = ((c->tessera != NULL) && (*c->tessera != 0u));
    /* Handing over a slot for a token is the ask. A caller with nowhere to put a tessera cannot be
       asking for a grant, and one that brought a slot is asking for nothing else. */
    const int asking = ((c->src != NULL) || (c->tessera != NULL));
    mmgr_u16 why = 0u;
    uint8_t *at = NULL;

    if (live && !sing_token_live(s, *c->tessera))
    {
        *c->tessera = 0u;
        why = MMGR_SING_STALE | MMGR_SING_REFUSED;
    }
    else if (!live && ((c->sing == NULL) || !sing_admit(s, c->sing)))
    {
        /* The path has to be opened, and by the one that holds it. Asking nothing of a path that is
           not yours is still not yours, but it is not a refusal either - nothing was asked. */
        why = (mmgr_u16)(MMGR_SING_ALIEN | (asking ? MMGR_SING_REFUSED : 0u));
    }
    else if (asking)
    {
        at = MMGR_CALL(sing_body, InfinCtx, .s = s, .src = c->src, .n = c->n, .off = c->off, .tessera = c->tessera,
                       .units = c->units, .rearm = (c->sing != NULL));
        if ((at == NULL) && !(live && (c->sing == NULL)))
        {
            why = MMGR_SING_REFUSED; /* a commit that was told to stop returns NULL and meant to */
        }
    }

    if (c->status != NULL)
    {
        *c->status = sing_state(s, why);
    }
    return at;
}

/**
 * @brief Give the ingestion path up, so another stream may have it.
 *
 * Only the auctor may do this. The ring will not take the path off whoever holds it and nothing a
 * second caller can say will move it - a path that could be taken is a path that can be taken in
 * the middle of a transfer, and the const address the holder is still filling into would be handed
 * to someone else on the strength of an unlucky call order.
 *
 * A grant that is still out refuses the detach outright. The ground is promised, the reservation
 * says so, and a signal that the holder is finished is not the same as the hardware being finished.
 * Commit it, or commit nothing, and then let go.
 *
 * The generation is bumped on the way out, so every token the old stream was ever issued stops
 * matching before the next one is admitted. A completion arriving late from a channel that has been
 * torn down is not a hypothetical, and it must not land in the stream that replaced it.
 */
mmgr_bool mmgr_infin_detach(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);
    mmgr_bool let_go = MMGR_FALSE;

    if ((s->sing.open == 0) || (c->sing == NULL) || (c->sing->owner != s->sing.owner))
    {
        let_go = MMGR_FALSE;
        if (c->status != NULL)
        {
            *c->status = sing_state(s, MMGR_SING_ALIEN | MMGR_SING_REFUSED);
        }
        return let_go;
    }
    if (s->sing.span != 0u)
    {
        if (c->status != NULL)
        {
            *c->status = sing_state(s, MMGR_SING_REFUSED);
        }
        return MMGR_FALSE;
    }

    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.open = 0;
    s->sing.gen++;
    let_go = MMGR_TRUE;

    /* The next thing anyone asks this path will be told it is ready. That is the whole handover. */
    if (c->status != NULL)
    {
        *c->status = sing_state(s, 0u);
    }
    return let_go;
}

void mmgr_infin_seek(const InfinCfg *c)
{
    MMGR_CALL(infin_seek, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .off = c->off);
}
