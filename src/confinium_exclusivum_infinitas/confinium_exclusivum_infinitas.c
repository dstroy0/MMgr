/**
 * @brief Power-of-two ring with segment holds, drain runs and one exclusive writer grant.
 *
 * @note head, tail, held and slots are atomic; every other member of RingState is plain.
 */
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include <stdatomic.h>

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
 * @brief Clears the given bits of an atomic word.
 *
 * @param[in] p    Address of the atomic to modify [BORROWS].
 * @param[in] bits Mask of bits to clear.
 * @note Discards the fetched value, so it reports nothing about which bits were set.
 */
#define MMGR_ATOMIC_CLEAR(p, bits) ((void)atomic_fetch_and_explicit((p), (mmgr_word) ~(bits), memory_order_release))

/**
 * @brief A reader's position within a frame of the ring.
 *
 * @note Declared opaquely in the header, so callers hold only a pointer.
 */
struct MmgrCursor
{
    size_t base; /**< Ring offset the frame starts at. */
    size_t span; /**< Bytes in the frame. */
    size_t off;  /**< Current position, measured from base and never past span. */
};

/**
 * @brief One in-flight drain run: a span of segments being handed out a segment at a time.
 *
 * @note first equal to last marks the entry as free.
 */
typedef struct
{
    size_t first; /**< First segment of the run. */
    size_t last;  /**< One past the last segment of the run. */
    size_t next;  /**< Next segment to hand out. */
    size_t gen;   /**< Generation, bumped on retire so old tesserae stop matching. */
} Drain;

/**
 * @brief Number of tessera holders: one per drain entry, plus one for the exclusive grant.
 */
#define MMGR_TESSERA_SLOTS (MMGR_RING_DRAINS + 1u)

/**
 * @brief Holder index the exclusive grant uses, sitting just past the drain entries.
 */
#define MMGR_TESSERA_SING MMGR_RING_DRAINS

/**
 * @brief Packs a holder index and a generation into a tessera.
 *
 * @param[in] idx Holder index, below MMGR_TESSERA_SLOTS.
 * @param[in] gen Generation of that holder.
 * @return        The tessera, always non-zero.
 * @note The trailing plus one reserves 0 to mean "no tessera held".
 */
#define MMGR_TESSERA(idx, gen) (((gen) * MMGR_TESSERA_SLOTS) + (idx) + 1u)

/**
 * @brief Recovers the holder index from a tessera.
 *
 * @param[in] t Tessera built by MMGR_TESSERA.
 * @return      The holder index.
 * @warning A t of 0 is not a valid tessera; every caller tests for it before calling this.
 */
#define MMGR_TESSERA_IDX(t) ((((t) - 1u)) % MMGR_TESSERA_SLOTS)

/**
 * @brief Recovers the generation from a tessera.
 *
 * @param[in] t Tessera built by MMGR_TESSERA.
 * @return      The generation the holder had when the tessera was issued.
 * @note Comparing this against the holder's current generation is what detects a stale tessera.
 */
#define MMGR_TESSERA_GEN(t) ((((t) - 1u)) / MMGR_TESSERA_SLOTS)

/**
 * @brief The exclusive writer attachment, and the region currently granted to it.
 *
 * @note open records the attachment; span records a live grant, and is 0 between grants.
 */
typedef struct
{
    const void *owner; /**< Attached writer's identity [BORROWS]. */
    size_t gran;       /**< Granule size every grant is a whole multiple of. */
    size_t at;         /**< Ring offset of the granted region. */
    size_t span;       /**< Bytes granted, or 0 when nothing is outstanding. */
    size_t gen;        /**< Generation, bumped on commit and detach so old tesserae stop matching. */
    mmgr_bool open;    /**< Whether a writer is attached. */
} Singularitas;

/**
 * @brief The whole ring, laid into the caller's mmgr_ring storage.
 *
 * @note head, tail, held and slots are atomic; the rest is touched only under those.
 * @warning Reached by casting mmgr_ring::opaque; the assertion below checks RingState fits inside it.
 */
typedef struct
{
    uint8_t *buf;                   /**< Ring bytes [BORROWS]. */
    size_t cap;                     /**< Bytes in buf, always a power of two. */
    size_t nsegs;                   /**< Segments the ring is divided into, a power of two. */
    size_t seg;                     /**< Bytes per segment, cap divided by nsegs. */
    _Atomic mmgr_word *held;        /**< One bit per segment, set while a holder owns it [BORROWS]. */
    _Atomic size_t head;            /**< Write position, advanced by the producer. */
    _Atomic size_t tail;            /**< Read position, advanced by the consumer. */
    struct MmgrCursor ord;          /**< The single cursor handed out by open. */
    const void *owner;              /**< Identity that opened the cursor [BORROWS]. */
    mmgr_bool open;                 /**< Whether the cursor is currently handed out. */
    _Atomic mmgr_word slots;        /**< One bit per drains entry, set while that entry is in use. */
    Singularitas sing;              /**< The exclusive writer attachment and its grant. */
    Drain drains[MMGR_RING_DRAINS]; /**< In-flight drain runs. */
} RingState;

/**
 * @brief Pins the opaque ring storage large enough, and the status packing consistent.
 *
 * @note The first fires when MMGR_RING_WORDS no longer covers RingState.
 * @note The other two keep the segment index and the flags inside their own octet of a status.
 */
MMGR_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

MMGR_STATIC_ASSERT((size_t)MMGR_RING_LOCULI_MAX <= (size_t)0xFF,
                   "a segment index has to fit the upper octet of a status");
MMGR_STATIC_ASSERT(MMGR_SING_REFUSED < ((mmgr_u16)1 << MMGR_SING_FLAG_BITS),
                   "a status flag has to fit the lower octet of a status");

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
 * @brief Arguments for every backend in this file, grouped by the calls that read them.
 *
 * @note Each backend reads one group; MMGR_CALL zeroes the members it is not given.
 */
typedef struct
{
    RingState *s;                /**< Ring to act on [BORROWS]. */
    struct MmgrCursor *cur;      /**< Cursor for seek [BORROWS]. */
    uint8_t *dst;                /**< Destination for read_byte and peek [BORROWS]. */
    const uint8_t *src;          /**< Bytes to publish through sing_put [BORROWS]. */
    size_t bytes;                /**< Byte count, or granule count on the singularitas path. */
    size_t off;                  /**< Ring offset, or cursor position for seek. */
    size_t from;                 /**< First byte of a drain run. */
    size_t to;                   /**< One past the last byte of a drain run. */
    size_t *tessera;             /**< Caller's tessera, read and rewritten in place [BORROWS]. */
    const void *owner;           /**< Identity opening the cursor [BORROWS]. */
    size_t *units;               /**< Set to the granules actually granted [BORROWS]. */
    const SingularitasCfg *sing; /**< Attachment request [BORROWS]. */
    mmgr_u16 *status;            /**< Set to the packed status word [BORROWS]. */
    mmgr_u16 why;                /**< Flags to fold into that status word. */
    mmgr_bool rearm;             /**< Ask for a fresh grant right after committing one. */
} InfinCtx;

/**
 * @brief Arguments for the drain backends that act on one entry.
 */
typedef struct
{
    RingState *s; /**< Ring the entry belongs to [BORROWS]. */
    Drain *d;     /**< Entry to act on [BORROWS]. */
} DrainCtx;

/**
 * @brief Returns the bytes the consumer may still read.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      Distance from tail to head, wrapped into the ring.
 * @note Loads head and tail separately, so the answer may be stale by the time it is used.
 */
MMGR_INLINE size_t infin_available(const InfinCtx *c)
{
    return MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->head) - MMGR_ATOMIC_LOAD(&c->s->tail), c->s->cap);
}

/**
 * @brief Returns the bytes the producer may still write.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      cap minus one, minus the readable bytes, minus any outstanding grant.
 * @note sing.span is non-zero only while a grant is outstanding.
 */
MMGR_INLINE size_t infin_vacant(const InfinCtx *c)
{
    return (c->s->cap - 1u) - infin_available(c) - c->s->sing.span;
}

/**
 * @brief Takes one byte from the tail and advances past it.
 *
 * @param[in,out] c Ring and the destination byte [BORROWS].
 * @return          MMGR_TRUE when a byte was taken, MMGR_FALSE when the ring was empty.
 * @note Writes through c->dst only when it returns MMGR_TRUE.
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
 * @brief Points at c->bytes contiguous readable bytes without consuming them.
 *
 * @param[in] c Ring and the byte count wanted [BORROWS].
 * @return      Address inside the ring buffer, or NULL [BORROWS].
 * @note Returns NULL when the ring is empty, when fewer bytes are available, or when the run would wrap.
 * @warning The bytes stay valid only until the consumer advances the tail.
 */
MMGR_INLINE const uint8_t *infin_read(const InfinCtx *c)
{
    const size_t have = infin_available(c);
    if ((have == 0u) || (c->bytes > have))
    {
        return NULL;
    }

    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    if (c->bytes > (c->s->cap - t))
    {
        return NULL;
    }
    return &c->s->buf[t];
}

/**
 * @brief Copies c->bytes out of the ring starting c->off past the tail, leaving the tail alone.
 *
 * @param[in,out] c Ring, destination, byte count and starting offset [BORROWS].
 * @note Wraps at the end of the buffer, so the copy may be drawn from two runs.
 * @warning Copies c->bytes whether or not that many are available.
 */
MMGR_INLINE void infin_peek(const InfinCtx *c)
{
    size_t at = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->off, c->s->cap);

    for (size_t i = 0; i < c->bytes; i++)
    {
        c->dst[i] = c->s->buf[at];
        at = MMGR_RING_WRAP(at + 1u, c->s->cap);
    }
}

/**
 * @brief Advances the tail past c->bytes.
 *
 * @param[in,out] c Ring and the byte count to drop [BORROWS].
 * @warning Advances whether or not that many bytes were available.
 */
MMGR_INLINE void infin_consume(const InfinCtx *c)
{
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->bytes, c->s->cap));
}

/**
 * @brief Measures how many of the next c->bytes from c->off lie in segments nobody holds.
 *
 * @param[in] c Ring, the starting offset and the byte count wanted [BORROWS].
 * @return      Bytes reachable before the first held segment, at most c->bytes.
 * @note Returns the whole request without walking when no segment is held at all.
 * @note Steps segment by segment, so a partial first segment is counted from c->off.
 */
MMGR_INLINE size_t infin_room(const InfinCtx *c)
{
    RingState *const s = c->s;
    const mmgr_word bits = MMGR_ATOMIC_LOAD(s->held);
    const size_t want = c->bytes;
    size_t at = c->off;

    if (bits == 0u)
    {
        return want;
    }

    size_t room = 0;
    while (room < want)
    {
        const size_t seg = at / s->seg;

        // Explicit casts build the segment's bit at mmgr_word width before the test
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

/**
 * @brief Moves a cursor to c->off within its frame.
 *
 * @param[in,out] c Ring, the cursor and the new position [BORROWS].
 * @warning c->off must not exceed the cursor's span.
 */
MMGR_INLINE void infin_seek(const InfinCtx *c)
{
    MMGR_ASSERT(c->off <= c->cur->span, "a cursor cannot be moved outside its frame");
    c->cur->off = c->off;
}

/**
 * @brief Builds the bit mask covering segments first through last, last excluded.
 *
 * @param[in] first First segment in the run.
 * @param[in] last  One past the last segment in the run.
 * @return          A mask with one bit set per segment in the run.
 * @note An empty run, where first equals last, gives 0.
 */
#define MMGR_SEG_RUN(first, last) ((mmgr_word)(MMGR_SEG_BELOW(last) & ~MMGR_SEG_BELOW(first)))

/**
 * @brief Builds the bit mask covering every segment below k.
 *
 * @param[in] k Segment index to stop below.
 * @return      A mask with the low k bits set.
 * @note A k at or above MMGR_RING_LOCULI_MAX gives all ones, avoiding a shift by the full width.
 */
#define MMGR_SEG_BELOW(k)                                                                                              \
    ((mmgr_word)(((k) >= MMGR_RING_LOCULI_MAX) ? (mmgr_word) ~(mmgr_word)0                                             \
                                               : (mmgr_word)(((mmgr_word)1 << (k)) - (mmgr_word)1)))

/**
 * @brief Resolves a tessera to the drain entry it names, if that entry is still the same one.
 *
 * @param[in] c Ring and the caller's tessera [BORROWS].
 * @return      The entry, or NULL when the tessera names none [BORROWS].
 * @note Returns NULL for a tessera of 0, for one naming the exclusive grant, for a retired entry,
 *       and for one whose generation no longer matches.
 */
MMGR_INLINE Drain *drain_of(const InfinCtx *c)
{
    const size_t tessera = *c->tessera;

    if (tessera == 0u)
    {
        return NULL;
    }

    const size_t idx = MMGR_TESSERA_IDX(tessera);
    if (idx >= MMGR_RING_DRAINS)
    {
        return NULL;
    }

    Drain *const d = &c->s->drains[idx];

    if ((d->first == d->last) || (d->gen != MMGR_TESSERA_GEN(tessera)))
    {
        return NULL;
    }
    return d;
}

/**
 * @brief Ends a drain run, releasing its segments and its entry.
 *
 * @param[in,out] c Ring and the entry to retire [BORROWS].
 * @note Bumps the entry's generation, so any tessera still naming it stops resolving.
 * @note Clears the run's segment bits first, then the entry's own bit in slots.
 */
MMGR_INLINE void drain_retire(const DrainCtx *c)
{
    Drain *const d = c->d;
    // Explicit cast converts the pointer difference into the size_t entry index
    const size_t slot = (size_t)(d - &c->s->drains[0]);

    MMGR_ATOMIC_CLEAR(c->s->held, MMGR_SEG_RUN(d->first, d->last));
    d->first = 0u;
    d->last = 0u;
    d->next = 0u;
    d->gen++;
    // Explicit casts build the entry's bit at mmgr_word width, matching the atomic it clears
    MMGR_ATOMIC_CLEAR(&c->s->slots, (mmgr_word)((mmgr_word)1 << slot));
}

/**
 * @brief Takes every segment covering c->off through c->off plus c->bytes, all or nothing.
 *
 * @param[in,out] c Ring, the starting offset and the byte count [BORROWS].
 * @return          MMGR_TRUE when the whole run was taken, MMGR_FALSE when any part was already held.
 * @note One atomic fetch-or takes the run; on refusal only the bits this call set are given back.
 * @note The run stays held until segs_drop or drain_retire clears those bits.
 */
MMGR_INLINE mmgr_bool segs_claim(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t first = c->off / s->seg;
    const size_t last = (c->off + c->bytes + s->seg - 1u) / s->seg;
    const mmgr_word want = MMGR_SEG_RUN(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);

    if ((prev & want) != 0)
    {
        // Explicit cast keeps the complement at mmgr_word width, so only bits this call set are cleared
        MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return MMGR_FALSE;
    }
    return MMGR_TRUE;
}

/**
 * @brief Releases every segment covering c->off through c->off plus c->bytes.
 *
 * @param[in,out] c Ring, the starting offset and the byte count [BORROWS].
 * @warning Clears the bits whether or not this caller set them.
 */
MMGR_INLINE void segs_drop(const InfinCtx *c)
{
    RingState *const s = c->s;

    MMGR_ATOMIC_CLEAR(s->held, MMGR_SEG_RUN(c->off / s->seg, (c->off + c->bytes + s->seg - 1u) / s->seg));
}

/**
 * @brief Grants the exclusive writer a run at the head, and holds its segments.
 *
 * @param[in,out] c Ring, the granules wanted in c->bytes, and where to report what was given [BORROWS].
 * @return          Start of the granted run, or NULL when nothing could be granted [BORROWS].
 * @note A c->bytes of 0 asks for whatever fits, and requires c->units to receive the answer.
 * @note The run is trimmed to the buffer end, to unheld segments, then down to a whole granule count.
 * @note A fixed request is refused outright unless exactly that many granules survive the trimming.
 * @warning Refuses while s->sing.span is non-zero, which is what marks an outstanding grant.
 */
MMGR_INLINE uint8_t *sing_claim(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t units = c->bytes;
    size_t *const given = c->units;

    if (s->sing.span != 0u)
    {
        return NULL;
    }
    if ((units == 0u) && (given == NULL))
    {
        return NULL;
    }

    const size_t head = MMGR_ATOMIC_LOAD(&s->head);
    size_t len = ((s->cap - 1u) - MMGR_RING_WRAP(head - MMGR_ATOMIC_LOAD(&s->tail), s->cap));

    if (units != 0u)
    {
        const size_t want = units * s->sing.gran;
        if (want > len)
        {
            return NULL;
        }
        len = want;
    }
    if (len > (s->cap - head))
    {
        len = s->cap - head;
    }
    len = MMGR_CALL(infin_room, InfinCtx, .s = s, .off = head, .bytes = len);
    len -= (len % s->sing.gran);

    if ((len == 0u) || ((units != 0u) && (len != (units * s->sing.gran))))
    {
        return NULL;
    }
    if (!MMGR_CALL(segs_claim, InfinCtx, .s = s, .off = head, .bytes = len))
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

/**
 * @brief Publishes c->bytes of the outstanding grant and releases the rest.
 *
 * @param[in,out] c Ring and the bytes actually written [BORROWS].
 * @note A c->bytes above the granted span is trimmed down to it, so head never passes the grant.
 * @note Moves head to the end of what was written, then drops the whole granted run's segments.
 * @note Bumps the grant generation, so the tessera that named it stops resolving.
 */
MMGR_INLINE void sing_commit(const InfinCtx *c)
{
    RingState *const s = c->s;
    size_t bytes = c->bytes;

    if (bytes > s->sing.span)
    {
        bytes = s->sing.span;
    }
    MMGR_ATOMIC_STORE(&s->head, MMGR_RING_WRAP(s->sing.at + bytes, s->cap));
    MMGR_CALL(segs_drop, InfinCtx, .s = s, .off = s->sing.at, .bytes = s->sing.span);
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen++;
}

/**
 * @brief Writes c->bytes granules from c->src into the ring and publishes them at once.
 *
 * @param[in,out] c Ring, the source bytes and the granule count [BORROWS].
 * @return          Start of what was written, or NULL when it would not fit [BORROWS].
 * @note Copies in runs that stop at the buffer end, so a write may wrap.
 * @note Checks unheld room but takes no hold of its own; head advances before it returns.
 * @warning Refuses while a grant is outstanding, and refuses a granule count of 0.
 */
MMGR_INLINE uint8_t *sing_put(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t n = c->bytes * s->sing.gran;
    size_t at = MMGR_ATOMIC_LOAD(&s->head);

    if ((n == 0u) || (s->sing.span != 0u) || (n > ((s->cap - 1u) - infin_available(c))))
    {
        return NULL;
    }
    if (MMGR_CALL(infin_room, InfinCtx, .s = s, .off = at, .bytes = n) < n)
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
        MMGR_CALL(proxim.read, ProximusCfg, .dst = &s->buf[at], .at = src, .size = chunk);
        at = MMGR_RING_WRAP(at + chunk, s->cap);
        src += chunk;
        left -= chunk;
    }
    MMGR_ATOMIC_STORE(&s->head, at);
    return first;
}

/**
 * @brief Runs one exclusive writer step: publish, commit, or grant.
 *
 * @param[in,out] c Ring, plus whichever of src, tessera, bytes, off and units the step needs [BORROWS].
 * @return          Start of a granted or written run, or NULL [BORROWS].
 * @note A non-NULL c->src publishes straight away through sing_put and grants nothing.
 * @note A live tessera first commits c->off granules, then clears it; a new grant follows only when rearm is set.
 * @note A grant that succeeds writes a fresh tessera back through c->tessera.
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
        MMGR_CALL(sing_commit, InfinCtx, .s = s, .bytes = c->off * s->sing.gran);
        *c->tessera = 0u;
        if (!c->rearm)
        {
            return NULL;
        }
    }

    uint8_t *const at = MMGR_CALL(sing_claim, InfinCtx, .s = s, .bytes = c->bytes, .units = c->units);
    if ((at != NULL) && (c->tessera != NULL))
    {
        *c->tessera = MMGR_TESSERA(MMGR_TESSERA_SING, s->sing.gen);
    }
    return at;
}

/**
 * @brief Lays a fresh ring into the caller's storage and clears every counter.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
mmgr_bool mmgr_infin_init(const RingCfg *c)
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
    // Explicit cast compares the segment count against the bit-count limit at size_t width
    if (c->nsegs > (size_t)MMGR_RING_LOCULI_MAX)
    {
        return MMGR_FALSE;
    }

    RingState *const s = ring_of(c->ring);
    s->buf = c->buf;
    s->cap = c->cap;
    s->nsegs = c->nsegs;
    s->seg = c->cap / c->nsegs;
    s->held = c->held;
    atomic_init(&s->head, 0u);
    atomic_init(&s->tail, 0u);
    // Explicit cast types the zero to mmgr_word, matching the atomic it is stored into
    MMGR_ATOMIC_STORE(s->held, (mmgr_word)0);
    s->ord.base = 0u;
    s->ord.span = c->cap;
    s->ord.off = 0u;
    s->owner = NULL;
    s->open = MMGR_FALSE;
    atomic_init(&s->slots, (mmgr_word)0);
    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen = 0u;
    s->sing.open = MMGR_FALSE;
    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        s->drains[i].first = 0u;
        s->drains[i].last = 0u;
        s->drains[i].next = 0u;
        s->drains[i].gen = 0u;
    }
    return MMGR_TRUE;
}

/**
 * @brief Hands out the ring's single cursor, rewound to the start of its frame.
 *
 * @param[in,out] c Ring and the identity taking the cursor [BORROWS].
 * @return          The cursor, or NULL when it is already out [BORROWS].
 * @note Records c->owner in the ring state.
 * @warning One cursor per ring; only mmgr_infin_init clears s->open.
 */
MMGR_INLINE struct MmgrCursor *infin_open(const InfinCtx *c)
{
    RingState *const s = c->s;

    if (s->open)
    {
        return NULL;
    }
    s->ord.off = 0u;
    s->open = MMGR_TRUE;
    s->owner = c->owner;
    return &s->ord;
}

#if MMGR_ENABLE_KEEPOUT
/**
 * @brief Hands out the next segment of an in-flight drain run.
 *
 * @param[in,out] c Ring and the caller's tessera [BORROWS].
 * @return          Start of the next segment, or NULL when the run is finished [BORROWS].
 * @note Retires the run and clears the caller's tessera once every segment has been handed out.
 * @note Also returns NULL when the tessera no longer resolves to a live run.
 */
MMGR_INLINE const uint8_t *drain_step(const InfinCtx *c)
{
    RingState *const s = c->s;
    Drain *const d = MMGR_CALL(drain_of, InfinCtx, .s = s, .tessera = c->tessera);

    if (d == NULL)
    {
        return NULL;
    }
    if (d->next >= d->last)
    {
        MMGR_CALL(drain_retire, DrainCtx, .s = s, .d = d);
        *c->tessera = 0u;
        return NULL;
    }

    const size_t give = d->next;
    d->next++;
    return &s->buf[give * s->seg];
}

/**
 * @brief Reports whether the byte range c->from to c->to has arrived and not yet been consumed.
 *
 * @param[in] c Ring and the range to test [BORROWS].
 * @return      MMGR_TRUE when the whole range lies between the tail and the head.
 * @note Measures the range relative to the tail, so the test survives the ring wrapping.
 */
MMGR_INLINE mmgr_bool drain_in_flight(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t tail = MMGR_ATOMIC_LOAD(&s->tail);
    const size_t arrived = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&s->head) - tail, s->cap);
    const size_t rel = MMGR_RING_WRAP(c->from - tail, s->cap);

    // Explicit cast converts the combined test into the mmgr_bool return
    return (mmgr_bool)((arrived != 0u) && (rel < arrived) && ((c->to - c->from) <= (arrived - rel)));
}

/**
 * @brief Starts a drain run over the segments covering c->from to c->to.
 *
 * @param[in,out] c Ring, the range, and where to write the tessera [BORROWS].
 * @return          Start of the first segment, or NULL when the run could not start [BORROWS].
 * @note Refuses an empty range, a range not yet arrived, or one reaching past the last segment.
 * @note Takes the whole segment run at once, then looks for a free drains entry to record it in.
 * @warning Gives the segments straight back when every drains entry is busy.
 */
MMGR_INLINE const uint8_t *drain_claim(const InfinCtx *c)
{
    RingState *const s = c->s;

    if ((c->to <= c->from) || !drain_in_flight(c))
    {
        return NULL;
    }

    const size_t first = c->from / s->seg;
    const size_t last = (c->to + s->seg - 1u) / s->seg;
    if (last > s->nsegs)
    {
        return NULL;
    }

    const mmgr_word want = MMGR_SEG_RUN(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);
    if ((prev & want) != 0)
    {
        // Explicit cast keeps the complement at mmgr_word width, so only bits this call set are cleared
        MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return NULL;
    }

    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        // Explicit casts build the entry's bit at mmgr_word width, matching the atomic it is fetched into
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
}

/**
 * @brief Continues an in-flight drain run, or starts a new one.
 *
 * @param[in,out] c Ring, the range, and the caller's tessera [BORROWS].
 * @return          Start of the segment handed out, or NULL [BORROWS].
 * @note A non-zero tessera steps the existing run; a zero one starts a new one over c->from to c->to.
 */
MMGR_INLINE const uint8_t *infin_drain(const InfinCtx *c)
{
    if ((c->tessera != NULL) && (*c->tessera != 0u))
    {
        return drain_step(c);
    }
    return drain_claim(c);
}
#endif

/**
 * @brief Hands out the ring's cursor to c->owner.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
struct MmgrCursor *mmgr_infin_open(const InfinCfg *c)
{
    return MMGR_CALL(infin_open, InfinCtx, .s = ring_of(c->ring), .owner = c->owner);
}

/**
 * @brief Hands out the next drain segment, or starts a run over c->from to c->to.
 *
 * @note Returns NULL without touching the ring when MMGR_ENABLE_KEEPOUT is 0.
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
const uint8_t *mmgr_infin_drain(const InfinCfg *c)
{
#if !MMGR_ENABLE_KEEPOUT
    (void)c;
    return NULL;
#else
    return MMGR_CALL(infin_drain, InfinCtx, .s = ring_of(c->ring), .from = c->from, .to = c->to, .tessera = c->tessera);
#endif
}

/**
 * @brief Returns the bytes waiting to be read.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
size_t mmgr_infin_available(const InfinCfg *c)
{
    return MMGR_CALL(infin_available, InfinCtx, .s = ring_of(c->ring));
}

/**
 * @brief Returns the bytes still free to write.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
size_t mmgr_infin_vacant(const InfinCfg *c)
{
    return MMGR_CALL(infin_vacant, InfinCtx, .s = ring_of(c->ring));
}

/**
 * @brief Takes one byte into c->dst and advances the tail.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
mmgr_bool mmgr_infin_read_byte(const InfinCfg *c)
{
    return MMGR_CALL(infin_read_byte, InfinCtx, .s = ring_of(c->ring), .cur = c->cur, .dst = c->dst);
}

/**
 * @brief Points at c->bytes contiguous readable bytes without consuming them.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
const uint8_t *mmgr_infin_read(const InfinCfg *c)
{
    return MMGR_CALL(infin_read, InfinCtx, .s = ring_of(c->ring), .cur = c->cur, .bytes = c->bytes);
}

/**
 * @brief Copies c->bytes from c->off past the tail into c->dst, leaving the tail alone.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
void mmgr_infin_peek(const InfinCfg *c)
{
    MMGR_CALL(infin_peek, InfinCtx, .s = ring_of(c->ring), .cur = c->cur, .dst = c->dst, .bytes = c->bytes, .off = c->off);
}

/**
 * @brief Advances the tail past c->bytes.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
void mmgr_infin_consume(const InfinCfg *c)
{
    MMGR_CALL(infin_consume, InfinCtx, .s = ring_of(c->ring), .cur = c->cur, .bytes = c->bytes);
}

/**
 * @brief Attaches an exclusive writer, or confirms the one already attached.
 *
 * @param[in,out] c Ring and the attachment request [BORROWS].
 * @return          MMGR_TRUE when the caller now holds the attachment.
 * @note With a writer already attached, this only checks the owner matches and changes nothing.
 * @warning The granule must be a non-zero power of two, no larger than MMGR_SING_GRANULE_MAX or the ring.
 */
MMGR_INLINE mmgr_bool sing_admit(const InfinCtx *c)
{
    RingState *const s = c->s;
    const SingularitasCfg *const sing = c->sing;

    if (s->sing.open)
    {
        // Explicit cast converts the identity comparison into the mmgr_bool return
        return (mmgr_bool)(sing->owner == s->sing.owner);
    }
    if ((sing->gran == 0u) || !MMGR_RING_POW2(sing->gran) || (sing->gran > MMGR_SING_GRANULE_MAX) ||
        (sing->gran > s->cap))
    {
        return MMGR_FALSE;
    }
    s->sing.owner = sing->owner;
    s->sing.gran = sing->gran;
    s->sing.open = MMGR_TRUE;
    return MMGR_TRUE;
}

/**
 * @brief Reports whether the caller's tessera still names the outstanding grant.
 *
 * @param[in] c Ring and the caller's tessera [BORROWS].
 * @return      MMGR_TRUE when a grant is outstanding and the tessera matches it.
 * @note Checks the holder index names the grant, and that the generation has not moved on.
 */
MMGR_INLINE mmgr_bool sing_token_live(const InfinCtx *c)
{
    const size_t tessera = *c->tessera;

    // Explicit cast converts the three-part match into the mmgr_bool return
    return (mmgr_bool)((c->s->sing.span != 0u) && (MMGR_TESSERA_IDX(tessera) == MMGR_TESSERA_SING) &&
                       (MMGR_TESSERA_GEN(tessera) == c->s->sing.gen));
}

/**
 * @brief Packs the writer's current state and segment into one status word.
 *
 * @param[in] c Ring and any flags the caller wants folded in through why [BORROWS].
 * @return      The flags in the low octet, the segment index in the high one.
 * @note Reports ATTACHED or READY, adds GRANTED while a grant is outstanding, and FULL when a granule will not fit.
 * @note The segment is taken from the grant when one is outstanding, and from the head otherwise.
 */
MMGR_INLINE mmgr_u16 sing_state(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t at = (s->sing.span != 0u) ? s->sing.at : MMGR_ATOMIC_LOAD(&s->head);
    mmgr_u16 st = c->why | (s->sing.open ? MMGR_SING_ATTACHED : MMGR_SING_READY);

    if (s->sing.span != 0u)
    {
        st |= MMGR_SING_GRANTED;
    }
    if (MMGR_CALL(infin_vacant, InfinCtx, .s = s) < s->sing.gran)
    {
        st |= MMGR_SING_FULL;
    }
    // Explicit casts keep the packing in mmgr_u16: the segment index shifts above the flag octet
    return (mmgr_u16)(st | (mmgr_u16)((at / s->seg) << MMGR_SING_FLAG_BITS));
}

/**
 * @brief The single entry the exclusive writer drives: attach, grant, commit and report.
 *
 * @param[in,out] c Ring, plus whichever of sing, src, tessera, bytes, off, units and status apply [BORROWS].
 * @return          Start of a granted or written run, or NULL [BORROWS].
 * @note A tessera that no longer matches is cleared and reported as STALE and REFUSED.
 * @note Without a live tessera, a NULL c->sing or a refused sing_admit reports ALIEN.
 * @note Work happens only when src or tessera is given; otherwise the call just reports state.
 * @note Writes the packed status through c->status whenever that is not NULL, on every path.
 */
MMGR_INLINE uint8_t *infin_singularitas(const InfinCtx *c)
{
    RingState *const s = c->s;
    const mmgr_bool live = ((c->tessera != NULL) && (*c->tessera != 0u));
    const mmgr_bool asking = ((c->src != NULL) || (c->tessera != NULL));
    mmgr_u16 why = 0u;
    uint8_t *at = NULL;

    if (live && !MMGR_CALL(sing_token_live, InfinCtx, .s = s, .tessera = c->tessera))
    {
        *c->tessera = 0u;
        why = MMGR_SING_STALE | MMGR_SING_REFUSED;
    }
    else if (!live && ((c->sing == NULL) || !MMGR_CALL(sing_admit, InfinCtx, .s = s, .sing = c->sing)))
    {
        // Explicit cast holds the combined flags in mmgr_u16, the width sing_state folds them into
        why = (mmgr_u16)(MMGR_SING_ALIEN | (asking ? MMGR_SING_REFUSED : 0u));
    }
    else if (asking)
    {
        at = MMGR_CALL(sing_body, InfinCtx, .s = s, .src = c->src, .bytes = c->bytes, .off = c->off, .tessera = c->tessera,
                       .units = c->units, .rearm = (c->sing != NULL));
        if ((at == NULL) && !(live && (c->sing == NULL)))
        {
            why = MMGR_SING_REFUSED;
        }
    }

    if (c->status != NULL)
    {
        *c->status = MMGR_CALL(sing_state, InfinCtx, .s = s, .why = why);
    }
    return at;
}

/**
 * @brief Releases the exclusive writer attachment.
 *
 * @param[in,out] c Ring, the attachment to release, and where to report state [BORROWS].
 * @return          MMGR_TRUE when the attachment was released.
 * @note Refuses with ALIEN and REFUSED when no writer is attached or the owner does not match.
 * @note Refuses with REFUSED while s->sing.span is non-zero.
 * @note Bumps the grant generation on success, so any surviving tessera stops resolving.
 */
MMGR_INLINE mmgr_bool infin_detach(const InfinCtx *c)
{
    RingState *const s = c->s;

    if ((!s->sing.open) || (c->sing == NULL) || (c->sing->owner != s->sing.owner))
    {
        if (c->status != NULL)
        {
            *c->status = MMGR_CALL(sing_state, InfinCtx, .s = s, .why = MMGR_SING_ALIEN | MMGR_SING_REFUSED);
        }
        return MMGR_FALSE;
    }
    if (s->sing.span != 0u)
    {
        if (c->status != NULL)
        {
            *c->status = MMGR_CALL(sing_state, InfinCtx, .s = s, .why = MMGR_SING_REFUSED);
        }
        return MMGR_FALSE;
    }

    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.open = MMGR_FALSE;
    s->sing.gen++;

    if (c->status != NULL)
    {
        *c->status = MMGR_CALL(sing_state, InfinCtx, .s = s, .why = 0u);
    }
    return MMGR_TRUE;
}

/**
 * @brief Drives the exclusive writer: attach, grant, commit and report, in one call.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
uint8_t *mmgr_infin_singularitas(const InfinCfg *c)
{
    return MMGR_CALL(infin_singularitas, InfinCtx, .s = ring_of(c->ring), .src = c->src, .bytes = c->bytes, .off = c->off,
                     .tessera = c->tessera, .units = c->units, .sing = c->sing, .status = c->status);
}

/**
 * @brief Releases the exclusive writer attachment held by c->sing->owner.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
mmgr_bool mmgr_infin_detach(const InfinCfg *c)
{
    return MMGR_CALL(infin_detach, InfinCtx, .s = ring_of(c->ring), .sing = c->sing, .status = c->status);
}

/**
 * @brief Moves c->cur to c->off within its frame.
 *
 * @note Documented at the declaration in confinium_exclusivum_infinitas.h.
 */
void mmgr_infin_seek(const InfinCfg *c)
{
    MMGR_CALL(infin_seek, InfinCtx, .s = ring_of(c->ring), .cur = c->cur, .off = c->off);
}
