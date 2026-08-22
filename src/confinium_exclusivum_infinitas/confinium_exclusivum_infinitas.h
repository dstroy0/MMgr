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
 * holding a cursor may move it - reset it, offset it - and every one of those is a call the ring
 * owns, because a cursor the caller could advance itself is a cursor the ring has stopped knowing
 * about.
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
#define MMGR_RING_WORDS 32u

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
    size_t *const tessera;         /**< In/out. The token the ring issued for this drain. */
    const void *const owner;       /**< Who the ordinary cursor goes back to. */
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
    size_t (*write)(const InfinCfg *c);
    void (*seek)(const InfinCfg *c);
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, init, open, drain, available, free_, read_byte, read, peek, consume, write, seek);

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
size_t mmgr_infin_write(const InfinCfg *c);
void mmgr_infin_seek(const InfinCfg *c);
/** @} */

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
    .write = mmgr_infin_write,
    .seek = mmgr_infin_seek,
};

MMGR_FINIS_DECLS

#endif
