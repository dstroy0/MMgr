/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file confinium_exclusivum_infinitas.h
 * @brief Single-producer, single-consumer byte ring, a segment view over it, and loculus keepouts.
 *
 * @note Nothing of the implementation is spelled here: mmgr_ring is a run of opaque bytes, and the
 *       state laid into it is declared nowhere a consumer can reach.
 * @note The caller declares the ring and supplies the bytes; every index, mask and span is the ring's.
 * @note Exactly one producer advances head and exactly one consumer advances tail, so ordering is
 *       all that is needed and no entry takes a lock or a read-modify-write on those two.
 * @note No byte of the ring is ever scrubbed, and a drop leaves the loculus record alone, so a
 *       restream can run again. Only mmgr_infin_init clears those records.
 */
#ifndef MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H
#define MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief A byte region a held loculus keeps out: the storage, its extent, and how far a reader has gone.
 *
 * @note Carried here rather than taken from spatium, so this module reaches nothing outside config.
 */
typedef struct
{
    uint8_t *buf; /**< First byte of the region [BORROWS]. */
    size_t cap;   /**< Bytes at buf. */
    size_t pos;   /**< How far a reader has walked it. */
} mmgr_ring_span;

/**
 * @brief Size of the ring storage a caller declares, counted in size_t units.
 *
 * @note The implementation asserts its state fits inside this, so a change there fails a build
 *       rather than overrunning a caller's object.
 * @note Most of it is the keepout array, so raising MMGR_RING_LOCULI may require raising this too;
 *       the assertion names it when that happens. A build sets this the same way it sets the
 *       loculus count, before including this header.
 */
#ifndef MMGR_RING_WORDS

#define MMGR_RING_WORDS 40u
#endif

/**
 * @brief Loculi a mask can address, which is one bit per loculus in a machine word.
 *
 * @warning A pool wider than this needs a wider mask or a scan; the assertion below names which.
 */
#define MMGR_RING_LOCULI_MAX MMGR_WORD_BITS

/**
 * @brief Loculi this build reserves, which a build may set before including this header.
 *
 * @note The keepout spans are most of the ring's state, so a build with no use for the loculus view
 *       sets this to 0 and can then lower MMGR_RING_WORDS; on its own this setting shrinks the state
 *       and not the storage a caller declares. Every entry that reads the loculi then reports empty
 *       or refuses, and mmgr_infin_loculus_drop and mmgr_infin_loculus_mark do nothing.
 */
#ifndef MMGR_RING_LOCULI

#define MMGR_RING_LOCULI 8u
#endif

/**
 * @brief Asserts the loculi a build declares fit in one machine word.
 *
 * @note The free and held masks are each one mmgr_word, one bit per loculus, so a count past
 *       MMGR_RING_LOCULI_MAX has no bit to sit in. ring_loculus_bit in confinium_exclusivum_infinitas.c
 *       builds that bit by shifting, and this bound is what keeps the shift inside the word.
 */
MMGR_STATIC_ASSERT(
    MMGR_RING_LOCULI <= MMGR_RING_LOCULI_MAX,
    "the loculus masks are one machine word; widen them or fall back to a scan past MMGR_RING_LOCULI_MAX");

/**
 * @brief Reports whether cap is a power of two.
 *
 * @param[in] cap Value to test.
 * @return        Non-zero when cap has at most one bit set.
 * @warning Also reports true for 0; mmgr_infin_init rejects 0 separately.
 * @warning cap appears twice in the expansion, so an argument with a side effect is evaluated twice.
 */
#define MMGR_RING_POW2(cap) (((cap) & ((cap) - 1u)) == 0u)

/**
 * @brief Wraps an index into a ring of cap bytes.
 *
 * @param[in] i   Index to wrap.
 * @param[in] cap Ring size.
 * @return        The index masked into range.
 * @warning Correct only because cap is a power of two, which mmgr_infin_init enforces.
 */
#define MMGR_RING_WRAP(i, cap) ((i) & ((cap) - 1u))

/**
 * @brief Storage a caller declares for one ring, whose contents belong to the implementation.
 *
 * @note MMGR_ALIGN aligns opaque to size_t.
 * @note From mmgr_infin_init onward these bytes hold the buffer that call was given, which must
 *       outlive the ring [BORROWS].
 * @warning The bytes carry no documented layout; the infin calls are the only accessors.
 */
typedef struct
{
    MMGR_ALIGN(sizeof(size_t)) uint8_t opaque[MMGR_RING_WORDS * sizeof(size_t)]; /**< Private ring state. */
} mmgr_ring;

/**
 * @brief Arguments for every infin call; each reads only what it needs.
 *
 * @note Every call but mmgr_infin_loculus_next reads ring; init adds buf, cap and nsegs.
 * @note Members left unset are zero, and the calls that ignore them never read them.
 */
typedef struct
{
    mmgr_ring *const ring;    /**< Ring to act on [BORROWS]. */
    uint8_t *const buf;       /**< Ring bytes, for init [BORROWS]. */
    const size_t cap;         /**< Bytes in buf; a non-zero power of two. */
    const size_t nsegs;       /**< Segments to divide the ring into; a power of two, at most cap. */
    uint8_t *const dst;       /**< Destination for read, read_byte and peek [BORROWS]. */
    const uint8_t *const src; /**< Bytes put writes, or the region hold records [BORROWS]. */
    const size_t bytes;       /**< Byte count the call moves or records. */
    const size_t off;         /**< Offset ahead of the tail that peek starts at. */
    const size_t idx;         /**< Loculus or segment the call acts on. */
    const mmgr_word mask;     /**< Mask loculus_next picks the lowest set bit of. */
    size_t *const out;        /**< Set to the segment index a next or front call chose [BORROWS]. */
} InfinCfg;

/**
 * @brief Type of the iteratio_infinita dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the twenty members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note init lays all three down; the seven after it are the byte ring, then six for the segment view
 *       and six for the loculi.
 */
typedef struct
{
    mmgr_bool (*init)(const InfinCfg *args);          /**< Lays a fresh ring into the caller's storage. */
    size_t (*available)(const InfinCfg *args);        /**< Bytes waiting to be read. */
    size_t (*vacant)(const InfinCfg *args);           /**< Bytes still free to write. */
    mmgr_bool (*read_byte)(const InfinCfg *args);     /**< Takes one byte and advances the tail. */
    size_t (*read)(const InfinCfg *args);             /**< Takes up to bytes and advances the tail once. */
    void (*peek)(const InfinCfg *args);               /**< Copies bytes out without advancing the tail. */
    void (*consume)(const InfinCfg *args);            /**< Advances the tail past bytes. */
    mmgr_bool (*put)(const InfinCfg *args);           /**< Writes a whole span, or refuses it entire. */
    size_t (*seg_inflight)(const InfinCfg *args);     /**< Segments filled and not yet released. */
    mmgr_bool (*seg_next)(const InfinCfg *args);      /**< Index of the segment the producer fills next. */
    void (*seg_publish)(const InfinCfg *args);        /**< Makes the filled segment visible to the consumer. */
    mmgr_bool (*seg_front)(const InfinCfg *args);     /**< Index of the segment the consumer takes next. */
    void (*seg_release)(const InfinCfg *args);        /**< Frees the front segment. */
    uint8_t *(*seg_at)(const InfinCfg *args);         /**< The contiguous span of one segment. */
    mmgr_word (*loculus_ready)(const InfinCfg *args); /**< Loculi that are free and not held. */
    mmgr_iword (*loculus_next)(const InfinCfg *args); /**< Lowest set bit of a mask, or -1. */
    mmgr_bool (*loculus_hold)(const InfinCfg *args);  /**< Takes a loculus and records its keepout. */
    const mmgr_ring_span *(*loculus_keepout)(const InfinCfg *args); /**< The region a held loculus keeps out. */
    void (*loculus_drop)(const InfinCfg *args); /**< Gives a loculus back, leaving its bytes alone. */
    void (*loculus_mark)(const InfinCfg *args); /**< Marks a loculus free. */
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, init, available, vacant, read_byte, read, peek, consume, put, seg_inflight, seg_next,
               seg_publish, seg_front, seg_release, seg_at, loculus_ready, loculus_next, loculus_hold, loculus_keepout,
               loculus_drop, loculus_mark);

/**
 * @brief Lays a fresh ring into args->ring, over the bytes at args->buf.
 *
 * @param[in,out] args Storage, bytes, capacity and segment count [BORROWS].
 * @return          MMGR_TRUE when the ring is ready, MMGR_FALSE when a size was rejected.
 * @note Refuses a cap or nsegs that is 0 or not a power of two, and an nsegs above cap.
 * @note Marks every loculus free and none held, and clears the recorded keepouts.
 * @warning args->ring and args->buf are both the caller's, and args->buf is kept by the ring [BORROWS].
 * @warning Neither may be null, and nothing holds them there outside a MMGR_DEBUG_CHECKS build. A null
 *          ring is written through as soon as the sizes pass, and a null buf is taken and not noticed
 *          until the first move.
 * @warning args->buf must carry the alignment the caller's own accesses need; this call does not align it.
 */
mmgr_bool mmgr_infin_init(const InfinCfg *args);

/**
 * @brief Returns the bytes waiting to be read.
 *
 * @param[in] args Ring to inspect [BORROWS].
 * @return      Distance from the tail to the head, wrapped into the ring.
 * @warning Only a snapshot: a concurrent producer may add more before the caller acts on it.
 */
size_t mmgr_infin_available(const InfinCfg *args);

/**
 * @brief Returns the bytes still free to write.
 *
 * @param[in] args Ring to inspect [BORROWS].
 * @return      cap minus one, minus the readable bytes.
 * @note One byte is held back always, which is what keeps a full ring apart from an empty one.
 * @warning Only a snapshot: a concurrent consumer may free more before the caller acts on it.
 */
size_t mmgr_infin_vacant(const InfinCfg *args);

/**
 * @brief Takes one byte into args->dst and advances the tail past it.
 *
 * @param[in,out] args Ring and the destination byte [BORROWS].
 * @return          MMGR_TRUE when a byte was taken, MMGR_FALSE when the ring was empty.
 * @note Writes through args->dst only when it returns MMGR_TRUE.
 * @note One byte per call, so what this costs is the call rather than the move; a caller drawing a
 *       run of bytes wants mmgr_infin_read, which moves them a word at a time under one tail store.
 * @warning args->dst must be writable for one byte.
 */
mmgr_bool mmgr_infin_read_byte(const InfinCfg *args);

/**
 * @brief Takes up to args->bytes into args->dst and advances the tail once at the end.
 *
 * @param[in,out] args Ring, destination and the most to take [BORROWS].
 * @return          Bytes actually taken, which is 0 when the ring was empty.
 * @note Moves the bytes in at most two runs, so the wrap costs one extra move rather than one per byte.
 * @note Reads the tail once and publishes it once, not per byte.
 * @warning args->dst must be writable for args->bytes.
 */
size_t mmgr_infin_read(const InfinCfg *args);

/**
 * @brief Copies args->bytes starting args->off ahead of the tail into args->dst, leaving the tail alone.
 *
 * @param[in,out] args Ring, destination, byte count and starting offset [BORROWS].
 * @note Moves the bytes in at most two runs, the same way mmgr_infin_read does.
 * @warning Copies args->bytes whether or not that many have arrived; read mmgr_infin_available first.
 * @warning An args->bytes above the ring's capacity is held there, since one lap is all two runs express.
 * @warning args->dst must be writable for args->bytes.
 */
void mmgr_infin_peek(const InfinCfg *args);

/**
 * @brief Advances the tail past args->bytes.
 *
 * @param[in,out] args Ring and the byte count to drop [BORROWS].
 * @warning Advances whether or not that many have arrived; read mmgr_infin_available first.
 */
void mmgr_infin_consume(const InfinCfg *args);

/**
 * @brief Writes args->bytes of args->src into the ring, or refuses the whole span.
 *
 * @param[in,out] args Ring, the bytes to write and their count [BORROWS].
 * @return          MMGR_TRUE when the span was written, MMGR_FALSE when it would not fit.
 * @note Checks the whole span against mmgr_infin_vacant first, so a partial write never happens.
 * @note Advances a local head across the wrap and publishes it once, so no half span is ever visible.
 * @warning args->src must be readable for args->bytes.
 */
mmgr_bool mmgr_infin_put(const InfinCfg *args);

/**
 * @brief Returns the segments filled and not yet released.
 *
 * @param[in] args Ring to inspect [BORROWS].
 * @return      The distance between the claim and release counters.
 * @warning The two counters are read one after the other, so the answer is only a snapshot: a publish
 *          or a release may land between the reads, or after both.
 */
size_t mmgr_infin_seg_inflight(const InfinCfg *args);

/**
 * @brief Reports the index of the segment the producer fills next.
 *
 * @param[in,out] args Ring, and where to write the index [BORROWS].
 * @return          MMGR_TRUE with the index in args->out, MMGR_FALSE when every segment is in flight.
 * @note Writes through args->out only when it returns MMGR_TRUE.
 * @note Publishing is separate, so a half-filled segment is never visible to the consumer.
 * @note A MMGR_FALSE can go stale the moment the consumer releases a segment; a MMGR_TRUE cannot,
 *       since releases only make room.
 * @warning args->out must be writable.
 */
mmgr_bool mmgr_infin_seg_next(const InfinCfg *args);

/**
 * @brief Makes the filled segment visible to the consumer.
 *
 * @param[in,out] args Ring to advance [BORROWS].
 * @note Only the producer calls this; the counter is read and written as two steps, which needs no
 *       atomicity between them only because one side owns it.
 * @warning Advances whether or not a segment was filled, so a publish with no matching
 *          mmgr_infin_seg_next puts more in flight than the ring holds, and that call then refuses
 *          until releases catch up.
 */
void mmgr_infin_seg_publish(const InfinCfg *args);

/**
 * @brief Reports the index of the segment the consumer takes next.
 *
 * @param[in,out] args Ring, and where to write the index [BORROWS].
 * @return          MMGR_TRUE with the index in args->out, MMGR_FALSE when none is in flight.
 * @note Writes through args->out only when it returns MMGR_TRUE.
 * @note A MMGR_FALSE can go stale the moment the producer publishes a segment; a MMGR_TRUE cannot,
 *       since publishes only add.
 * @warning args->out must be writable.
 */
mmgr_bool mmgr_infin_seg_front(const InfinCfg *args);

/**
 * @brief Frees the front segment.
 *
 * @param[in,out] args Ring to advance [BORROWS].
 * @note Segments release in the order they were claimed.
 * @note Only the consumer calls this; the counter is read and written as two steps, which needs no
 *       atomicity between them only because one side owns it.
 * @warning Advances whether or not a segment was in flight. A release past what has been published
 *          wraps the in-flight count and leaves mmgr_infin_seg_front handing out a segment that was
 *          never filled.
 */
void mmgr_infin_seg_release(const InfinCfg *args);

/**
 * @brief Returns the contiguous span of segment args->idx.
 *
 * @param[in] args Ring and the segment index [BORROWS].
 * @return      Its first byte inside the ring buffer [BORROWS].
 * @note The span runs for the cap divided by the nsegs that mmgr_infin_init was given.
 * @warning args->idx is not checked against the segment count, and no assertion covers it. An index
 *          at or past that count returns a pointer outside the ring's bytes.
 */
uint8_t *mmgr_infin_seg_at(const InfinCfg *args);

/**
 * @brief Returns the loculi that are free and not held.
 *
 * @param[in] args Ring to inspect [BORROWS].
 * @return      The free mask with the held ones cleared, bounded to MMGR_RING_LOCULI.
 * @note A loculus is takeable only when both hold, which is what makes reuse safe.
 * @warning Only a snapshot: the two masks are read one after the other, and a hold or a drop may land
 *          before the caller acts on it. mmgr_infin_loculus_hold settles that race, refusing a
 *          loculus another caller took first.
 */
mmgr_word mmgr_infin_loculus_ready(const InfinCfg *args);

/**
 * @brief Returns the index of the lowest set bit of args->mask.
 *
 * @param[in] args The mask to pick from [BORROWS].
 * @return      The index, or -1 when args->mask is empty.
 * @note Counts rather than scans, and branches on nothing but the empty mask.
 * @note Only args->mask is read; this is the one entry that never reaches the ring.
 */
mmgr_iword mmgr_infin_loculus_next(const InfinCfg *args);

/**
 * @brief Takes loculus args->idx and records the args->bytes at args->src that it keeps out.
 *
 * @param[in,out] args Ring, the loculus, and the region to record [BORROWS].
 * @return          MMGR_TRUE when this caller took it, MMGR_FALSE when it was already held.
 * @note The recorded region stays valid until mmgr_infin_loculus_drop, so a reader walks it in place.
 * @warning An out-of-range args->idx names nothing, so it reads as held and is never handed out.
 * @warning args->src is kept by the ring and handed back by mmgr_infin_loculus_keepout, so it must
 *          outlive the hold [BORROWS].
 */
mmgr_bool mmgr_infin_loculus_hold(const InfinCfg *args);

/**
 * @brief Returns the region loculus args->idx is keeping out.
 *
 * @param[in] args Ring and the loculus [BORROWS].
 * @return      The recorded span, or NULL when args->idx is out of range [BORROWS].
 * @note Handed back const, so a reader walks it without moving the ring's own record.
 * @warning The const covers the span, not the bytes it names: buf is reachable as a writable pointer,
 *          though the region reached mmgr_infin_loculus_hold as const.
 * @warning The record comes back for any loculus in range, held or not. A drop leaves it standing, so
 *          afterwards it still names the last region that loculus recorded [BORROWS].
 */
const mmgr_ring_span *mmgr_infin_loculus_keepout(const InfinCfg *args);

/**
 * @brief Gives loculus args->idx back.
 *
 * @param[in,out] args Ring and the loculus [BORROWS].
 * @note Leaves the recorded span and the bytes alone, so a restream can run again.
 * @note An out-of-range args->idx names no bit and clears nothing, and dropping a loculus that is
 *       not held does nothing.
 */
void mmgr_infin_loculus_drop(const InfinCfg *args);

/**
 * @brief Marks loculus args->idx free.
 *
 * @param[in,out] args Ring and the loculus [BORROWS].
 * @note Sets the free bit. The held one is mmgr_infin_loculus_drop's to clear, and a loculus is
 *       takeable only when both agree.
 * @note mmgr_infin_init sets the free bit for every loculus and no entry clears it, so on a ring that
 *       call has laid down this changes nothing.
 * @note An out-of-range args->idx names no bit and sets nothing.
 */
void mmgr_infin_loculus_mark(const InfinCfg *args);

/**
 * @brief Dispatch table instance named iteratio_infinita; each member calls the matching
 *        mmgr_infin_ function.
 */
MMGR_NS InfinitasNs iteratio_infinita MMGR_UNUSED = {
    .init = mmgr_infin_init,
    .available = mmgr_infin_available,
    .vacant = mmgr_infin_vacant,
    .read_byte = mmgr_infin_read_byte,
    .read = mmgr_infin_read,
    .peek = mmgr_infin_peek,
    .consume = mmgr_infin_consume,
    .put = mmgr_infin_put,
    .seg_inflight = mmgr_infin_seg_inflight,
    .seg_next = mmgr_infin_seg_next,
    .seg_publish = mmgr_infin_seg_publish,
    .seg_front = mmgr_infin_seg_front,
    .seg_release = mmgr_infin_seg_release,
    .seg_at = mmgr_infin_seg_at,
    .loculus_ready = mmgr_infin_loculus_ready,
    .loculus_next = mmgr_infin_loculus_next,
    .loculus_hold = mmgr_infin_loculus_hold,
    .loculus_keepout = mmgr_infin_loculus_keepout,
    .loculus_drop = mmgr_infin_loculus_drop,
    .loculus_mark = mmgr_infin_loculus_mark,
};

MMGR_FINIS_DECLS

#endif
