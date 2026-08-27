/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Power-of-two byte ring, a segment view over the same bytes, and loculus keepouts.
 *
 * @note Every type the header only declared is defined here, so the public surface carries no layout.
 * @note head, tail, claim, rel, gratis and held are atomic; the spans and the sizes are plain.
 * @note A move across the wrap is two runs through ring_move, never a walk over single bytes.
 * @note Self-contained: the span mover, the bit index and the span type are all defined here, so the
 *       module reaches nothing outside config.
 */
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include <stdatomic.h>

/**
 * @brief The attributes that make an access at an arbitrary address legal.
 *
 * @note MMGR_ALIGN(1) states the address carries no alignment; MMGR_ALIAS states the type may alias.
 * @warning Either half expands to nothing where its attribute is unavailable.
 */
#define RING_RAW MMGR_ALIGN(1) MMGR_ALIAS

/**
 * @brief Bytes the mover steps at a time: 8 at MMGR_WORD_BITS 64 or more, 4 at 32 or more, 2 otherwise.
 */
#if MMGR_WORD_BITS >= 64
#define RING_STEP 8
#elif MMGR_WORD_BITS >= 32
#define RING_STEP 4
#else
#define RING_STEP 2
#endif

/**
 * @brief The unsigned type RING_STEP bytes wide.
 */
#if RING_STEP >= 8
typedef mmgr_u64 ring_step_word;
#elif RING_STEP >= 4
typedef mmgr_u32 ring_step_word;
#else
typedef mmgr_u16 ring_step_word;
#endif

/**
 * @brief The unaligned view of one step, for a source that may sit anywhere.
 */
typedef ring_step_word ring_raw_word RING_RAW;

/**
 * @brief The unaligned views the tail narrows through, each half the one above it.
 *
 * @note A tail is shorter than one step, so it is carried by these rather than by a step that would
 *       reach past what the caller gave.
 */
typedef mmgr_u32 ring_raw_u32 RING_RAW;
typedef mmgr_u16 ring_raw_u16 RING_RAW;

/**
 * @brief Builds the mask of the lowest n bits of type T.
 *
 * @param[in] T    Unsigned type the mask is built at.
 * @param[in] n    Bits to set.
 * @param[in] bits Width of T in bits.
 * @return         A T with its low n bits set.
 * @note An n at the full width is the case a shift cannot express, so it is answered by complement
 *       instead. An n of 0 falls out of the shift, so neither end needs a test of its own.
 */
#define RING_LOW_MASK(T, n, bits)                                                                     \
    (((n) >= (bits)) ? (T) ~(T)0 : (T)(((T)(((T)1) << (n))) - (T)1))

/**
 * @brief Copies two words at offset i of the mover's walking pointers.
 *
 * @param[in] i First of the two word offsets.
 * @note Named d and s from the enclosing scope, so the cascade below reads as widths alone.
 */
#define RING_COPY_2(i)                                                                                \
    d[(i)] = s[(i)];                                                                                  \
    d[(i) + 1] = s[(i) + 1]

/**
 * @brief Copies four words at offset i, as two runs of two.
 *
 * @param[in] i First of the four word offsets.
 */
#define RING_COPY_4(i)                                                                                \
    RING_COPY_2(i);                                                                                   \
    RING_COPY_2((i) + 2)

/**
 * @brief Copies eight words at offset i, as two runs of four.
 *
 * @param[in] i First of the eight word offsets.
 */
#define RING_COPY_8(i)                                                                                \
    RING_COPY_4(i);                                                                                   \
    RING_COPY_4((i) + 4)

/**
 * @brief Carries n bytes of the tail through type T when the tail still holds that many.
 *
 * @param[in] T Unaligned view n bytes wide.
 * @param[in] n Bytes this rung carries.
 * @note Named db, sb and rem from the enclosing scope, so the cascade reads as widths alone.
 * @note One rung per bit of rem, so each runs at most once and together they carry every tail
 *       exactly, without an access reaching past the bytes the caller gave.
 */
#define RING_TAIL(T, n)                                                                               \
    if ((rem & (n)) != 0u)                                                                            \
    {                                                                                                 \
        *(T *)db = *(const T *)sb;                                                                    \
        db += (n);                                                                                    \
        sb += (n);                                                                                    \
    }

/**
 * @brief Moves sz bytes from src to dst at any alignment.
 *
 * @param[out] dst Destination [BORROWS].
 * @param[in]  src Source [BORROWS].
 * @param[in]  sz  Bytes to move.
 * @note Steps whole words, then narrows through half a step at a time for the tail, so a tail of
 *       seven bytes is three accesses rather than seven and none of them is a single byte twice.
 * @note Both sides go through the unaligned view: a ring offset is any byte, so neither pointer can
 *       be walked to a boundary first without putting a per-byte head back.
 * @note Every access lies inside sz. The tail narrows rather than masking a whole step, so neither
 *       region needs room past what the caller gave, and the destination is written, never read.
 * @warning The two regions must not overlap.
 */
MMGR_INLINE void ring_move(uint8_t *dst, const uint8_t *src, size_t sz)
{
    size_t words = sz / (size_t)RING_STEP;
    const size_t rem = sz - (words * (size_t)RING_STEP);
    ring_raw_word *d = (ring_raw_word *)dst;
    const ring_raw_word *s = (const ring_raw_word *)src;

    while (words >= 8u)
    {
        RING_COPY_8(0);
        d += 8;
        s += 8;
        words -= 8u;
    }
    if ((words & 4u) != 0u)
    {
        RING_COPY_4(0);
        d += 4;
        s += 4;
    }
    if ((words & 2u) != 0u)
    {
        RING_COPY_2(0);
        d += 2;
        s += 2;
    }
    if ((words & 1u) != 0u)
    {
        d[0] = s[0];
        d += 1;
        s += 1;
    }

    // Explicit casts bring the walked pointers back to bytes for the narrowing tail below
    uint8_t *db = (uint8_t *)d;
    const uint8_t *sb = (const uint8_t *)s;

#if RING_STEP >= 8
    RING_TAIL(ring_raw_u32, 4u)
#endif
#if RING_STEP >= 4
    RING_TAIL(ring_raw_u16, 2u)
#endif
    RING_TAIL(uint8_t, 1u)
}

/**
 * @brief A word holding 1 in every octet, which is 0x0101...01.
 *
 * @note Multiplying a per-octet count by this sums every octet into the top one.
 */
#define RING_ONES ((mmgr_word)(((mmgr_word) ~(mmgr_word)0) / 0xFFu))

/**
 * @brief A word holding 0x55 in every octet, which pairs the bits for the first fold.
 */
#define RING_PAIRS ((mmgr_word)(((mmgr_word) ~(mmgr_word)0) / 3u))

/**
 * @brief A word holding 0x33 in every octet, which pairs the counts for the second fold.
 */
#define RING_QUADS ((mmgr_word)(((mmgr_word) ~(mmgr_word)0) / 5u))

/**
 * @brief A word holding 0x0F in every octet, which holds each octet's count after the third fold.
 */
#define RING_OCTETS ((mmgr_word)(((mmgr_word) ~(mmgr_word)0) / 17u))

/**
 * @brief Counts the zero bits below the lowest set bit of x.
 *
 * @param[in] x Value to measure.
 * @return      Trailing zero count, 0 through MMGR_WORD_BITS.
 * @note Isolating the lowest set bit and taking one off leaves exactly that many bits set, so the
 *       trailing zero count is a population count.
 * @note The count folds in place a lane at a time, then one multiply sums every octet into the top
 *       one, which the final shift reads out. Every constant is derived from the word, so the same
 *       four steps run at any width.
 * @note No step branches on the value.
 * @warning An x of 0 returns MMGR_WORD_BITS; every caller tests for an empty mask first.
 */
MMGR_INLINE mmgr_iword ring_trail(mmgr_word x)
{
    // Explicit cast keeps the two's complement negation at mmgr_word, isolating the lowest set bit
    mmgr_word v = (mmgr_word)((x & (mmgr_word)(0u - x)) - (mmgr_word)1);

    // Explicit casts hold each fold at mmgr_word: pairs, then quads, then whole octets
    v = (mmgr_word)(v - ((v >> 1) & RING_PAIRS));
    v = (mmgr_word)((v & RING_QUADS) + ((v >> 2) & RING_QUADS));
    v = (mmgr_word)((v + (v >> 4)) & RING_OCTETS);
    return (mmgr_iword)((mmgr_word)(v * RING_ONES) >> (MMGR_WORD_BITS - 8u));
}

/**
 * @brief Acquire load of an atomic member.
 *
 * @param[in] p Address of the atomic to read [BORROWS].
 * @return      The value read.
 * @note Acquire ordering, so writes released by the other side are visible after it.
 */
#define MMGR_ATOMIC_LOAD(p) atomic_load_explicit((p), memory_order_acquire)

/**
 * @brief Release store to an atomic member.
 *
 * @param[in] p Address of the atomic to write [BORROWS].
 * @param[in] v Value to store.
 * @note Release ordering, so buffer writes made before it are visible to an acquiring reader.
 */
#define MMGR_ATOMIC_STORE(p, v) atomic_store_explicit((p), (v), memory_order_release)

/**
 * @brief The whole ring state, laid into the caller's mmgr_ring storage.
 *
 * @warning Reached by casting mmgr_ring::opaque; the assertion below checks it fits inside.
 */
typedef struct
{
    uint8_t *buf;             /**< Ring bytes [BORROWS]. */
    size_t cap;               /**< Bytes in buf, always a power of two. */
    size_t nsegs;             /**< Segments the ring is divided into, a power of two. */
    size_t seg;               /**< Bytes per segment, cap divided by nsegs. */
    _Atomic size_t head;      /**< Write position, advanced by the producer. */
    _Atomic size_t tail;      /**< Read position, advanced by the consumer. */
    _Atomic size_t claim;     /**< Segments the producer has filled, counting up. */
    _Atomic size_t rel;       /**< Segments the consumer has released, counting up. */
    _Atomic mmgr_word gratis; /**< Bit i set while loculus i is free to take. */
    _Atomic mmgr_word held;   /**< Bit i set while loculus i still owns bytes in flight. */
    /**
     * @brief Region each held loculus keeps out, recorded at the hold.
     *
     * @note Sized to one entry when MMGR_RING_LOCULI is 0, since C has no zero-length array. That
     *       entry is unreachable: loculus_bit and infin_loculus_keepout both refuse every index then.
     */
    mmgr_ring_span keep[(MMGR_RING_LOCULI > 0u) ? MMGR_RING_LOCULI : 1u];
} RingState;

MMGR_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

/**
 * @brief Arguments for every backend in this file, grouped by the calls that read them.
 *
 * @note Each backend reads one group; MMGR_CALL zeroes the members it is not given.
 */
typedef struct
{
    RingState *s;       /**< Ring state to act on [BORROWS]. */
    uint8_t *dst;       /**< Destination for read, read_byte and peek [BORROWS]. */
    const uint8_t *src; /**< Bytes put writes, or the region hold records [BORROWS]. */
    size_t bytes;       /**< Byte count the call moves or records. */
    size_t off;         /**< Offset ahead of the tail that peek starts at. */
    size_t idx;         /**< Loculus or segment the call acts on. */
    mmgr_word mask;     /**< Mask loculus_next picks the lowest set bit of. */
    size_t *out;        /**< Set to the segment index a next or front call chose [BORROWS]. */
} InfinCtx;

/**
 * @brief Reads the ring state out of the caller's opaque storage.
 *
 * @param[in] r Ring storage the caller declared [BORROWS].
 * @return      The state laid into it [BORROWS].
 * @note The cast goes through void *; mmgr_ring aligns opaque to size_t.
 * @warning The state is only valid after mmgr_infin_init has returned MMGR_TRUE for this ring.
 */
MMGR_INLINE RingState *ring_of(mmgr_ring *r)
{
    return (RingState *)(void *)r->opaque;
}

/**
 * @brief Returns the bit naming loculus idx, or 0 when idx names none.
 *
 * @param[in] idx Loculus index.
 * @return        The bit, or 0.
 * @note The bound is here rather than at each call site, so a shift past the word never happens.
 * @note An out-of-range loculus names nothing, so it reads as held and is never handed out.
 */
MMGR_INLINE mmgr_word ring_loculus_bit(size_t idx)
{
#if MMGR_RING_LOCULI == 0u
    // A build with no loculi names none, so the bound below would compare against zero and always hold
    (void)idx;
    return (mmgr_word)0;
#else
    if (idx >= (size_t)MMGR_RING_LOCULI)
    {
        return (mmgr_word)0;
    }
    // Explicit casts build the loculus bit at mmgr_word width, matching the masks it is tested against
    return (mmgr_word)((mmgr_word)1 << idx);
#endif
}

/**
 * @brief Returns every loculus below MMGR_RING_LOCULI, as a mask.
 *
 * @return The mask.
 * @note Full width when the loculus count fills the word, which is the case a shift could not build.
 */
MMGR_INLINE mmgr_word ring_loculus_all(void)
{
    return RING_LOW_MASK(mmgr_word, (size_t)MMGR_RING_LOCULI, (size_t)MMGR_RING_LOCULI_MAX);
}

/**
 * @brief How a move of want bytes from offset at divides across the wrap.
 */
typedef struct
{
    size_t n;     /**< Bytes the move will actually carry. */
    size_t first; /**< Bytes of that lying before the end of the buffer. */
} RingRun;

/**
 * @brief Divides a move at the end of the buffer.
 *
 * @param[in] s    Ring state [BORROWS].
 * @param[in] at   Ring offset to start from.
 * @param[in] want Bytes the caller asked for.
 * @return         The count to carry and how much of it precedes the wrap.
 * @note Both directions divide the same way, so the arithmetic lives here once.
 * @warning want is held at cap: two runs cannot express more than one lap, so a larger count would
 *          take the second run past the end of the buffer.
 */
MMGR_INLINE RingRun ring_run(const RingState *s, size_t at, size_t want)
{
    const size_t n = (want > s->cap) ? s->cap : want;
    const size_t room = s->cap - at;
    RingRun r;

    r.n = n;
    r.first = (room < n) ? room : n;
    return r;
}

/**
 * @brief Moves want bytes out of the ring from offset at, in at most two runs.
 *
 * @param[in]  s    Ring state [BORROWS].
 * @param[in]  at   Ring offset to start from.
 * @param[out] dst  Destination [BORROWS].
 * @param[in]  want Bytes to move.
 * @return          The offset one past what was moved, wrapped.
 * @note The first pass stops at the end of the buffer and the second takes whatever wrapped, so the
 *       wrap costs one extra pass rather than a test on every byte.
 * @note A loop rather than a pass and a guarded second one, so the mover is emitted once instead of
 *       twice. Two copies of it made this entry too large to inline well, which cost far more than
 *       the loop does.
 */
MMGR_INLINE size_t ring_move_out(RingState *s, size_t at, uint8_t *dst, size_t want)
{
    const RingRun r = ring_run(s, at, want);
    size_t done = 0u;
    size_t from = at;
    size_t n = r.first;

    while (done < r.n)
    {
        ring_move(dst + done, &s->buf[from], n);
        done += n;
        // The second pass starts at the buffer's first byte and carries whatever wrapped
        from = 0u;
        n = r.n - done;
    }
    return MMGR_RING_WRAP(at + r.n, s->cap);
}

/**
 * @brief Moves want bytes into the ring at offset at, in at most two runs.
 *
 * @param[in,out] s    Ring state [BORROWS].
 * @param[in]     at   Ring offset to start at.
 * @param[in]     src  Source bytes [BORROWS].
 * @param[in]     want Bytes to move.
 * @return             The offset one past what was moved, wrapped.
 * @note The mirror of ring_move_out, and the reason a fill and a drain cost the same per byte.
 */
MMGR_INLINE size_t ring_move_in(RingState *s, size_t at, const uint8_t *src, size_t want)
{
    const RingRun r = ring_run(s, at, want);
    size_t done = 0u;
    size_t to = at;
    size_t n = r.first;

    while (done < r.n)
    {
        ring_move(&s->buf[to], src + done, n);
        done += n;
        // The second pass starts at the buffer's first byte and carries whatever wrapped
        to = 0u;
        n = r.n - done;
    }
    return MMGR_RING_WRAP(at + r.n, s->cap);
}

/**
 * @brief Returns the readable bytes between two cursors already in hand.
 *
 * @param[in] h   Head the caller read.
 * @param[in] t   Tail the caller read.
 * @param[in] cap Ring size.
 * @return        Distance from t to h, wrapped into the ring.
 * @note Takes the cursors rather than the ring, so a caller holding one of them from its own load
 *       reaches the same arithmetic without reading it twice.
 */
MMGR_INLINE size_t ring_used(size_t h, size_t t, size_t cap)
{
    return MMGR_RING_WRAP(h - t, cap);
}

/**
 * @brief Returns the writable bytes between two cursors already in hand.
 *
 * @param[in] h   Head the caller read.
 * @param[in] t   Tail the caller read.
 * @param[in] cap Ring size.
 * @return        cap minus one, minus the readable bytes.
 * @note One byte is withheld so a full ring and an empty one do not share a cursor pair.
 */
MMGR_INLINE size_t ring_free(size_t h, size_t t, size_t cap)
{
    return (cap - 1u) - ring_used(h, t, cap);
}

/**
 * @brief Returns the bytes the consumer may still read.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      Distance from tail to head, wrapped into the ring.
 */
MMGR_INLINE size_t infin_available(const InfinCtx *c)
{
    return ring_used(MMGR_ATOMIC_LOAD(&c->s->head), MMGR_ATOMIC_LOAD(&c->s->tail), c->s->cap);
}

/**
 * @brief Returns the bytes the producer may still write.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      cap minus one, minus the readable bytes.
 */
MMGR_INLINE size_t infin_vacant(const InfinCtx *c)
{
    return ring_free(MMGR_ATOMIC_LOAD(&c->s->head), MMGR_ATOMIC_LOAD(&c->s->tail), c->s->cap);
}

/**
 * @brief Takes one byte from the tail and advances past it.
 *
 * @param[in,out] c Ring and the destination byte [BORROWS].
 * @return          MMGR_TRUE when a byte was taken, MMGR_FALSE when the ring was empty.
 */
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
 * @brief Takes up to c->bytes into c->dst and advances the tail once at the end.
 *
 * @param[in,out] c Ring, destination and the most to take [BORROWS].
 * @return          Bytes actually taken.
 * @note Publishes the tail once, after the move, rather than per byte.
 */
MMGR_INLINE size_t infin_read(const InfinCtx *c)
{
    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    const size_t have = ring_used(MMGR_ATOMIC_LOAD(&c->s->head), t, c->s->cap);
    const size_t n = (have < c->bytes) ? have : c->bytes;

    MMGR_ATOMIC_STORE(&c->s->tail, ring_move_out(c->s, t, c->dst, n));
    return n;
}

/**
 * @brief Copies c->bytes out of the ring starting c->off past the tail, leaving the tail alone.
 *
 * @param[in,out] c Ring, destination, byte count and starting offset [BORROWS].
 * @warning Copies c->bytes whether or not that many are available.
 */
MMGR_INLINE void infin_peek(const InfinCtx *c)
{
    const size_t at = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->off, c->s->cap);

    (void)ring_move_out(c->s, at, c->dst, c->bytes);
}

/**
 * @brief Advances the tail past c->bytes.
 *
 * @param[in,out] c Ring and the byte count to drop [BORROWS].
 */
MMGR_INLINE void infin_consume(const InfinCtx *c)
{
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->bytes, c->s->cap));
}

/**
 * @brief Writes c->bytes of c->src into the ring, or refuses the whole span.
 *
 * @param[in,out] c Ring, the bytes to write and their count [BORROWS].
 * @return          MMGR_TRUE when the span was written, MMGR_FALSE when it would not fit.
 * @note The head stays local across the move and is published once, so no half span is visible.
 */
MMGR_INLINE mmgr_bool infin_put(const InfinCtx *c)
{
    const size_t h = MMGR_ATOMIC_LOAD(&c->s->head);

    if (c->bytes > ring_free(h, MMGR_ATOMIC_LOAD(&c->s->tail), c->s->cap))
    {
        return MMGR_FALSE;
    }
    MMGR_ATOMIC_STORE(&c->s->head, ring_move_in(c->s, h, c->src, c->bytes));
    return MMGR_TRUE;
}

/**
 * @brief Returns the segments filled and not yet released.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      The distance between the two counters.
 */
MMGR_INLINE size_t infin_seg_inflight(const InfinCtx *c)
{
    return MMGR_ATOMIC_LOAD(&c->s->claim) - MMGR_ATOMIC_LOAD(&c->s->rel);
}

/**
 * @brief Hands back the segment a counter names, when its side says one is there.
 *
 * @param[in,out] c      Ring, and where to write the index [BORROWS].
 * @param[in]     cursor Counter naming the segment.
 * @param[in]     ok     Whether the caller's side has a segment to give.
 * @return               ok, and c->out is only written when it holds.
 * @note Both ends reduce to this: the counter is wrapped into range and reported. Only the test for
 *       whether a segment exists differs, so each end keeps its own and passes the answer down.
 */
MMGR_INLINE mmgr_bool ring_seg_pick(const InfinCtx *c, size_t cursor, mmgr_bool ok)
{
    if (!ok)
    {
        return MMGR_FALSE;
    }
    *c->out = cursor & (c->s->nsegs - 1u);
    return MMGR_TRUE;
}

/**
 * @brief Reports the index of the segment the producer fills next.
 *
 * @param[in,out] c Ring, and where to write the index [BORROWS].
 * @return          MMGR_FALSE when every segment is in flight.
 */
MMGR_INLINE mmgr_bool infin_seg_next(const InfinCtx *c)
{
    const size_t claim = MMGR_ATOMIC_LOAD(&c->s->claim);

    return ring_seg_pick(c, claim, (claim - MMGR_ATOMIC_LOAD(&c->s->rel)) < c->s->nsegs);
}

/**
 * @brief Advances one of the segment counters by one.
 *
 * @param[in,out] p Counter to advance [BORROWS].
 * @note Publishing and releasing are the same step on opposite counters, so both reach this.
 * @note One side owns each counter, so the read and the write need no atomicity between them.
 */
MMGR_INLINE void ring_bump(_Atomic size_t *p)
{
    MMGR_ATOMIC_STORE(p, MMGR_ATOMIC_LOAD(p) + 1u);
}

/**
 * @brief Makes the filled segment visible to the consumer.
 *
 * @param[in,out] c Ring to advance [BORROWS].
 */
MMGR_INLINE void infin_seg_publish(const InfinCtx *c)
{
    ring_bump(&c->s->claim);
}

/**
 * @brief Reports the index of the segment the consumer takes next.
 *
 * @param[in,out] c Ring, and where to write the index [BORROWS].
 * @return          MMGR_FALSE when none is in flight.
 */
MMGR_INLINE mmgr_bool infin_seg_front(const InfinCtx *c)
{
    const size_t rel = MMGR_ATOMIC_LOAD(&c->s->rel);

    return ring_seg_pick(c, rel, MMGR_ATOMIC_LOAD(&c->s->claim) != rel);
}

/**
 * @brief Frees the front segment.
 *
 * @param[in,out] c Ring to advance [BORROWS].
 */
MMGR_INLINE void infin_seg_release(const InfinCtx *c)
{
    ring_bump(&c->s->rel);
}

/**
 * @brief Returns the contiguous span of segment c->idx.
 *
 * @param[in] c Ring and the segment index [BORROWS].
 * @return      Its first byte inside the ring buffer [BORROWS].
 */
MMGR_INLINE uint8_t *infin_seg_at(const InfinCtx *c)
{
    return &c->s->buf[c->idx * c->s->seg];
}

/**
 * @brief Returns the loculi that are free and not held.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      The free mask with the held ones cleared, bounded to the loculi this build has.
 */
MMGR_INLINE mmgr_word infin_loculus_ready(const InfinCtx *c)
{
    return MMGR_ATOMIC_LOAD(&c->s->gratis) & ~MMGR_ATOMIC_LOAD(&c->s->held) & ring_loculus_all();
}

/**
 * @brief Returns the index of the lowest set bit of c->mask.
 *
 * @param[in] c The mask to pick from [BORROWS].
 * @return      The index, or -1 when c->mask is empty.
 */
MMGR_INLINE mmgr_iword infin_loculus_next(const InfinCtx *c)
{
    if (c->mask == (mmgr_word)0)
    {
        return (mmgr_iword)-1;
    }
    return ring_trail(c->mask);
}

/**
 * @brief Takes loculus c->idx and records the region it keeps out.
 *
 * @param[in,out] c Ring, the loculus, and the region to record [BORROWS].
 * @return          MMGR_TRUE when this caller took the loculus.
 */
MMGR_INLINE mmgr_bool infin_loculus_hold(const InfinCtx *c)
{
    const mmgr_word bit = ring_loculus_bit(c->idx);

    if (bit == (mmgr_word)0)
    {
        return MMGR_FALSE;
    }

    const mmgr_word prev = atomic_fetch_or_explicit(&c->s->held, bit, memory_order_acquire);

    if ((prev & bit) != 0u)
    {
        return MMGR_FALSE;
    }
    // Explicit cast drops the source qualifier: the span records a region it never writes through
    c->s->keep[c->idx].buf = (uint8_t *)(uintptr_t)c->src;
    c->s->keep[c->idx].cap = c->bytes;
    c->s->keep[c->idx].pos = 0u;
    return MMGR_TRUE;
}

/**
 * @brief Returns the region loculus c->idx is keeping out.
 *
 * @param[in] c Ring and the loculus [BORROWS].
 * @return      The recorded span, or NULL when c->idx names none [BORROWS].
 */
MMGR_INLINE const mmgr_ring_span *infin_loculus_keepout(const InfinCtx *c)
{
#if MMGR_RING_LOCULI == 0u
    // A build with no loculi keeps nothing out, so there is no span to hand back
    (void)c;
    return NULL;
#else
    if (c->idx >= (size_t)MMGR_RING_LOCULI)
    {
        return NULL;
    }
    return &c->s->keep[c->idx];
#endif
}

/**
 * @brief Gives loculus c->idx back.
 *
 * @param[in,out] c Ring and the loculus [BORROWS].
 * @note Leaves the recorded span and the bytes alone, so a restream can run again.
 */
MMGR_INLINE void infin_loculus_drop(const InfinCtx *c)
{
    // Explicit cast keeps the complement at mmgr_word width, matching the atomic it clears
    (void)atomic_fetch_and_explicit(&c->s->held, (mmgr_word)~ring_loculus_bit(c->idx), memory_order_release);
}

/**
 * @brief Marks loculus c->idx free.
 *
 * @param[in,out] c Ring and the loculus [BORROWS].
 */
MMGR_INLINE void infin_loculus_mark(const InfinCtx *c)
{
    (void)atomic_fetch_or_explicit(&c->s->gratis, ring_loculus_bit(c->idx), memory_order_release);
}

/**
 * @brief Lays a fresh ring into c->ring, over the bytes at c->buf.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
mmgr_bool mmgr_infin_init(const InfinCfg *c)
{
    MMGR_ASSERT(c->ring != NULL, "a ring needs storage");
    MMGR_ASSERT(c->buf != NULL, "a ring needs a buffer");

    if ((c->cap == 0u) || !MMGR_RING_POW2(c->cap))
    {
        return MMGR_FALSE;
    }
    if ((c->nsegs == 0u) || !MMGR_RING_POW2(c->nsegs) || (c->nsegs > c->cap))
    {
        return MMGR_FALSE;
    }

    RingState *const s = ring_of(c->ring);

    s->buf = c->buf;
    s->cap = c->cap;
    s->nsegs = c->nsegs;
    s->seg = c->cap / c->nsegs;
    atomic_init(&s->head, 0u);
    atomic_init(&s->tail, 0u);
    atomic_init(&s->claim, 0u);
    atomic_init(&s->rel, 0u);
    atomic_init(&s->gratis, ring_loculus_all());
    atomic_init(&s->held, (mmgr_word)0);
#if MMGR_RING_LOCULI > 0u
    for (size_t i = 0; i < (size_t)MMGR_RING_LOCULI; i++)
    {
        s->keep[i].buf = NULL;
        s->keep[i].cap = 0u;
        s->keep[i].pos = 0u;
    }
#endif
    return MMGR_TRUE;
}

/**
 * @brief The state argument every entry point but one forwards.
 *
 * @note Names c from the entry point's own parameter, so the table below carries fields alone.
 */
#define RING_S .s = ring_of(c->ring)

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_infin_ and infin_ prefixes, which the two share.
 * @note The prefixes and the two structure types are the same for every entry, so they are named
 *       once here and the table below states only what each entry differs in.
 * @note The variadic part is the argument pack, never empty, so no comma needs eliding.
 */
#define RING_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_infin_, infin_, InfinCtx, InfinCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_infin_ and infin_ prefixes, which the two share.
 * @note Separate from RING_ENTRY because a return with an expression is not allowed in a void
 *       function, so the two cannot share one body.
 */
#define RING_ENTRY_V(name, ...) GENERIC_ENTRY_V(mmgr_infin_, infin_, InfinCtx, InfinCfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in confinium_exclusivum_infinitas.h.
 */
RING_ENTRY(size_t, available, RING_S)
RING_ENTRY(size_t, vacant, RING_S)
RING_ENTRY(mmgr_bool, read_byte, RING_S, .dst = c->dst)
RING_ENTRY(size_t, read, RING_S, .dst = c->dst, .bytes = c->bytes)
RING_ENTRY_V(peek, RING_S, .dst = c->dst, .bytes = c->bytes, .off = c->off)
RING_ENTRY_V(consume, RING_S, .bytes = c->bytes)
RING_ENTRY(mmgr_bool, put, RING_S, .src = c->src, .bytes = c->bytes)
RING_ENTRY(size_t, seg_inflight, RING_S)
RING_ENTRY(mmgr_bool, seg_next, RING_S, .out = c->out)
RING_ENTRY_V(seg_publish, RING_S)
RING_ENTRY(mmgr_bool, seg_front, RING_S, .out = c->out)
RING_ENTRY_V(seg_release, RING_S)
RING_ENTRY(uint8_t *, seg_at, RING_S, .idx = c->idx)
RING_ENTRY(mmgr_word, loculus_ready, RING_S)
RING_ENTRY(mmgr_iword, loculus_next, .mask = c->mask)
RING_ENTRY(mmgr_bool, loculus_hold, RING_S, .idx = c->idx, .src = c->src, .bytes = c->bytes)
RING_ENTRY(const mmgr_ring_span *, loculus_keepout, RING_S, .idx = c->idx)
RING_ENTRY_V(loculus_drop, RING_S, .idx = c->idx)
RING_ENTRY_V(loculus_mark, RING_S, .idx = c->idx)
