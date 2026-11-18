/**
 * @brief Power-of-two byte ring, a segment view over the same bytes, and loculus keepouts.
 *
 * @note Every type the header only declared has its definition here, and so the public surface carries no layout.
 * @note head, tail, claim, rel, gratis and pinned are atomic; the spans and the sizes are plain.
 * @note A move across the wrap is two runs through ring_move, never a walk over single bytes.
 * @note Self-contained: the span mover, the bit index and the span type are all defined here, and so the
 *       module reaches nothing outside config.
 */
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include "embed_atomic.h"

/**
 * @brief The attributes that make an access at an arbitrary address legal.
 *
 * @note EMBED_ALIGN(1) states the address carries no alignment; EMBED_ALIAS states the type may alias.
 * @warning Either half expands to nothing where its attribute is unavailable.
 */
#define RING_RAW EMBED_ALIGN(1) EMBED_ALIAS

/**
 * @brief Bytes the mover steps at a time: 8 at EMBED_WORD_BITS 64 or more, 4 at 32 or more, 2 otherwise.
 */
#if EMBED_WORD_BITS >= 64
#define RING_STEP 8
#elif EMBED_WORD_BITS >= 32
#define RING_STEP 4
#else
#define RING_STEP 2
#endif

/**
 * @brief The unsigned type RING_STEP bytes wide.
 */
#if RING_STEP >= 8
typedef embed_u64 ring_step_word;
#elif RING_STEP >= 4
typedef embed_u32 ring_step_word;
#else
typedef embed_u16 ring_step_word;
#endif

/**
 * @brief The unaligned view of one step, for a source that may sit anywhere.
 */
typedef ring_step_word ring_raw_word RING_RAW;

/**
 * @brief The unaligned views the tail narrows through, each half the one above it.
 *
 * @note A tail is shorter than one step. So these carry it, and not a step that would reach past
 *       what the caller gave.
 */
typedef embed_u32 ring_raw_u32 RING_RAW;
typedef embed_u16 ring_raw_u16 RING_RAW;

/**
 * @brief Builds the mask of the lowest n bits of type T.
 *
 * @param[in] T    Unsigned type of the mask.
 * @param[in] n    Bits to set.
 * @param[in] bits Width of T in bits.
 * @return         A T with its low n bits set.
 * @note An n at the full width is the case a shift cannot express, and so the complement answers it
 *       instead. An n of 0 falls out of the shift, and so neither end needs a test of its own.
 */
#define RING_LOW_MASK(T, n, bits) (((n) >= (bits)) ? (T) ~(T)0 : (T)(((T)(((T)1) << (n))) - (T)1))

/**
 * @brief Copies two words at offset i of the mover's walk pointers.
 *
 * @param[in] i First of the two word offsets.
 * @note Named d and s from the scope around it, and so the cascade below reads as widths alone.
 */
#define RING_COPY_2(i)                                                                                                 \
    d[(i)] = s[(i)];                                                                                                   \
    d[(i) + 1] = s[(i) + 1]

/**
 * @brief Copies four words at offset i, as two runs of two.
 *
 * @param[in] i First of the four word offsets.
 */
#define RING_COPY_4(i)                                                                                                 \
    RING_COPY_2(i);                                                                                                    \
    RING_COPY_2((i) + 2)

/**
 * @brief Copies eight words at offset i, as two runs of four.
 *
 * @param[in] i First of the eight word offsets.
 */
#define RING_COPY_8(i)                                                                                                 \
    RING_COPY_4(i);                                                                                                    \
    RING_COPY_4((i) + 4)

/**
 * @brief Carries n bytes of the tail through type T when the tail still holds that many.
 *
 * @param[in] T Unaligned view n bytes wide.
 * @param[in] n Bytes this rung carries.
 * @note Named db, sb and rem from the scope around it, and so the cascade reads as widths alone.
 * @note One rung per bit of rem, and so each runs at max once. Together they carry every tail
 *       exactly, with no access past the bytes the caller gave.
 */
#define RING_TAIL(T, n)                                                                                                \
    if ((rem & (n)) != 0u)                                                                                             \
    {                                                                                                                  \
        *(T *)db = *(const T *)sb;                                                                                     \
        db += (n);                                                                                                     \
        sb += (n);                                                                                                     \
    }

/**
 * @brief Moves sz bytes from src to dst at any alignment.
 *
 * @param[out] dst Destination [BORROWS].
 * @param[in]  src Source [BORROWS].
 * @param[in]  sz  Bytes to move.
 * @note Steps whole words, then narrows through half a step at a time for the tail. A tail of
 *       seven bytes is three accesses and not seven and none of them is a single byte twice.
 * @note Both sides go through the unaligned view. A ring offset is any byte, and so neither pointer can
 *       walk to a boundary first with no per-byte head put back.
 * @note Every access lies inside sz. The tail narrows instead of a mask over a whole step. So neither
 *       region needs room past what the caller gave, and the destination only takes writes, never reads.
 * @warning The two regions must not overlap.
 */
EMBED_INLINE void ring_move(uint8_t *dst, const uint8_t *src, size_t sz)
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

    // Explicit casts bring the walked pointers back to bytes for the narrow tail below
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
 * @brief A word with 1 in every octet, which is 0x0101...01.
 *
 * @note A per-octet count times this sums every octet into the top one.
 */
#define RING_ONES ((embed_word)(((embed_word) ~(embed_word)0) / 0xFFu))

/**
 * @brief A word with 0x55 in every octet, which pairs the bits for the first fold.
 */
#define RING_PAIRS ((embed_word)(((embed_word) ~(embed_word)0) / 3u))

/**
 * @brief A word with 0x33 in every octet, which pairs the counts for the second fold.
 */
#define RING_QUADS ((embed_word)(((embed_word) ~(embed_word)0) / 5u))

/**
 * @brief A word with 0x0F in every octet, which holds each octet's count after the third fold.
 */
#define RING_OCTETS ((embed_word)(((embed_word) ~(embed_word)0) / 17u))

/**
 * @brief Counts the zero bits below the lowest set bit of x.
 *
 * @param[in] x Value to measure.
 * @return      Trailing zero count, 0 through EMBED_WORD_BITS.
 * @note The lowest set bit, isolated and with one taken off, leaves exactly that many bits set. So the
 *       trailing zero count is a population count.
 * @note The count folds in place a lane at a time. Then one multiply sums every octet into the top
 *       one, which the final shift reads out. Every constant derives from the word, and so the same
 *       four steps run at any width.
 * @note No step branches on the value.
 * @warning An x of 0 returns EMBED_WORD_BITS; every caller tests for an empty mask first.
 */
EMBED_INLINE embed_iword ring_trail(embed_word x)
{
    // Explicit cast retains the two's complement negation at embed_word, which picks out the lowest set bit
    embed_word v = (embed_word)((x & (embed_word)(0u - x)) - (embed_word)1);

    // Explicit casts hold each fold at embed_word: pairs, then quads, then whole octets
    v = (embed_word)(v - ((v >> 1) & RING_PAIRS));
    v = (embed_word)((v & RING_QUADS) + ((v >> 2) & RING_QUADS));
    v = (embed_word)((v + (v >> 4)) & RING_OCTETS);
    return (embed_iword)((embed_word)(v * RING_ONES) >> (EMBED_WORD_BITS - 8u));
}

/**
 * @brief The whole ring state, positioned into the caller's mmgr_ring storage.
 *
 * @warning Reached by a cast of mmgr_ring::opaque; the assertion below checks it fits inside.
 */
typedef struct
{
    uint8_t *buf;             /**< Ring bytes [BORROWS]. */
    size_t cap;               /**< Bytes in buf, always a power of two. */
    size_t nsegs;             /**< Segments in the ring, a power of two. */
    size_t seg;               /**< Bytes per segment, cap divided by nsegs. */
    embed_atomic_usize head;      /**< Write position, advanced by the producer. */
    embed_atomic_usize tail;      /**< Read position, advanced by the consumer. */
    embed_atomic_usize claim;     /**< Segments the producer filled, a count that only rises. */
    embed_atomic_usize rel;       /**< Segments the consumer released, a count that only rises. */
    embed_atomic_word gratis; /**< Bit i set while loculus i is free to take. */
    embed_atomic_word pinned; /**< Bit i set while loculus i still owns bytes in flight. */
    /**
     * @brief Region each pinned loculus retains out, recorded at the hold.
     *
     * @note Sized to one entry when MMGR_RING_LOCULI is 0, since C has no zero-length array. That
     *       entry is unreachable: loculus_bit and infin_loculus_keepout both refuse every index then.
     */
    mmgr_ring_span retain[(MMGR_RING_LOCULI > 0u) ? MMGR_RING_LOCULI : 1u];
} RingState;

EMBED_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

/**
 * @brief Arguments for every backend in this file, grouped by the calls that read them.
 *
 * @note Each backend reads one group; EMBED_CALL zeroes the members the caller omits.
 */
typedef struct
{
    RingState *s;       /**< Ring state to act on [BORROWS]. */
    uint8_t *dst;       /**< Destination for read, read_byte and peek [BORROWS]. */
    const uint8_t *src; /**< Bytes put writes, or the region hold records [BORROWS]. */
    size_t bytes;       /**< Byte count the call moves or records. */
    size_t off;         /**< Offset ahead of the tail that peek starts at. */
    size_t idx;         /**< Loculus or segment the call acts on. */
    embed_word mask;     /**< Mask loculus_next picks the lowest set bit of. */
    size_t *out;        /**< Set to the segment index a next or front call chose [BORROWS]. */
} InfinCtx;

/**
 * @brief Reads the ring state out of the caller's opaque storage.
 *
 * @param[in] r Ring storage the caller declared [BORROWS].
 * @return      The state positioned into it [BORROWS].
 * @note The cast goes through void *; mmgr_ring aligns opaque to size_t.
 * @warning The state is only valid after mmgr_infin_init returned EMBED_TRUE for this ring.
 */
EMBED_INLINE RingState *ring_of(mmgr_ring *r)
{
    return (RingState *)(void *)r->opaque;
}

/**
 * @brief Returns the bit that names loculus idx, or 0 when idx names none.
 *
 * @param[in] idx Loculus index.
 * @return        The bit, or 0.
 * @note The bound is here and not at each call site. A shift past the word never happens.
 * @note An out-of-range loculus names nothing, and so it reads as pinned and is never passed out.
 */
EMBED_INLINE embed_word ring_loculus_bit(size_t idx)
{
#if MMGR_RING_LOCULI == 0u
    // A build with no loculi names none, and so the bound below would compare against zero and always hold
    (void)idx;
    return (embed_word)0;
#else
    if (idx >= (size_t)MMGR_RING_LOCULI)
    {
        return (embed_word)0;
    }
    // Explicit casts build the loculus bit at embed_word width, as the masks it tests against
    return (embed_word)((embed_word)1 << idx);
#endif
}

/**
 * @brief Returns every loculus below MMGR_RING_LOCULI, as a mask.
 *
 * @return The mask.
 * @note Full width when the loculus count fills the word, the case a shift could not build.
 */
EMBED_INLINE embed_word ring_loculus_all(void)
{
    return RING_LOW_MASK(embed_word, (size_t)MMGR_RING_LOCULI, (size_t)MMGR_RING_LOCULI_MAX);
}

/**
 * @brief How a move of want bytes from offset at divides across the wrap.
 */
typedef struct
{
    size_t n;     /**< Bytes the move will actually carry. */
    size_t first; /**< Bytes of that before the end of the buffer. */
} RingRun;

/**
 * @brief Divides a move at the end of the buffer.
 *
 * @param[in] s    Ring state [BORROWS].
 * @param[in] at   Ring offset to start from.
 * @param[in] want Bytes the caller asked for.
 * @return         The count to carry and how much of it precedes the wrap.
 * @note Both directions divide the same way, and so the arithmetic lives here once.
 * @warning want stops at cap: two runs cannot express more than one lap. A larger count would
 *          take the second run past the end of the buffer.
 */
EMBED_INLINE RingRun ring_run(const RingState *s, size_t at, size_t want)
{
    const size_t n = (want > s->cap) ? s->cap : want;
    const size_t room = s->cap - at;
    RingRun r;

    r.n = n;
    r.first = (room < n) ? room : n;
    return r;
}

/**
 * @brief Moves want bytes out of the ring from offset at, in at max two runs.
 *
 * @param[in]  s    Ring state [BORROWS].
 * @param[in]  at   Ring offset to start from.
 * @param[out] dst  Destination [BORROWS].
 * @param[in]  want Bytes to move.
 * @return          The offset one past the bytes moved, wrapped.
 * @note The first pass stops at the end of the buffer and the second takes whatever wrapped. So the
 *       wrap costs one extra pass and not a test on every byte.
 * @note A loop and not a pass and a guarded second one, and so the mover comes out once instead of
 *       twice. Two copies of it make this entry too large to inline well, which costs far more than
 *       the loop does.
 */
EMBED_INLINE size_t ring_move_out(RingState *s, size_t at, uint8_t *dst, size_t want)
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
 * @brief Moves want bytes into the ring at offset at, in at max two runs.
 *
 * @param[in,out] s    Ring state [BORROWS].
 * @param[in]     at   Ring offset to start at.
 * @param[in]     src  Source bytes [BORROWS].
 * @param[in]     want Bytes to move.
 * @return             The offset one past the bytes moved, wrapped.
 * @note The mirror of ring_move_out, and the reason a fill and a drain cost the same per byte.
 */
EMBED_INLINE size_t ring_move_in(RingState *s, size_t at, const uint8_t *src, size_t want)
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
 * @note Takes the cursors and not the ring. A caller with one of them from its own load
 *       reaches the same arithmetic without reading it twice.
 */
EMBED_INLINE size_t ring_used(size_t h, size_t t, size_t cap)
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
 * @note One byte stays back so a full ring and an empty one never share a cursor pair.
 */
EMBED_INLINE size_t ring_free(size_t h, size_t t, size_t cap)
{
    return (cap - 1u) - ring_used(h, t, cap);
}

/**
 * @brief Returns the bytes the consumer may still read.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      Distance from tail to head, wrapped into the ring.
 */
EMBED_INLINE size_t infin_available(const InfinCtx *c)
{
    return ring_used(embed_atomic_usize_load(&c->s->head), embed_atomic_usize_load(&c->s->tail), c->s->cap);
}

/**
 * @brief Returns the bytes the producer may still write.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      cap minus one, minus the readable bytes.
 */
EMBED_INLINE size_t infin_vacant(const InfinCtx *c)
{
    return ring_free(embed_atomic_usize_load(&c->s->head), embed_atomic_usize_load(&c->s->tail), c->s->cap);
}

/**
 * @brief Takes one byte from the tail and advances past it.
 *
 * @param[in,out] c Ring and the destination byte [BORROWS].
 * @return          EMBED_TRUE when a byte came out, EMBED_FALSE when the ring was empty.
 */
EMBED_INLINE embed_bool infin_read_byte(const InfinCtx *c)
{
    const size_t t = embed_atomic_usize_load(&c->s->tail);

    if (t == embed_atomic_usize_load(&c->s->head))
    {
        return EMBED_FALSE;
    }
    *c->dst = c->s->buf[t];
    embed_atomic_usize_store(&c->s->tail, MMGR_RING_WRAP(t + 1u, c->s->cap));
    return EMBED_TRUE;
}

/**
 * @brief Takes up to c->bytes into c->dst and advances the tail once at the end.
 *
 * @param[in,out] c Ring, destination and the max to take [BORROWS].
 * @return          Bytes actually taken.
 * @note Publishes the tail once, after the move, and not per byte.
 */
EMBED_INLINE size_t infin_read(const InfinCtx *c)
{
    const size_t t = embed_atomic_usize_load(&c->s->tail);
    const size_t have = ring_used(embed_atomic_usize_load(&c->s->head), t, c->s->cap);
    const size_t n = (have < c->bytes) ? have : c->bytes;

    embed_atomic_usize_store(&c->s->tail, ring_move_out(c->s, t, c->dst, n));
    return n;
}

/**
 * @brief Copies c->bytes out of the ring from c->off past the tail, and leaves the tail alone.
 *
 * @param[in,out] c Ring, destination, byte count and start offset [BORROWS].
 * @warning Copies c->bytes regardless of whether that many are available.
 */
EMBED_INLINE void infin_peek(const InfinCtx *c)
{
    const size_t at = MMGR_RING_WRAP(embed_atomic_usize_load(&c->s->tail) + c->off, c->s->cap);

    (void)ring_move_out(c->s, at, c->dst, c->bytes);
}

/**
 * @brief Advances the tail past c->bytes.
 *
 * @param[in,out] c Ring and the byte count to drop [BORROWS].
 */
EMBED_INLINE void infin_consume(const InfinCtx *c)
{
    embed_atomic_usize_store(&c->s->tail, MMGR_RING_WRAP(embed_atomic_usize_load(&c->s->tail) + c->bytes, c->s->cap));
}

/**
 * @brief Writes c->bytes of c->src into the ring, or refuses the whole span.
 *
 * @param[in,out] c Ring, the bytes to write and their count [BORROWS].
 * @return          EMBED_TRUE when the span went in, EMBED_FALSE when it would not fit.
 * @note The head stays local across the move and publishes once, and so no half span is visible.
 */
EMBED_INLINE embed_bool infin_put(const InfinCtx *c)
{
    const size_t h = embed_atomic_usize_load(&c->s->head);

    if (c->bytes > ring_free(h, embed_atomic_usize_load(&c->s->tail), c->s->cap))
    {
        return EMBED_FALSE;
    }
    embed_atomic_usize_store(&c->s->head, ring_move_in(c->s, h, c->src, c->bytes));
    return EMBED_TRUE;
}

/**
 * @brief Returns the segments filled and not yet released.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      The distance between the two counters.
 */
EMBED_INLINE size_t infin_seg_inflight(const InfinCtx *c)
{
    return embed_atomic_usize_load(&c->s->claim) - embed_atomic_usize_load(&c->s->rel);
}

/**
 * @brief Passes back the segment a counter names, when its side says one is there.
 *
 * @param[in,out] c      Ring, and where to write the index [BORROWS].
 * @param[in]     cursor Counter that names the segment.
 * @param[in]     ok     Whether the caller's side has a segment to give.
 * @return               ok, and c->out is only written when it holds.
 * @note Both ends reduce to this: the counter goes into range by mask and reports. Only the test for
 *       whether a segment exists differs, and so each end retains its own and passes the answer down.
 */
EMBED_INLINE embed_bool ring_seg_pick(const InfinCtx *c, size_t cursor, embed_bool ok)
{
    if (!ok)
    {
        return EMBED_FALSE;
    }
    *c->out = cursor & (c->s->nsegs - 1u);
    return EMBED_TRUE;
}

/**
 * @brief Reports the index of the segment the producer fills next.
 *
 * @param[in,out] c Ring, and where to write the index [BORROWS].
 * @return          EMBED_FALSE when every segment is in flight.
 */
EMBED_INLINE embed_bool infin_seg_next(const InfinCtx *c)
{
    const size_t claim = embed_atomic_usize_load(&c->s->claim);

    return ring_seg_pick(c, claim, (claim - embed_atomic_usize_load(&c->s->rel)) < c->s->nsegs);
}

/**
 * @brief Advances one of the segment counters by one.
 *
 * @param[in,out] p Counter to advance [BORROWS].
 * @note A publish and a release are the same step on opposite counters, and so both reach this.
 * @note One side owns each counter, and so the read and the write need no atomicity between them.
 */
EMBED_INLINE void ring_bump(embed_atomic_usize *p)
{
    embed_atomic_usize_store(p, embed_atomic_usize_load(p) + 1u);
}

/**
 * @brief Makes the filled segment visible to the consumer.
 *
 * @param[in,out] c Ring to advance [BORROWS].
 */
EMBED_INLINE void infin_seg_publish(const InfinCtx *c)
{
    ring_bump(&c->s->claim);
}

/**
 * @brief Reports the index of the segment the consumer takes next.
 *
 * @param[in,out] c Ring, and where to write the index [BORROWS].
 * @return          EMBED_FALSE when none is in flight.
 */
EMBED_INLINE embed_bool infin_seg_front(const InfinCtx *c)
{
    const size_t rel = embed_atomic_usize_load(&c->s->rel);

    return ring_seg_pick(c, rel, embed_atomic_usize_load(&c->s->claim) != rel);
}

/**
 * @brief Frees the front segment.
 *
 * @param[in,out] c Ring to advance [BORROWS].
 */
EMBED_INLINE void infin_seg_release(const InfinCtx *c)
{
    ring_bump(&c->s->rel);
}

/**
 * @brief Returns the contiguous span of segment c->idx.
 *
 * @param[in] c Ring and the segment index [BORROWS].
 * @return      Its first byte inside the ring buffer [BORROWS].
 */
EMBED_INLINE uint8_t *infin_seg_at(const InfinCtx *c)
{
    return &c->s->buf[c->idx * c->s->seg];
}

/**
 * @brief Returns the loculi that are free and not pinned.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      The free mask with the pinned ones cleared, bounded to the loculi this build has.
 */
EMBED_INLINE embed_word infin_loculus_ready(const InfinCtx *c)
{
    return embed_atomic_word_load(&c->s->gratis) & ~embed_atomic_word_load(&c->s->pinned) & ring_loculus_all();
}

/**
 * @brief Returns the index of the lowest set bit of c->mask.
 *
 * @param[in] c The mask to pick from [BORROWS].
 * @return      The index, or -1 when c->mask is empty.
 */
EMBED_INLINE embed_iword infin_loculus_next(const InfinCtx *c)
{
    if (c->mask == (embed_word)0)
    {
        return (embed_iword)-1;
    }
    return ring_trail(c->mask);
}

/**
 * @brief Takes loculus c->idx and records the region it retains out.
 *
 * @param[in,out] c Ring, the loculus, and the region to record [BORROWS].
 * @return          EMBED_TRUE when this caller took the loculus.
 */
EMBED_INLINE embed_bool infin_loculus_hold(const InfinCtx *c)
{
    const embed_word bit = ring_loculus_bit(c->idx);

    if (bit == (embed_word)0)
    {
        return EMBED_FALSE;
    }

    const embed_word prev = embed_atomic_word_fetch_or(&c->s->pinned, bit);

    if ((prev & bit) != 0u)
    {
        return EMBED_FALSE;
    }
    // Explicit cast drops the source qualifier: the span records a region it never writes through
    c->s->retain[c->idx].buf = (uint8_t *)(uintptr_t)c->src;
    c->s->retain[c->idx].cap = c->bytes;
    c->s->retain[c->idx].pos = 0u;
    return EMBED_TRUE;
}

/**
 * @brief Returns the region loculus c->idx retains out.
 *
 * @param[in] c Ring and the loculus [BORROWS].
 * @return      The recorded span, or NULL when c->idx names none [BORROWS].
 */
EMBED_INLINE const mmgr_ring_span *infin_loculus_keepout(const InfinCtx *c)
{
#if MMGR_RING_LOCULI == 0u
    // A build with no loculi retains nothing out, and so there is no span to hand back
    (void)c;
    return NULL;
#else
    if (c->idx >= (size_t)MMGR_RING_LOCULI)
    {
        return NULL;
    }
    return &c->s->retain[c->idx];
#endif
}

/**
 * @brief Gives loculus c->idx back.
 *
 * @param[in,out] c Ring and the loculus [BORROWS].
 * @note Leaves the recorded span and the bytes alone. A restream can run again.
 */
EMBED_INLINE void infin_loculus_drop(const InfinCtx *c)
{
    // Explicit cast retains the complement at embed_word width, as the atomic it clears
    (void)embed_atomic_word_fetch_and(&c->s->pinned, (embed_word)~ring_loculus_bit(c->idx));
}

/**
 * @brief Marks loculus c->idx free.
 *
 * @param[in,out] c Ring and the loculus [BORROWS].
 */
EMBED_INLINE void infin_loculus_mark(const InfinCtx *c)
{
    (void)embed_atomic_word_fetch_or(&c->s->gratis, ring_loculus_bit(c->idx));
}

/**
 * @brief Lays a fresh ring into c->ring, over the bytes at c->buf.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
embed_bool mmgr_infin_init(const InfinCfg *c)
{
    MMGR_ASSERT(c->ring != NULL, "a ring needs storage");
    MMGR_ASSERT(c->buf != NULL, "a ring needs a buffer");

    if ((c->cap == 0u) || !MMGR_RING_POW2(c->cap))
    {
        return EMBED_FALSE;
    }
    if ((c->nsegs == 0u) || !MMGR_RING_POW2(c->nsegs) || (c->nsegs > c->cap))
    {
        return EMBED_FALSE;
    }

    RingState *const s = ring_of(c->ring);

    s->buf = c->buf;
    s->cap = c->cap;
    s->nsegs = c->nsegs;
    s->seg = c->cap / c->nsegs;
    embed_atomic_usize_store(&s->head, 0u);
    embed_atomic_usize_store(&s->tail, 0u);
    embed_atomic_usize_store(&s->claim, 0u);
    embed_atomic_usize_store(&s->rel, 0u);
    embed_atomic_word_store(&s->gratis, ring_loculus_all());
    embed_atomic_word_store(&s->pinned, (embed_word)0);
#if MMGR_RING_LOCULI > 0u
    for (size_t i = 0; i < (size_t)MMGR_RING_LOCULI; i++)
    {
        s->retain[i].buf = NULL;
        s->retain[i].cap = 0u;
        s->retain[i].pos = 0u;
    }
#endif
    return EMBED_TRUE;
}

/**
 * @brief The state argument every entry point but one forwards.
 *
 * @note Names args from the entry point's own parameter, and so the table below carries fields alone.
 */
#define RING_S .s = ring_of(args->ring)

/**
 * @brief Binds this module's four fixed arguments to EMBED_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_infin_ and infin_ prefixes, which the two share.
 * @note The prefixes and the two structure types are the same for every entry. So their names appear
 *       once here, and the table below states only what each entry differs in.
 * @note The variadic part is the argument pack, never empty, and so no comma has to go.
 */
#define RING_ENTRY(ret, name, ...) EMBED_ENTRY(mmgr_infin_, infin_, InfinCtx, InfinCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to EMBED_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_infin_ and infin_ prefixes, which the two share.
 * @note Separate from RING_ENTRY because a void function cannot return an expression, and so the
 *       two cannot share one body.
 */
#define RING_ENTRY_V(name, ...) EMBED_ENTRY_V(mmgr_infin_, infin_, InfinCtx, InfinCfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note The declaration of each in confinium_exclusivum_infinitas.h describes it.
 */
RING_ENTRY(size_t, available, RING_S)
RING_ENTRY(size_t, vacant, RING_S)
RING_ENTRY(embed_bool, read_byte, RING_S, .dst = args->dst)
RING_ENTRY(size_t, read, RING_S, .dst = args->dst, .bytes = args->bytes)
RING_ENTRY_V(peek, RING_S, .dst = args->dst, .bytes = args->bytes, .off = args->off)
RING_ENTRY_V(consume, RING_S, .bytes = args->bytes)
RING_ENTRY(embed_bool, put, RING_S, .src = args->src, .bytes = args->bytes)
RING_ENTRY(size_t, seg_inflight, RING_S)
RING_ENTRY(embed_bool, seg_next, RING_S, .out = args->out)
RING_ENTRY_V(seg_publish, RING_S)
RING_ENTRY(embed_bool, seg_front, RING_S, .out = args->out)
RING_ENTRY_V(seg_release, RING_S)
RING_ENTRY(uint8_t *, seg_at, RING_S, .idx = args->idx)
RING_ENTRY(embed_word, loculus_ready, RING_S)
RING_ENTRY(embed_iword, loculus_next, .mask = args->mask)
RING_ENTRY(embed_bool, loculus_hold, RING_S, .idx = args->idx, .src = args->src, .bytes = args->bytes)
RING_ENTRY(const mmgr_ring_span *, loculus_keepout, RING_S, .idx = args->idx)
RING_ENTRY_V(loculus_drop, RING_S, .idx = args->idx)
RING_ENTRY_V(loculus_mark, RING_S, .idx = args->idx)
