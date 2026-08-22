// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H
#define MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file confinium_exclusivum_infinitas.h
 * @brief A lock free ring, and the cursors it hands out over it.
 *
 * The consumer constructs the ring and owns its storage. It does not own the internals: what is
 * inside the handle is this module's, and the header does not say. Every entry is a request - the
 * ring answers it or declines it, and nothing on this surface can compel it.
 *
 * Nothing reads or writes the buffer except through the ring. The ring knows where every cursor is
 * and what bounds it, so an overread is not a thing a caller can ask for and be given. A caller
 * holding a cursor may move it, and moving it is a call the ring owns - a cursor the caller could
 * advance itself is a cursor the ring has stopped knowing about.
 *
 * There is one move, seek, and it takes an offset from the frame's start. Returning to the start is
 * an offset of zero rather than an entry of its own, because a cursor's position is an offset and a
 * reset is that offset being none.
 *
 * Everything that goes in goes in through singularitas, which is the one ingestion path and holds
 * one cursor. A writer asks for so many units and is handed an address or refused; it never says
 * where. The unit is the writer's, set once when the path is opened - a byte at a time up to a
 * machine word at a time - and every count on that path is in units, never in bytes. That is what
 * keeps the head aligned to the unit forever instead of decaying at the first short transfer, and
 * it is why a granted address is one a DMA channel can be handed as it stands.
 *
 * A grant is a destination, not a credential. The address lets the holder write; only the tessera
 * lets it publish, and the cursor moves by an offset handed back through the ring. A bare pointer
 * entitles no one to anything.
 *
 * Ordinarily there is one cursor and one accessor. A second exists only while a keepout does: the
 * consumer reports a priority drain from here to here, the ring frames it as a mask overlay, spawns
 * a cursor bounded by that frame and hands it over, and the caller gives it to whatever drains it.
 * The mask is set once and stands for the life of the keepout. When that cursor reaches the last
 * segment the ring pulls it and the keepout together and hands the ordinary cursor back to the
 * caller it came from, which it knows by the const address it was given.
 *
 * Concurrent writes are legal only on disparate segments, which is what the segments are for.
 *
 * The bodies are in the .c so that <stdatomic.h> is that file's problem. A read modify write on a
 * cursor has to be indivisible rather than merely ordered, because deferred work landing between a
 * load and a store loses whatever that work wrote - one core is enough for that.
 *
 * The bitmap is the machine word, so the count of reservations follows the platform: sixteen on a
 * 16-bit target, sixty four on a 64-bit one. A word wide atomic is lock free on every target by
 * construction, which matters, because one that falls back to a lock cannot be touched from
 * deferred work at all.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Is a capacity a power of two. */
#define MMGR_RING_POW2(cap) (((cap) & ((cap) - 1)) == 0)

/** @brief Wrap an index. Capacity must be a power of two. */
#define MMGR_RING_WRAP(i, cap) ((i) & ((cap) - 1))

/** @brief Most reservations a ring can hold at once: one per bit of the machine word. */
#define MMGR_RING_LOCULI_MAX MMGR_WORD_BITS

/**
 * @brief How many words of storage a ring handle takes.
 *
 * The consumer declares one of these; the .c asserts its own layout fits. A number here rather than
 * the layout itself is the point - the size is the consumer's business because it has to find room
 * for it, and the arrangement is not.
 */
#define MMGR_RING_WORDS 40u

/**
 * @brief The largest unit an ingestion path may move at a time.
 *
 * A machine word. Wider than that is not one store, and a unit that is not one store is a unit a
 * reader can catch half of.
 */
#define MMGR_SING_GRANULE_MAX ((size_t)sizeof(mmgr_word))

/** @name The ingestion path's status.
 *  @brief What the path is, and why the last ask ended the way it did.
 *
 *  An address answers whether; these answer why, which an address cannot. A producer handed NULL
 *  has to decide between waiting, asking for less, and giving up, and those are three different
 *  answers to three different refusals. Ask for the status on any call, or on a call that asks for
 *  nothing else - a status is free to read and reading it changes nothing.
 *
 *  READY and ATTACHED are the same fact from either side and never appear together.
 *  @{ */
#define MMGR_SING_READY ((mmgr_u16)1 << 0)    /**< No auctor holds the path. It can be attached. */
#define MMGR_SING_ATTACHED ((mmgr_u16)1 << 1) /**< An auctor holds it. */
#define MMGR_SING_GRANTED ((mmgr_u16)1 << 2)  /**< A grant is out. The ground is promised. */
#define MMGR_SING_FULL ((mmgr_u16)1 << 3)     /**< Nothing is free. Wait for the consumer. */
#define MMGR_SING_ALIEN ((mmgr_u16)1 << 4)    /**< The caller is not the auctor, or opened nothing. */
#define MMGR_SING_STALE ((mmgr_u16)1 << 5)    /**< The token does not name a live grant. */
#define MMGR_SING_REFUSED ((mmgr_u16)1 << 6)  /**< The ask was declined. */
/** @} */

/**
 * @brief How much of a status the flags take. Everything above it is the address space.
 *
 * One octet, so the split is readable in hex and a flag is never mistaken for an address.
 */
#define MMGR_SING_FLAG_BITS 8u

/** @name Reading a status.
 *  @brief Constants below, address space above.
 *
 *  Sixteen bits on every target, big endian or little, because it is one scalar and not a layout -
 *  a status fits a 16-bit register on the narrowest machine this builds for and loses nothing on
 *  the widest. That is what fixes the split at an octet each: seven flags and a spare below, and a
 *  segment index above, which cannot exceed MMGR_RING_LOCULI_MAX and so cannot exceed 255.
 *
 *  The address space is segments rather than bytes because segments are what the ring arbitrates
 *  in. One bit of the mask is one segment, a grant is denied by that bit, and a status that named a
 *  byte would be naming something no reservation is expressed in.
 *
 *  The segment reported is the one the path will write into next: the grant's first while a grant
 *  is out, the head's otherwise.
 *  @{ */
#define MMGR_SING_FLAGS(st) ((mmgr_u16)((st) & (mmgr_u16)(((mmgr_u16)1 << MMGR_SING_FLAG_BITS) - 1u)))
#define MMGR_SING_SEG(st) ((size_t)((mmgr_u16)(st) >> MMGR_SING_FLAG_BITS))
/** @} */

/**
 * @brief A ring, as far as anyone outside this module may know it.
 *
 * Sized in size_t, not in MMGR_RAW_WORD. What is inside is buffers, counts and cursors, and those
 * follow the machine the code is built for - a narrow SWAR lane does not make a pointer smaller.
 * Sizing this by the lane made the storage shrink while the thing it has to hold did not, which the
 * assert in the .c caught.
 */
typedef struct
{
    MMGR_ALIGN(sizeof(size_t)) unsigned char opaque[MMGR_RING_WORDS * sizeof(size_t)];
} mmgr_ring;

/**
 * @brief A cursor over a ring.
 *
 * Incomplete on purpose. The ring spawns these and hands them out; a caller holds one and passes it
 * to whatever does the work, and can do nothing with it except give it back to the ring.
 */
struct MmgrCursor;

/**
 * @brief What a ring is made from.
 *
 * The buffer, how big it is, how it is divided, and the bitmap the reservations are recorded in -
 * all the consumer's, none of it this module's to allocate.
 */
typedef struct
{
    uint8_t *const buf;        /**< The ring. */
    const size_t cap;          /**< Its capacity, a power of two. */
    const size_t nsegs;        /**< Segments it divides into, a power of two. */
    _Atomic mmgr_word *const held; /**< Where reservations are recorded. */
} RingCfg;

/**
 * @brief What the one ingestion path is opened as.
 *
 * The unit is fixed here and never again. Every count on the path is in units: a claim of four on a
 * path of words is sixteen bytes, and the head moves by sixteen. Bytes never appear in the
 * arithmetic, so a head that starts aligned to the unit stays aligned to it for the life of the
 * ring - which is the whole reason a granted address may go straight into a DMA descriptor.
 *
 * The buffer is aligned before the ring is given it. The ring does not check that and has no way
 * to; a unit wider than the buffer's own alignment is the consumer's mistake to not make.
 */
typedef struct
{
    const void *const owner; /**< Who the path belongs to. There is one. */
    const size_t gran;       /**< The unit, in bytes: a power of two, 1 up to MMGR_SING_GRANULE_MAX. */
} SingularitasCfg;

/**
 * @brief What a ring operation is given.
 *
 * The cursor being asked, and the operation's own arguments. There are no cursors in here that the
 * caller made; the ring hands every one of them out.
 */
typedef struct
{
    mmgr_ring *const r;            /**< The ring being asked. */
    struct MmgrCursor *const cur;  /**< Which cursor, when the entry moves one. */
    uint8_t *const dst;            /**< Where bytes are taken to. */
    const uint8_t *const src;      /**< Where bytes are written from. */
    const size_t n;                /**< A byte count, or a most-to-take. */
    const size_t off;              /**< An offset, for peek and seek. */
    const size_t from;             /**< First byte of a drain. */
    const size_t to;               /**< One past its last. */
    size_t *const tessera;         /**< In/out. The token the ring issued for this drain or grant. */
    const void *const owner;       /**< Who the ordinary cursor goes back to. */
    const SingularitasCfg *const sing; /**< The ingestion path: who is asking, and in what unit. */
    size_t *const units;               /**< Out. How many units a grant covers. */
    mmgr_u16 *const status;            /**< Out. The ingestion path's flags and where it is. */
} InfinCfg;

/** @brief Ring dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_bool (*init)(mmgr_ring *r, const RingCfg *c);
    struct MmgrCursor *(*open)(const InfinCfg *c);
    const uint8_t *(*drain)(const InfinCfg *c);
    size_t (*available)(const InfinCfg *c);
    size_t (*free_)(const InfinCfg *c);
    mmgr_bool (*read_byte)(const InfinCfg *c);
    const uint8_t *(*read)(const InfinCfg *c);
    void (*peek)(const InfinCfg *c);
    void (*consume)(const InfinCfg *c);
    uint8_t *(*singularitas)(const InfinCfg *c);
    mmgr_bool (*detach)(const InfinCfg *c);
    void (*seek)(const InfinCfg *c);
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, init, open, drain, available, free_, read_byte, read, peek, consume, singularitas, detach,
               seek);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
mmgr_bool mmgr_infin_init(mmgr_ring *r, const RingCfg *c);
struct MmgrCursor *mmgr_infin_open(const InfinCfg *c);
const uint8_t *mmgr_infin_drain(const InfinCfg *c);
size_t mmgr_infin_available(const InfinCfg *c);
size_t mmgr_infin_free(const InfinCfg *c);
mmgr_bool mmgr_infin_read_byte(const InfinCfg *c);
const uint8_t *mmgr_infin_read(const InfinCfg *c);
void mmgr_infin_peek(const InfinCfg *c);
void mmgr_infin_consume(const InfinCfg *c);
uint8_t *mmgr_infin_singularitas(const InfinCfg *c);
mmgr_bool mmgr_infin_detach(const InfinCfg *c);
void mmgr_infin_seek(const InfinCfg *c);
/** @} */

/**
 * @brief Give the ingestion path up, so another stream may have it.
 *
 * Switching streams is a signal from the auctor and from nobody else. The ring will not take the
 * path off whoever holds it, and no second caller can say anything that moves it - a path that can
 * be taken is a path that can be taken in the middle of a transfer, and the address the holder is
 * still filling would go to someone else on the strength of an unlucky call order.
 *
 * A grant still out refuses the detach. Commit it, or commit nothing, and then let go.
 *
 * Afterwards the path answers MMGR_SING_READY to the next caller that asks, which is how the next
 * stream learns it may attach.
 */

/**
 * @brief The one ingestion path. Everything that enters the ring enters here.
 *
 * One question, asked three ways, and the same answer every time: an address, or NULL because the
 * ring declined. Nothing here is written short. A writer handed back less than it asked for has to
 * work out which of its units went, and a producer that has to reason about that will get it wrong
 * once.
 *
 * @c sing opens the path on first use and names its owner thereafter; a second owner is refused,
 * because there is one ingestion cursor. Every count below is in that path's units.
 *
 * - **put** - @c src set. "May I write @c n units." The ring copies them, publishes them, and
 *   hands back where the first one went. This is the whole of a synchronous producer.
 * - **claim** - @c src NULL, @c tessera zero. "May I have @c n units to fill." The ring reserves
 *   the run against the mask, issues a tessera and hands back the address. Nothing is published:
 *   the head has not moved and a reader cannot see the ground.
 *
 *   The run never wraps, because a channel takes one base and one length. That is also why @c n of
 *   zero exists - it asks for as much as one run allows, which is the only ask that cannot be
 *   refused forever at the buffer's end, and it requires @c units because a caller told a length it
 *   did not name has to be told it. Ask for a count and @c units is a confirmation; ask for zero
 *   and it is the answer.
 * - **commit** - @c src NULL, @c tessera set. @c off units landed; the ring advances the head by
 *   exactly that and drops the reservation. With @c sing set it re-arms and hands back the next
 *   grant, which is what a completion callback wants; with @c sing NULL it stops and the tessera is
 *   spent. @c off of zero publishes nothing and gives the ground back.
 *
 * A grant is a destination and a tessera is the permission. The address alone will not publish a
 * byte, and a tessera from a finished grant stops matching the moment the record is reused - which
 * is what a DMA completion arriving after its channel was torn down runs into.
 */

/**
 * @brief Ring namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 *
 * free_ carries the underscore for the reason xor_ does: spelled bare, a member access reading
 * `iteratio_infinita.free(...)` is a call to whatever a freestanding libc defined free as, and a
 * function-like macro expands wherever its name is followed by an open parenthesis - the member
 * access in front does not stop it.
 */
MMGR_NS InfinitasNs iteratio_infinita MMGR_UNUSED = {
    .init = mmgr_infin_init,
    .open = mmgr_infin_open,
    .drain = mmgr_infin_drain,
    .available = mmgr_infin_available,
    .free_ = mmgr_infin_free,
    .read_byte = mmgr_infin_read_byte,
    .read = mmgr_infin_read,
    .peek = mmgr_infin_peek,
    .consume = mmgr_infin_consume,
    .singularitas = mmgr_infin_singularitas,
    .detach = mmgr_infin_detach,
    .seek = mmgr_infin_seek,
};

MMGR_FINIS_DECLS

#endif
