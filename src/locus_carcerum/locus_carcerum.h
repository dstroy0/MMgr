/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum.h
 * @brief A prison site: a caller's storage divided into cellblocks, with the warden that holds them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note LocusCarcerum() emits each cellblock's state and the warden that holds them and that every
 *       call goes through, all as initialized data. Nothing runs at startup. A configuration the
 *       asserts reject fails the build.
 * @note The storage is the caller's. A cellblock stands over a pool, ParsMemoriaeInternae or
 *       ParsMemoriaeExternum in mmgr.h, and its SeriesCellularum record holds that pool's address.
 *       The linker resolves it. No call computes a cellblock's address from a site and an index.
 *       The declaration of the pool settles which memory a cellblock sits in.
 * @note A cellblock has two tiers. The persistent tier runs up from base and the temporary tier runs
 *       down from size. Both boundaries are the two halves of one atomic word. A take moves its own
 *       half by one fetch-add whose old value carries the other, through the cellblock's turnstiles.
 * @note A cellblock's declaration picks its security level. MMGR_MINIMUM_SECURITY leaves a cell's
 *       bytes as they are on release. MMGR_MAXIMUM_SECURITY zeroes them. The warden is const:
 *       nothing after the declaration can change the level a cellblock runs at.
 * @note The public entries are English and the internals are Latin, this filename included.
 * @note spatium provides the spans over those bytes.
 */
#ifndef MMGR_LOCUS_CARCERUM_H
#define MMGR_LOCUS_CARCERUM_H

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Set to 1 to track each tier's high-water mark.
 *
 * @note Off by default because the two marks cost a size_t each in every SeriesCellularum, and a
 *       build that never reads them should not carry them.
 * @note Adds SeriesCellularum::persistent_hw and SeriesCellularum::temporary_hw, and the update that
 *       raises them in carcer_grow, which both tiers reach. One mark per tier: neither counts the
 *       other's bytes.
 * @note The #ifndef leaves a build's own definition in place, whether it arrives on the command line
 *       or from a header included ahead of this one.
 * @note Takes 0 or 1 alone. Both readers are #if, which would take any non-zero as set. The check
 *       below keeps a mistyped value from a quiet growth of every SeriesCellularum.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif
#if (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 0) && (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 1)
#error "MMGR_ENABLE_HW_MEM_CAPACITY_CB must be 0 or 1"
#endif

/**
 * @brief The most owners a program's owner table holds.
 *
 * @note The table is the program's one list of owners, laid by MMGR_OWNERS in the order of their
 *       declaration. An owner's place in it is its key. A declaration past this count fails the build.
 */
#ifndef MMGR_CARCER_OWNERS
#define MMGR_CARCER_OWNERS 128u
#endif

/**
 * @brief The fragmentation at which a cellblock's mover sorts its movable cells, as a percentage of the
 *        bytes its persistent tier occupies.
 *
 * @note mmgr_series_cellularum_fragmentation reads what the build compares against this.
 */
#ifndef MMGR_MOVABLE_SETTLE_PERCENT
#define MMGR_MOVABLE_SETTLE_PERCENT 25u
#endif
#if (MMGR_MOVABLE_SETTLE_PERCENT < 1u) || (MMGR_MOVABLE_SETTLE_PERCENT > 100u)
#error "MMGR_MOVABLE_SETTLE_PERCENT must be from 1 to 100"
#endif

/** @brief The milliseconds between the mover's readings of its cellblock. */
#ifndef MMGR_MOVABLE_READ_MS
#define MMGR_MOVABLE_READ_MS 100u
#endif

/** @brief The readings in a row that must find the cellblock quiet before the mover sorts. */
#ifndef MMGR_MOVABLE_QUIET_READINGS
#define MMGR_MOVABLE_QUIET_READINGS 3u
#endif

/** @brief The longest the mover waits between readings after a sort it backed off from. */
#ifndef MMGR_MOVABLE_BACKOFF_MOST_MS
#define MMGR_MOVABLE_BACKOFF_MOST_MS 25600u
#endif

#include "embed_atomic.h"
#include "embed_turnstile.h"

/**
 * @brief Alignment of every prisoner on the way out: one machine word, or the atomic size word's alignment where
 *        that is wider.
 *
 * @note Derived from the two types in place of a number. Every cell carries an atomic size word at its header
 *       and at its foot. A cell's offset and payload are whole multiples of this, and each of those words
 *       lands at its own alignment at every width.
 * @note LocusCarcerum() puts this alignment on the storage it declares. This rounds only
 *       the offsets inside a cellblock.
 */
#define MMGR_CARCER_ALIGN                                                                                              \
    ((sizeof(embed_word) > _Alignof(embed_atomic_usize)) ? (size_t)sizeof(embed_word)                                  \
                                                         : (size_t)_Alignof(embed_atomic_usize))

/**
 * @brief Asserts MMGR_CARCER_ALIGN is a power of two.
 *
 * @note A round of an offset masks off its low bits, which lands on a multiple only for a power
 *       of two. Both carcer_round and CARCER_HDR in locus_carcerum.c round that way.
 */
EMBED_STATIC_ASSERT((MMGR_CARCER_ALIGN & (MMGR_CARCER_ALIGN - 1u)) == 0u,
                    "the cellblock rounds offsets by masking, which needs a power of two alignment");

/**
 * @brief Asserts the storage a pool declaration aligns stands on the cell grain.
 *
 * @note A cell's words lie at whole grains from the storage's first byte, and they stand at their alignment only
 *       where that byte does.
 */
EMBED_STATIC_ASSERT((MMGR_ALIGN_BYTES % MMGR_CARCER_ALIGN) == 0u,
                    "MMGR_ALIGN_BYTES must be a multiple of MMGR_CARCER_ALIGN, the grain a cell's words stand on");

/**
 * @brief One owner of cells: its name, its key, the payload bytes of its cells now on both tiers, and
 *        the most it had at once.
 *
 * @note The library's storage, laid at compile time. The owner table is the list of these records
 *       that MMGR_OWNERS declares in the program's one translation unit for it. The same declaration
 *       defines each owner's identity: a const pointer into the table, which the linker resolves. The
 *       owner's module names it on every take, give and growth, as it names a movable cell's entry.
 * @note Nothing registers at run time. The pointer's const says only that the holder does not write;
 *       the record is MMgr's to write. Every entry refuses NULL in place of an owner, and every take
 *       refuses a pointer that is not one of the table's.
 * @note The key is the owner's place in the table, counted from 1.
 * @note A take adds the cell's payload bytes to in_use and raises hw to it; a give takes the same
 *       bytes off. Every count is an atomic word, added to and raised in one step each, and an owner
 *       shared between contexts counts exactly.
 * @note The bytes counted are the cell's own: the request rounded to a whole word. The take counts them
 *       on, the give reads them off the cell's word, and a matched pair returns in_use to where it stood.
 * @note Every cell carries its owner in the word behind its payload, and comes off that owner as a give
 *       drops it. A give or a growth that names another owner fails, and mmgr_carcer_hw_misnamed hears
 *       of it.
 */
typedef struct
{
    const char *name;          /**< The owner's name, as a reader prints it, unique in the table [BORROWS]. */
    size_t place;              /**< The owner's place in the table, from 1. */
    embed_atomic_usize in_use; /**< Payload bytes of the cells the owner has now. */
    embed_atomic_usize hw;     /**< The highest value of in_use so far. */
} CellOwner;

/**
 * @brief The owner word a cell carries behind its payload, one machine word laid as fields, high to low. The
 *        fields are the owner's place in the table, the completion bit, the signal bits, and the nanosecond of
 *        the take. The clock is the library's, and the nanosecond rolls over its bits.
 *
 * @note The owner test is one AND: the word's place field against an owner's place. A word whose place field
 *       is 0 names no owner, as a free cell's does. The one context whose swap lands sets the completion bit
 *       once, when the cell's bytes come off its owner. Two gives that reach the same cell count it off once
 *       between them. The signal bits are the cellblock's to raise and read with one atomic step each. The
 *       timestamp is what the cell's take read on mmgr_carcer_hw_clock_ns, in the bits left, and a reader
 *       of lifetimes knows it rolls over.
 * @note MMGR_CARCER_OWNERS sizes the place field: the fewest bits every owner's place fits, from 4 to 16.
 *       Then come 1 completion bit, 3 signal bits, and the timestamp in the bits left, the word's width
 *       less those. 1024 owners on a 64-bit word leave 49 timestamp bits. On a 32-bit word they leave 17,
 *       which roll over every 131 microseconds, and on a 16-bit word 1. The layout fails the build where
 *       the word has no timestamp bit.
 */
#if MMGR_CARCER_OWNERS < 0x10u
#define MMGR_OWNER_PLACE_BITS 4u
#elif MMGR_CARCER_OWNERS < 0x20u
#define MMGR_OWNER_PLACE_BITS 5u
#elif MMGR_CARCER_OWNERS < 0x40u
#define MMGR_OWNER_PLACE_BITS 6u
#elif MMGR_CARCER_OWNERS < 0x80u
#define MMGR_OWNER_PLACE_BITS 7u
#elif MMGR_CARCER_OWNERS < 0x100u
#define MMGR_OWNER_PLACE_BITS 8u
#elif MMGR_CARCER_OWNERS < 0x200u
#define MMGR_OWNER_PLACE_BITS 9u
#elif MMGR_CARCER_OWNERS < 0x400u
#define MMGR_OWNER_PLACE_BITS 10u
#elif MMGR_CARCER_OWNERS < 0x800u
#define MMGR_OWNER_PLACE_BITS 11u
#elif MMGR_CARCER_OWNERS < 0x1000u
#define MMGR_OWNER_PLACE_BITS 12u
#elif MMGR_CARCER_OWNERS < 0x2000u
#define MMGR_OWNER_PLACE_BITS 13u
#elif MMGR_CARCER_OWNERS < 0x4000u
#define MMGR_OWNER_PLACE_BITS 14u
#elif MMGR_CARCER_OWNERS < 0x8000u
#define MMGR_OWNER_PLACE_BITS 15u
#elif MMGR_CARCER_OWNERS < 0x10000u
#define MMGR_OWNER_PLACE_BITS 16u
#else
#error "MMGR_CARCER_OWNERS must be below 65536: the owner word's place field is 16 bits at the most"
#endif
#define MMGR_OWNER_SIGNAL_BITS 3u
EMBED_STATIC_ASSERT(EMBED_WORD_BITS > (MMGR_OWNER_PLACE_BITS + 1u + MMGR_OWNER_SIGNAL_BITS),
                    "the owner word must have a timestamp bit left after its place, completion and signaling bits");
#define MMGR_OWNER_TIME_BITS (EMBED_WORD_BITS - MMGR_OWNER_PLACE_BITS - 1u - MMGR_OWNER_SIGNAL_BITS)
#define MMGR_OWNER_TIME_MASK (((embed_atomic_usize_value)1u << MMGR_OWNER_TIME_BITS) - 1u)
#define MMGR_OWNER_SIGNAL_SHIFT MMGR_OWNER_TIME_BITS
#define MMGR_OWNER_COMPLETE ((embed_atomic_usize_value)1u << (MMGR_OWNER_TIME_BITS + MMGR_OWNER_SIGNAL_BITS))
#define MMGR_OWNER_PLACE_SHIFT (MMGR_OWNER_TIME_BITS + MMGR_OWNER_SIGNAL_BITS + 1u)
#define MMGR_OWNER_PLACE_MASK ((((embed_atomic_usize_value)1u << MMGR_OWNER_PLACE_BITS) - 1u) << MMGR_OWNER_PLACE_SHIFT)
EMBED_STATIC_ASSERT((size_t)MMGR_CARCER_OWNERS < ((size_t)1u << MMGR_OWNER_PLACE_BITS),
                    "MMGR_CARCER_OWNERS must fit the owner word's place field");

/**
 * @brief The library's clock in nanoseconds, which a cell's owner word carries the take on.
 *
 * @return The nanoseconds since a start the port chooses, which wrap at 2^64.
 * @note Weak, and the default answers 0 every time: a build that defines none stamps every take 0. A port
 *       defines this name to replace it.
 */
uint64_t mmgr_carcer_hw_clock_ns(void);

/**
 * @brief The program's owner table and the owners in it, which MMGR_OWNERS defines once.
 *
 * @note A program that reaches a cellblock declares its owners with MMGR_OWNERS in one translation unit.
 *       A build with none fails at the link, and the link names these.
 */
extern CellOwner mmgr_owner_table[];
extern const size_t mmgr_owner_table_count;

/**
 * @brief One owner's place in the table, counted from 0, as the enumeration MMGR_OWNERS lays names it.
 *
 * @param[in] name_ The owner's identity, as declared.
 * @param[in] text_ The owner's name as a reader prints it, unread here.
 */
#define MMGR_OWNER_PLACE(name_, text_) mmgr_owner_place_##name_,

/**
 * @brief One owner's record as the table's initializer lays it: its name, its place from 1, nothing counted.
 *
 * @param[in] name_ The owner's identity, as declared.
 * @param[in] text_ The owner's name as a reader prints it [BORROWS].
 */
#define MMGR_OWNER_RECORD(name_, text_)                                                                                \
    {(text_), (size_t)mmgr_owner_place_##name_ + 1u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)},

/**
 * @brief One owner's identity defined: a const pointer to its record in the table, resolved by the linker.
 *
 * @param[in] name_ The owner's identity, as declared.
 * @param[in] text_ The owner's name as a reader prints it, unread here.
 */
#define MMGR_OWNER_POINTER(name_, text_) const CellOwner *const name_ = &mmgr_owner_table[mmgr_owner_place_##name_];

/**
 * @brief One owner's identity declared, for the header the owner's module reads it through.
 *
 * @param[in] name_ The owner's identity, as declared.
 * @param[in] text_ The owner's name as a reader prints it, unread here.
 */
#define MMGR_OWNER_EXTERN(name_, text_) extern const CellOwner *const name_;

/**
 * @brief The program's owners declared as data: the table laid, its count, and each owner's identity defined.
 *
 * @param[in] list_ The program's list of owners, a macro that takes one macro and applies it to each
 *                  owner. The form list_(macro_) expands to macro_(name_, text_) for every owner. Here
 *                  name_ is the identity the owner's module names and text_ the name a reader prints.
 * @note Expanded once, in the translation unit that owns the program's memory. The enumeration gives each
 *       owner its place. The table has room for the owners declared and holds their records in that
 *       order. Each identity is a pointer into it that the linker resolves: there is no registration and
 *       nothing runs before the first take. A list longer than MMGR_CARCER_OWNERS fails the build.
 */
#define MMGR_OWNERS(list_)                                                                                             \
    enum                                                                                                               \
    {                                                                                                                  \
        list_(MMGR_OWNER_PLACE) mmgr_owner_table_laid                                                                  \
    };                                                                                                                 \
    EMBED_STATIC_ASSERT((size_t)mmgr_owner_table_laid <= (size_t)MMGR_CARCER_OWNERS,                                   \
                        "more owners are declared than MMGR_CARCER_OWNERS allows");                                    \
    CellOwner mmgr_owner_table[(size_t)mmgr_owner_table_laid] = {list_(MMGR_OWNER_RECORD)};                            \
    const size_t mmgr_owner_table_count = (size_t)mmgr_owner_table_laid;                                               \
    list_(MMGR_OWNER_POINTER)

/**
 * @brief Whether an owner is one of the table's.
 *
 * @param[in] owner The owner, which may be NULL [BORROWS].
 * @return          EMBED_TRUE where it is the record at its own place in the table.
 */
embed_bool mmgr_owner_listed(const CellOwner *owner);

/**
 * @brief The payload bytes of the cells an owner has now.
 *
 * @param[in] owner The owner [BORROWS].
 * @return          The bytes, off one word.
 */
EMBED_INLINE size_t mmgr_owner_in_use(const CellOwner *owner)
{
    // Explicit cast reads the word as the bytes it counts
    return (size_t)embed_atomic_usize_load(&owner->in_use);
}

/**
 * @brief The most payload bytes an owner has had at once.
 *
 * @param[in] owner The owner [BORROWS].
 * @return          The bytes, off one word.
 */
EMBED_INLINE size_t mmgr_owner_hw(const CellOwner *owner)
{
    // Explicit cast reads the word as the bytes it counts
    return (size_t)embed_atomic_usize_load(&owner->hw);
}

/**
 * @brief Told of each give or growth that names an owner other than the one the cell counts to.
 *
 * @param[in] named      The owner the give or the growth named [BORROWS].
 * @param[in] cell_owner The owner the cell counts to, NULL where its owner word names no registered
 *                       owner [BORROWS].
 * @param[in] bytes      The cell's payload bytes.
 * @note Such a give or growth fails and changes nothing: the cell stays with its holder, counted
 *       to its owner. A program that sees this has a give in the wrong place, and a port logs it.
 * @note Weak, and the default does nothing. A port defines this name to replace it.
 */
void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes);

/**
 * @brief The bits each boundary has in the word that carries both, half the atomic size word, and the mask of a
 *        half.
 */
#define MMGR_CARCER_HALF_BITS ((unsigned int)((sizeof(embed_atomic_usize_value) * 8u) / 2u))
#define MMGR_CARCER_HALF_MASK ((embed_atomic_usize_value)(((embed_atomic_usize_value)1u << MMGR_CARCER_HALF_BITS) - 1u))

/**
 * @brief The most bytes one cellblock may hold: half a boundary's reach. The other half is the headroom the
 *        bumps a round refuses move the boundary into before the next writer settles it.
 *
 * @note LocusCarcerum() asserts each pool against this. A cell's header carries a size this large in the bits
 *       above its two state bits.
 */
#define MMGR_CARCER_BYTES_MOST ((size_t)1u << (MMGR_CARCER_HALF_BITS - 1u))

/**
 * @brief The most payload one bump may ask. It is the headroom the bumps in flight on the turnstiles' lanes
 *        share. Each of them may move its boundary once past MMGR_CARCER_BYTES_MOST before the refusal stops
 *        the rest. A take past it goes to the writer's path at once.
 */
#define MMGR_CARCER_WANT_MOST ((size_t)1u << (MMGR_CARCER_HALF_BITS - 8u))

/**
 * @brief One cellblock's state: its bytes, the two boundaries that grow toward each other, and the closed
 *        word a writer holds. Also the sink a refused store lands on, and the turnstiles every pass goes
 *        through.
 *
 * @note Written by the declaration that emits the cellblock and by the entries that cellblock owns. The base is
 *       the cellblock's own storage, and nothing derives an address from a site and an index.
 * @note bounds is the two boundaries in one word, each in bytes. Its low half is the offset just past the last
 *       persistent cell, and its high half the offset of the lowest temporary cell. A take bumps its tier's
 *       half by one fetch-add on the word. The old value carries the other boundary as it stood at that same
 *       instant: the room the take reads. A bump the gap cannot meet leaves the boundary where it moved it and
 *       adds its bytes to the tier's overrun, persistent_overrun or temporary_overrun. Every later bump of
 *       either tier fails, since the boundaries only move toward each other between writers and every bump
 *       reads them as one word.
 * @note The next writer takes the overrun back off the boundary before it reads the tier. Between a refusal
 *       and that writer the boundary stands past the tier's cells by the overrun. The page
 *       docs/concepts/locus-carcerum-algorithm.md writes the algorithm out and theory/locus_carcerum checks it.
 * @note closed is the writer's word: bit 0 while a compaction, a growth or a move works alone, bit 1
 *       while a context sleeps on the word. A pass that reads bit 0 set touches nothing and tries once more after
 *       a backoff step. The word sink is the one every store of a pass that touches nothing lands on.
 * @note turnstiles is the bank of lanes a pass badges in at, EmbedTurnstiles' way, which a writer drains after
 *       it sets the closed bit. Zero at rest, as the last member, which a declaration leaves to the zero fill
 *       of static storage.
 * @note The member in_use is this cellblock's total, counted where each owner's is. The member
 *       persistent_in_use is the part of it in the persistent tier, which a sort reads. The member movable is
 *       the option mmgr_movable_init assigns, and it reads zero without it.
 */
typedef struct
{
    uint8_t *const base;       /**< First byte of the cellblock [BORROWS]. */
    const size_t size;         /**< Bytes in the cellblock. */
    embed_atomic_usize bounds; /**< The persistent end in the low half, the temporary top in the high. */
    embed_atomic_usize
        persistent_overrun;               /**< Bytes refused takes moved the persistent end by since the last writer. */
    embed_atomic_usize temporary_overrun; /**< Bytes refused takes moved the temporary top by since the last writer. */
    embed_atomic_usize in_use;            /**< Payload bytes of the cells taken and not given back. */
    embed_atomic_usize persistent_in_use; /**< The part of in_use in the persistent tier. */
    embed_atomic_uptr movable;            /**< The movable option's state, as an integer, zero without it. */
    embed_atomic_word closed;             /**< The writer's bit, and the sleeper flag above it. */
    embed_atomic_usize sink;              /**< Where a store that must not land lands. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    embed_atomic_usize persistent_hw; /**< The highest value of the persistent end so far. */
    embed_atomic_usize temporary_hw;  /**< The most bytes taken from the top so far. */
#endif
    EmbedTurnstiles turnstiles; /**< The lanes a pass badges in at. */
} SeriesCellularum;

/** @brief The high-water members' first values, or nothing where the build tracks none. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_CARCER_STATE_HW , EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)
#else
#define MMGR_CARCER_STATE_HW
#endif

/**
 * @brief A cellblock's state as it stands over bytes_ bytes at base_ with no cell taken. Both tiers are
 *        empty, every mark is at zero, and the zero fill of the storage covers the turnstiles.
 *
 * @param[in] base_  First byte of the cellblock [BORROWS].
 * @param[in] bytes_ Bytes in the cellblock.
 */
#define MMGR_CARCER_STATE(base_, bytes_)                                                                               \
    {(base_),                                                                                                          \
     (bytes_),                                                                                                         \
     EMBED_ATOMIC_INIT((embed_atomic_usize_value)(bytes_) << MMGR_CARCER_HALF_BITS),                                   \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u),                                                                                            \
     EMBED_ATOMIC_INIT(0u) MMGR_CARCER_STATE_HW,                                                                       \
     {0}}

/**
 * @brief The identity a movable cell's taker holds: a const pointer to the cell's entry in the
 *        cellblock's translation table.
 *
 * @note The entry is the mutable word, and it is MMgr's alone: it names where the cell stands now, and a
 *       move changes it. The pointer the caller holds never changes while the cell lives, however often
 *       the cell moves. No header a caller can reach declares the layout.
 */
typedef struct CarcerMovableEntry CarcerMovableEntry;

/**
 * @brief What a program gives a cellblock for the movable option, once, through its guard's movable_init.
 *
 * @note The option is the program's to choose. A cellblock with none takes and gives as it always does,
 *       each entry tests one word, and its movable_buf_alloc answers NULL.
 * @note storage holds the table, its claim bits and the rows. It lies outside the prison, and so nothing of the
 *       option pins a tier or counts as a cell. A pool declared at compile time gives it a link-time address.
 * @note width bounds the lanes the cellblock's turnstiles may have open while the mover reads it quiet. Quiet
 *       is twice the lanes open at or under width plus one, with no pass since the mover's last reading. The
 *       system's logical processors bound it: no more contexts than that run at once.
 * @note rows are the rows of touches, one a lane and the last for the lanes past them, which
 *       mmgr_movable_touches reads.
 * @note thread_started and thread_wait together are the mover's thread. With either unset, nothing moves a cell
 *       on its own, and a caller can still call mmgr_movable_buf_move. The member thread_id badges a pass
 *       that names no owner. The member thread_yield runs between a sort's moves, with the cellblock open.
 * @note thread_wait answers EMBED_FALSE to end the mover. The member thread_moving, where set, hears as each
 *       sort's moves start and end. A mover at low priority can then raise itself across the copies a reader
 *       may wait on.
 */
typedef struct
{
    void *const storage;        /**< Bytes for the option, zero as given [BORROWS]. */
    const size_t storage_bytes; /**< Bytes at storage, MMGR_MOVABLE_STORAGE_BYTES at least. */
    const size_t cells;         /**< Movable cells the table can name at once. */
    const size_t width;         /**< The lanes open bound the mover reads quiet under, one at the fewest. */
    const size_t rows;          /**< Rows of touches, one a lane, the last shared by the lanes past it. */
    embed_bool (*const thread_started)(void (*run)(void *context), void *context); /**< Starts the mover. */
    size_t (*const thread_id)(void);                        /**< The id of the thread that calls, for a pass that names no owner. */
    embed_bool (*const thread_wait)(uint32_t milliseconds); /**< Sleeps the mover, EMBED_FALSE to end it. */
    void (*const thread_yield)(void);               /**< Gives up the processor between a sort's moves, where set. */
    void (*const thread_moving)(embed_bool moving); /**< Told as each move starts and ends, where set. */
} CarcerMovableInitCfg;

/**
 * @brief Arguments for the calls that reach a movable cell's bytes, each of which reads the members it names.
 *
 * @note A check holds offset and bytes against the cell, and a span past its end fails.
 */
typedef struct
{
    const CarcerMovableEntry *const cell;       /**< The cell [BORROWS]. */
    const CarcerMovableEntry *const other_cell; /**< The cell compare orders it against, NULL for src. */
    const size_t offset;                        /**< Where in the cell the call starts. */
    const size_t other_offset;                  /**< Where in other_cell compare starts. */
    void *const dst;                            /**< Where read copies to [BORROWS]. */
    const void *const src;                      /**< What write copies from and compare orders against [BORROWS]. */
    const size_t bytes;                         /**< Bytes the call covers. */
} CarcerMovableCfg;

/** @brief Bytes of the option's state ahead of its turnstiles, which locus_carcerum.c asserts it fits. */
#define MMGR_MOVABLE_HEAD_BYTES 192u

/** @brief The bits of one word of the option's claim bits: one an entry. */
#define MMGR_MOVABLE_CLAIM_BITS ((size_t)(8u * sizeof(embed_atomic_usize)))

/**
 * @brief The storage the movable option takes for a table of cells_ entries and rows_ rows of touches.
 *
 * @param[in] cells_ Movable cells the table can name at once.
 * @param[in] width_ The lanes open bound the mover reads quiet under, which takes no storage.
 * @param[in] rows_  Rows of touches.
 * @note A constant expression, so that a pool declaration can use this size. The size covers the state, one
 *       word an entry, one claim bit an entry in whole words, and two words a row.
 */
#define MMGR_MOVABLE_STORAGE_BYTES(cells_, width_, rows_)                                                              \
    (MMGR_MOVABLE_HEAD_BYTES +                                                                                         \
     (((cells_) + (((cells_) + MMGR_MOVABLE_CLAIM_BITS - 1u) / MMGR_MOVABLE_CLAIM_BITS) + (2u * (rows_))) *            \
      sizeof(embed_atomic_usize)))

/**
 * @brief Offset just past the last persistent cell.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The persistent end, in bytes.
 * @note The end as the word holds it. A trim a give left owed on the open word does not run here;
 *       mmgr_buf_available and the next take run it.
 */
EMBED_INLINE size_t mmgr_series_cellularum_persistent_end(const SeriesCellularum *series)
{
    // Explicit cast reads the word's low half as the offset it holds
    return (size_t)(embed_atomic_usize_load(&series->bounds) & MMGR_CARCER_HALF_MASK);
}

/**
 * @brief Offset of the lowest temporary cell.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The temporary top, in bytes.
 * @note The top as the word holds it. A trim a give left owed on the open word does not run here;
 *       mmgr_buf_available and the next take run it.
 */
EMBED_INLINE size_t mmgr_series_cellularum_temporary_top(const SeriesCellularum *series)
{
    // Explicit cast reads the word's high half as the offset it holds
    return (size_t)(embed_atomic_usize_load(&series->bounds) >> MMGR_CARCER_HALF_BITS);
}

/**
 * @brief Whether a cellblock has no cell out.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           EMBED_TRUE where both tiers stand empty.
 */
EMBED_INLINE embed_bool mmgr_series_cellularum_idle(const SeriesCellularum *series)
{
    // Explicit cast narrows the int result of the test to the embed_bool container
    return (embed_bool)((mmgr_series_cellularum_persistent_end(series) == 0u) &&
                        (mmgr_series_cellularum_temporary_top(series) == series->size));
}

/**
 * @brief The furthest point of a cellblock's persistent tier so far.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The bytes, 0 where the build tracks none (MMGR_ENABLE_HW_MEM_CAPACITY_CB).
 */
EMBED_INLINE size_t mmgr_series_cellularum_persistent_hw(const SeriesCellularum *series)
{
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    // Explicit cast reads the word as the bytes it counts
    return (size_t)embed_atomic_usize_load(&series->persistent_hw);
#else
    (void)series;
    return 0u;
#endif
}

/**
 * @brief The most bytes a cellblock's temporary tier took from the top at one time.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The bytes, 0 where the build tracks none (MMGR_ENABLE_HW_MEM_CAPACITY_CB).
 */
EMBED_INLINE size_t mmgr_series_cellularum_temporary_hw(const SeriesCellularum *series)
{
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    // Explicit cast reads the word as the bytes it counts
    return (size_t)embed_atomic_usize_load(&series->temporary_hw);
#else
    (void)series;
    return 0u;
#endif
}

/**
 * @brief The minimum security guard over one cellblock: a released cell keeps whatever is in it.
 *
 * @note EMBED_TABLE_LAYOUT asserts the thirteen members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets,
 * with nothing else.
 * @note Every entry is bound to the cellblock its declaration named. No call carries a cellblock
 *       argument and there is nothing to pass wrongly. A cellblock at this level has no release that
 *       zeroes. The difference between the two levels is what exists, not what a caller remembers
 *       to reach for.
 * @note A cell of either tier goes back the same way, through persistent_buf_release, which reads the tier off
 *       the cell's offset.
 */
typedef struct
{
    void *(*persistent_buf_alloc)(
        size_t size,
        const CellOwner *owner); /**< Takes size bytes from the bottom for owner, unzeroed [RETURNS OWNERSHIP]. */
    void (*persistent_buf_release)(
        void *prisoner,
        const CellOwner *owner); /**< Releases owner's prisoner of either tier, cell left as it is [TAKES OWNERSHIP]. */
    void *(*temporary_buf_alloc)(
        size_t size,
        const CellOwner *owner);        /**< Takes size bytes from the top for owner, unzeroed [RETURNS OWNERSHIP]. */
    embed_bool (*who_owns_buf)(const void *at); /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);              /**< Bytes between the two tiers. */
    void *(*buf_grow)(
        void *prisoner, size_t size,
        const CellOwner *owner); /**< Grows the newest cell of its tier to size bytes in place, unzeroed. */
    embed_bool (*movable_init)(const CarcerMovableInitCfg *args); /**< Assigns the movable option, once. */
    const CarcerMovableEntry *(*movable_buf_alloc)(
        size_t size, const CellOwner *owner); /**< Takes a cell a move may move: its identity, NULL where refused. */
    embed_bool (*movable_buf_release)(const CarcerMovableEntry *cell,
                                      const CellOwner *owner);     /**< Releases owner's movable cell, left as it is. */
    embed_bool (*movable_buf_read)(const CarcerMovableCfg *args);  /**< Copies out of a movable cell. */
    embed_bool (*movable_buf_write)(const CarcerMovableCfg *args); /**< Copies into a movable cell. */
    embed_iword (*movable_buf_compare)(const CarcerMovableCfg *args); /**< Orders a movable cell's bytes. */
    size_t (*movable_buf_measure)(const CarcerMovableCfg *args);      /**< Bytes before a movable cell's next zero. */
} MinimumSecurityGuard;
EMBED_TABLE_LAYOUT(MinimumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   who_owns_buf, buf_available, buf_grow, movable_init, movable_buf_alloc, movable_buf_release,
                   movable_buf_read, movable_buf_write, movable_buf_compare, movable_buf_measure);

/**
 * @brief The maximum security guard over one cellblock: every release zeroes the cell first.
 *
 * @note EMBED_TABLE_LAYOUT asserts the thirteen members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets,
 * with nothing else.
 * @note The same thirteen entries, and no unzeroed release among them. Each zero fill runs before the
 *       cell's header goes free: the bytes are zero at the instant they become available.
 */
typedef struct
{
    void *(*persistent_buf_alloc)(
        size_t size, const CellOwner *owner); /**< Takes size bytes from the bottom for owner [RETURNS OWNERSHIP]. */
    void (*persistent_buf_release)(
        void *prisoner,
        const CellOwner *owner); /**< Zeroes owner's cell of either tier, then releases it [TAKES OWNERSHIP]. */
    void *(*temporary_buf_alloc)(
        size_t size, const CellOwner *owner);   /**< Takes size bytes from the top for owner [RETURNS OWNERSHIP]. */
    embed_bool (*who_owns_buf)(const void *at); /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);              /**< Bytes between the two tiers. */
    void *(*buf_grow)(void *prisoner, size_t size,
                      const CellOwner *owner); /**< Grows the newest cell of its tier to size bytes in place, the added
                                                  bytes zeroed. */
    embed_bool (*movable_init)(const CarcerMovableInitCfg *args); /**< Assigns the movable option, once. */
    const CarcerMovableEntry *(*movable_buf_alloc)(
        size_t size, const CellOwner *owner); /**< Takes a cell a move may move: its identity, NULL where refused. */
    embed_bool (*movable_buf_release)(const CarcerMovableEntry *cell,
                                      const CellOwner *owner);    /**< Zeroes owner's movable cell, then releases it. */
    embed_bool (*movable_buf_read)(const CarcerMovableCfg *args); /**< Copies out of a movable cell. */
    embed_bool (*movable_buf_write)(const CarcerMovableCfg *args);    /**< Copies into a movable cell. */
    embed_iword (*movable_buf_compare)(const CarcerMovableCfg *args); /**< Orders a movable cell's bytes. */
    size_t (*movable_buf_measure)(const CarcerMovableCfg *args);      /**< Bytes before a movable cell's next zero. */
} MaximumSecurityGuard;
EMBED_TABLE_LAYOUT(MaximumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   who_owns_buf, buf_available, buf_grow, movable_init, movable_buf_alloc, movable_buf_release,
                   movable_buf_read, movable_buf_write, movable_buf_compare, movable_buf_measure);

/**
 * @brief The movable entries of one cellblock, bound to it through series_cellularum_.
 *
 * @param[in] prisonsite_ Site whose name every entry symbol carries.
 * @param[in] name_       Pool whose name the symbols carry alongside the site's.
 * @param[in] wipe_to_zero_on_release_ Security flag, which the option keeps for its releases and moves.
 * @param[in] series_cellularum_  The cellblock's record, as a pointer expression.
 */
#define MMGR_CARCER_MOVABLE_ENTRIES(prisonsite_, name_, wipe_to_zero_on_release_, series_cellularum_)                  \
    static embed_bool prisonsite_##_##name_##_movable_init(const CarcerMovableInitCfg *args)                           \
    {                                                                                                                  \
        return mmgr_movable_init((series_cellularum_), args, (wipe_to_zero_on_release_));                              \
    }                                                                                                                  \
    static const CarcerMovableEntry *prisonsite_##_##name_##_movable_buf_alloc(size_t size, const CellOwner *owner)    \
    {                                                                                                                  \
        return mmgr_movable_buf_alloc((series_cellularum_), size, owner);                                              \
    }                                                                                                                  \
    static embed_bool prisonsite_##_##name_##_movable_buf_release(const CarcerMovableEntry *cell,                      \
                                                                  const CellOwner *owner)                              \
    {                                                                                                                  \
        return mmgr_movable_buf_release((series_cellularum_), cell, owner);                                            \
    }                                                                                                                  \
    static embed_bool prisonsite_##_##name_##_movable_buf_read(const CarcerMovableCfg *args)                           \
    {                                                                                                                  \
        return mmgr_movable_buf_read((series_cellularum_), args);                                                      \
    }                                                                                                                  \
    static embed_bool prisonsite_##_##name_##_movable_buf_write(const CarcerMovableCfg *args)                          \
    {                                                                                                                  \
        return mmgr_movable_buf_write((series_cellularum_), args);                                                     \
    }                                                                                                                  \
    static embed_iword prisonsite_##_##name_##_movable_buf_compare(const CarcerMovableCfg *args)                       \
    {                                                                                                                  \
        return mmgr_movable_buf_compare((series_cellularum_), args);                                                   \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_movable_buf_measure(const CarcerMovableCfg *args)                            \
    {                                                                                                                  \
        return mmgr_movable_buf_measure((series_cellularum_), args);                                                   \
    }

/**
 * @brief Everything one cellblock is: its bytes, its state, and the entries bound to it.
 *
 * @param[in] prisonsite_ Site of the cellblock, which every emitted symbol carries.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, read by MMGR_CARCER_MEM.
 * @param[in] name_       Pool under the cellblock, which is also its name as a member of its site
 *                        [BORROWS].
 * @param[in] wipe_to_zero_on_release_ 1 where a release zeroes the cell first, 0 where it does not.
 * @note The site name is part of every symbol. Two sites may each hold a cellblock of the same name,
 *       at different security levels, with no collision.
 * @note wipe_to_zero_on_release_ is a literal: the branch it guards folds away and the security
 *       level costs nothing to read.
 * @note The extent comes from the pool two ways. The count name_##_bytes is the one its declaration
 *       received and is what the record carries, and sizeof is what the compiler laid down. The
 *       asserts test the second: they say something the first cannot say about itself.
 * @warning A pool too small to hold one cell fails the build. Only the alignment is a power of two,
 *          and nothing reads the size as a mask. A bad size is a mistake the language would
 *          otherwise accept. A wrong name or guard type fails on its own. Two pools are separate
 *          objects: they cannot share an address and there is no overlap to check.
 * @note No upper bound. The pool states how much storage the cellblock gets, and nothing in the
 *       library sizes anything from a ceiling over it.
 */
#define MMGR_CARCER_BODY(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    MMGR_PARS_CLAIMED_ONCE(name_);                                                                                     \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) >= (2u * MMGR_CARCER_ALIGN),                                 \
                        #prisonsite_ "." #name_ " is too small for one cell");                                         \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) <= MMGR_CARCER_BYTES_MOST,                                   \
                        #prisonsite_ "." #name_ " is larger than a tier boundary's half word can name");               \
    static SeriesCellularum prisonsite_##_##name_##_ctx = MMGR_CARCER_STATE(mmgr_pars_storage_##name_, name_##_bytes); \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_persistent_buf_alloc(size_t size, const CellOwner *owner)  \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_persistent_max_security_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                  \
        }                                                                                                              \
        return mmgr_persistent_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_persistent_buf_release(void *prisoner, const CellOwner *owner)                 \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_persistent_max_security_buf_release(&prisonsite_##_##name_##_ctx, prisoner, owner);                   \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_persistent_buf_release(&prisonsite_##_##name_##_ctx, prisoner, owner);                                \
        }                                                                                                              \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(1)                                                                                                 \
    static void *prisonsite_##_##name_##_temporary_buf_alloc(size_t size, const CellOwner *owner)                      \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_temporary_max_security_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                   \
        }                                                                                                              \
        return mmgr_temporary_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                                    \
    }                                                                                                                  \
    static embed_bool prisonsite_##_##name_##_who_owns_buf(const void *at)                                             \
    {                                                                                                                  \
        return mmgr_who_owns_buf(&prisonsite_##_##name_##_ctx, at);                                                    \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_buf_available(void)                                                          \
    {                                                                                                                  \
        const size_t available = mmgr_buf_available(&prisonsite_##_##name_##_ctx);                                     \
        return available;                                                                                              \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(2)                                                                                                 \
    static void *prisonsite_##_##name_##_buf_grow(void *prisoner, size_t size, const CellOwner *owner)                 \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_max_security_buf_grow(&prisonsite_##_##name_##_ctx, prisoner, size, owner);                    \
        }                                                                                                              \
        return mmgr_buf_grow(&prisonsite_##_##name_##_ctx, prisoner, size, owner);                                     \
    }                                                                                                                  \
    MMGR_CARCER_MOVABLE_ENTRIES(prisonsite_, name_, wipe_to_zero_on_release_, &prisonsite_##_##name_##_ctx)

/**
 * @brief One cellblock as a member of its site's type.
 *
 * @param[in] prisonsite_ Site of the cellblock, unused here.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, the member's type.
 * @param[in] name_       Pool under the cellblock, and the name of the member.
 * @param[in] wipe_to_zero_on_release_ Security flag, unused here.
 * @note Takes all four because the three readers share one tuple shape, as MMGR_MINIMUM_SECURITY
 *       describes.
 */
#define MMGR_CARCER_MEM(prisonsite_, type_, name_, wipe_to_zero_on_release_) type_ name_;

/**
 * @brief One cellblock's entries, in the order its guard declares them.
 *
 * @param[in] prisonsite_ Site whose name every entry symbol carries.
 * @param[in] type_       Guard type, unused here.
 * @param[in] name_       Pool whose name the symbols carry alongside the site's.
 * @param[in] wipe_to_zero_on_release_ Security flag, unused here.
 * @note MinimumSecurityGuard and MaximumSecurityGuard declare the same thirteen members in the same
 *       order. One initializer serves either level.
 */
#define MMGR_CARCER_SEAT(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    {prisonsite_##_##name_##_persistent_buf_alloc,                                                                     \
     prisonsite_##_##name_##_persistent_buf_release,                                                                   \
     prisonsite_##_##name_##_temporary_buf_alloc,                                                                      \
     prisonsite_##_##name_##_who_owns_buf,                                                                             \
     prisonsite_##_##name_##_buf_available,                                                                            \
     prisonsite_##_##name_##_buf_grow,                                                                                 \
     prisonsite_##_##name_##_movable_init,                                                                             \
     prisonsite_##_##name_##_movable_buf_alloc,                                                                        \
     prisonsite_##_##name_##_movable_buf_release,                                                                      \
     prisonsite_##_##name_##_movable_buf_read,                                                                         \
     prisonsite_##_##name_##_movable_buf_write,                                                                        \
     prisonsite_##_##name_##_movable_buf_compare,                                                                      \
     prisonsite_##_##name_##_movable_buf_measure},

/**
 * @brief Declares a minimum security cellblock over a pool.
 *
 * @param[in] name_ Pool under the cellblock, which is also its name as a member of its site
 *                  [BORROWS].
 * @note The pool carries its own extent: no size appears here. A cellblock is a layer over bytes
 *       that already exist. Which of ParsMemoriaeInternae or ParsMemoriaeExternum declared the
 *       bytes settles where they live.
 * @note Expands to a tuple in place of code. The site reads the same list three times, once for
 *       the cellblocks' bodies, once for its own members, once for their entries. An element has to
 *       stay data until the site says which of the three readings it gets.
 */
#define MMGR_MINIMUM_SECURITY(name_) (MinimumSecurityGuard, name_, 0)

/**
 * @brief Declares a maximum security cellblock over a pool.
 *
 * @param[in] name_ Pool under the cellblock, which is also its name as a member of its site
 *                  [BORROWS].
 * @note The trailing 1 is the wipe flag MMGR_CARCER_BODY reads to pick the release that zeroes on both
 *       tiers. The choice of this over MMGR_MINIMUM_SECURITY settles a cellblock's level, and
 *       nothing after the declaration can change it.
 * @note Carries no size, for the reason MMGR_MINIMUM_SECURITY gives.
 */
#define MMGR_MAXIMUM_SECURITY(name_) (MaximumSecurityGuard, name_, 1)

/**
 * @brief Strips a tuple's parentheses.
 *
 * @param[in] ... The tuple's elements, which its own parentheses deliver as this macro's arguments.
 * @return        Those elements, comma separated, with nothing around them.
 * @note Written MMGR_UNTUPLE tuple_, with no parentheses of its own: the tuple supplies them.
 */
#define MMGR_UNTUPLE(...) __VA_ARGS__

/**
 * @brief Applies one reader to one cellblock tuple, with the site spliced in ahead of it.
 *
 * @param[in] what_       Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_ Site, which reaches the reader ahead of the tuple's own elements.
 * @param[in] tuple_      One MMGR_MINIMUM_SECURITY or MMGR_MAXIMUM_SECURITY tuple.
 * @return                What the reader expands to.
 * @note Two steps, because the preprocessor counts a macro's arguments before it expands them. The inner call
 *       lets the flattened tuple reach the reader as separate arguments.
 */
#define MMGR_CARCER_APPLY(what_, prisonsite_, tuple_) MMGR_CARCER_APPLY_(what_, prisonsite_, MMGR_UNTUPLE tuple_)

/**
 * @brief Expands to what_(prisonsite_, __VA_ARGS__).
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader ahead of the elements.
 * @param[in] ...         The tuple's elements, already flattened by MMGR_UNTUPLE.
 * @return                What the reader expands to.
 * @note The inner half of MMGR_CARCER_APPLY's two steps. The preprocessor expands its arguments before
 *       it puts them in. MMGR_UNTUPLE runs here and the tuple's elements reach the reader as separate
 *       arguments in place of one.
 */
#define MMGR_CARCER_APPLY_(what_, prisonsite_, ...) what_(prisonsite_, __VA_ARGS__)

/**
 * @brief Applies the reader to a site's one cellblock tuple.
 *
 * @param[in] what_              Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_        Site, which reaches the reader ahead of the tuple's own elements.
 * @param[in] SeriesCellularum1_ The cellblock's tuple.
 * @note The base of the expansion. Every longer line ends in this one, and it alone names no other.
 */
#define MMGR_CARCER_W1(what_, prisonsite_, SeriesCellularum1_) MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum1_)

/**
 * @brief Applies the reader to a site's two cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple, which MMGR_CARCER_W1 takes.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple, applied after it.
 * @note The step every longer line repeats: expand the line one shorter, then apply the reader once
 *       more. The preprocessor cannot step through a list: each cellblock count needs a line of its
 *       own.
 */
#define MMGR_CARCER_W2(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_)                                     \
    MMGR_CARCER_W1(what_, prisonsite_, SeriesCellularum1_)                                                             \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum2_)

/**
 * @brief Applies the reader to a site's three cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple, applied after MMGR_CARCER_W2 expands the first two.
 * @note MMGR_CARCER_WALK selects this line for a site that declares three cellblocks.
 */
#define MMGR_CARCER_W3(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_)                 \
    MMGR_CARCER_W2(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_)                                         \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum3_)

/**
 * @brief Applies the reader to a site's four cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple.
 * @param[in] SeriesCellularum4_ Fourth cellblock's tuple, applied after MMGR_CARCER_W3 expands the first three.
 * @note MMGR_CARCER_WALK selects this line for a site that declares four cellblocks.
 */
#define MMGR_CARCER_W4(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_,                 \
                       SeriesCellularum4_)                                                                             \
    MMGR_CARCER_W3(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_)                     \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum4_)

/**
 * @brief Applies the reader to a site's five cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple.
 * @param[in] SeriesCellularum4_ Fourth cellblock's tuple.
 * @param[in] SeriesCellularum5_ Fifth cellblock's tuple, applied after MMGR_CARCER_W4 expands the first four.
 * @note MMGR_CARCER_WALK selects this line for a site that declares five cellblocks.
 */
#define MMGR_CARCER_W5(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_,                 \
                       SeriesCellularum4_, SeriesCellularum5_)                                                         \
    MMGR_CARCER_W4(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_, SeriesCellularum4_) \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum5_)

/**
 * @brief Applies the reader to a site's six cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple.
 * @param[in] SeriesCellularum4_ Fourth cellblock's tuple.
 * @param[in] SeriesCellularum5_ Fifth cellblock's tuple.
 * @param[in] SeriesCellularum6_ Sixth cellblock's tuple, applied after MMGR_CARCER_W5 expands the first five.
 * @note MMGR_CARCER_WALK selects this line for a site that declares six cellblocks.
 */
#define MMGR_CARCER_W6(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_,                 \
                       SeriesCellularum4_, SeriesCellularum5_, SeriesCellularum6_)                                     \
    MMGR_CARCER_W5(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_, SeriesCellularum4_, \
                   SeriesCellularum5_)                                                                                 \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum6_)

/**
 * @brief Applies the reader to a site's seven cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple.
 * @param[in] SeriesCellularum4_ Fourth cellblock's tuple.
 * @param[in] SeriesCellularum5_ Fifth cellblock's tuple.
 * @param[in] SeriesCellularum6_ Sixth cellblock's tuple.
 * @param[in] SeriesCellularum7_ Seventh cellblock's tuple, applied after MMGR_CARCER_W6 expands the first six.
 * @note MMGR_CARCER_WALK selects this line for a site that declares seven cellblocks.
 */
#define MMGR_CARCER_W7(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_,                 \
                       SeriesCellularum4_, SeriesCellularum5_, SeriesCellularum6_, SeriesCellularum7_)                 \
    MMGR_CARCER_W6(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_, SeriesCellularum4_, \
                   SeriesCellularum5_, SeriesCellularum6_)                                                             \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum7_)

/**
 * @brief Applies the reader to a site's eight cellblock tuples.
 *
 * @param[in] what_              Reader to apply.
 * @param[in] prisonsite_        Site, which reaches the reader with every tuple.
 * @param[in] SeriesCellularum1_ First cellblock's tuple.
 * @param[in] SeriesCellularum2_ Second cellblock's tuple.
 * @param[in] SeriesCellularum3_ Third cellblock's tuple.
 * @param[in] SeriesCellularum4_ Fourth cellblock's tuple.
 * @param[in] SeriesCellularum5_ Fifth cellblock's tuple.
 * @param[in] SeriesCellularum6_ Sixth cellblock's tuple.
 * @param[in] SeriesCellularum7_ Seventh cellblock's tuple.
 * @param[in] SeriesCellularum8_ Eighth cellblock's tuple, applied after MMGR_CARCER_W7 expands the first seven.
 * @note MMGR_CARCER_WALK selects this line for a site that declares eight cellblocks.
 * @warning The last line written. A site that declares nine cellblocks pastes MMGR_CARCER_W9, which does
 *          not exist. That line is the whole fix. Nothing else changes and no configured ceiling
 *          has a part in it.
 */
#define MMGR_CARCER_W8(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_,                 \
                       SeriesCellularum4_, SeriesCellularum5_, SeriesCellularum6_, SeriesCellularum7_,                 \
                       SeriesCellularum8_)                                                                             \
    MMGR_CARCER_W7(what_, prisonsite_, SeriesCellularum1_, SeriesCellularum2_, SeriesCellularum3_, SeriesCellularum4_, \
                   SeriesCellularum5_, SeriesCellularum6_, SeriesCellularum7_)                                         \
    MMGR_CARCER_APPLY(what_, prisonsite_, SeriesCellularum8_)

/**
 * @brief Expands the walk that matches the cellblock count.
 *
 * @param[in] what_       Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_ Site, forwarded to the walk ahead of the tuples.
 * @param[in] ...         MMGR_MINIMUM_SECURITY and MMGR_MAXIMUM_SECURITY tuples, one per cellblock.
 * @note EMBED_CAT builds the line's name from EMBED_NARG's count of the tuples.
 * @warning EMBED_NARG gives 1 for an empty list. A site that declares no cellblocks reaches
 *          MMGR_CARCER_W1 and fails there with too few arguments for the reader.
 */
#define MMGR_CARCER_WALK(what_, prisonsite_, ...)                                                                      \
    EMBED_CAT(MMGR_CARCER_W, EMBED_NARG(__VA_ARGS__))(what_, prisonsite_, __VA_ARGS__)

/**
 * @brief Declares a prison site and the cellblocks built at it.
 *
 * @param[in] prisonsite_ Name of the site. Its cellblocks are members of it.
 * @param[in] ...         MMGR_MINIMUM_SECURITY and MMGR_MAXIMUM_SECURITY declarations, one each.
 * @note Everything it emits is data with its initial value. Nothing runs at startup, and a cellblock's first
 *       byte is the address of its own storage, which the linker resolves.
 * @note The warden is the const struct this emits under the site's name. Every call reaches a
 *       cellblock through it.
 * @note Every emitted symbol carries the site's name. A program may declare as many sites as it
 *       likes and two of them may each hold a cellblock called the same thing.
 * @note A cellblock's entries are bound to that cellblock. No call names one and none can reach
 *       another's bytes. A minimum security cellblock has no release that zeroes, and a maximum
 *       security one has no plain release.
 * @warning The bytes, the state and the warden are all static. A declaration in a header gives
 *          every translation unit that includes it a site of its own in place of one they share.
 */
#define LocusCarcerum(prisonsite_, ...)                                                                                \
    MMGR_CARCER_WALK(MMGR_CARCER_BODY, prisonsite_, __VA_ARGS__)                                                       \
    EMBED_TABLE_STORAGE struct                                                                                         \
    {                                                                                                                  \
        MMGR_CARCER_WALK(MMGR_CARCER_MEM, prisonsite_, __VA_ARGS__)                                                    \
    } prisonsite_ EMBED_UNUSED = {MMGR_CARCER_WALK(MMGR_CARCER_SEAT, prisonsite_, __VA_ARGS__)}

/**
 * @brief Takes size bytes from a cellblock's persistent tier.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The registered owner that counts the cell [BORROWS].
 * @return               Start of the cell, or NULL when owner is NULL or not registered or the
 *                       cellblock cannot meet the take [RETURNS OWNERSHIP].
 * @note Reached through the cellblock's own guard, never called by name.
 * @note A persistent cell stands until the same cellblock's persistent_buf_release takes it.
 * @note A cell handed out adds its payload bytes, the request rounded to a word, to owner's in_use
 *       and raises owner's hw to it. A refused take counts nothing.
 * @note The take is one pass through the cellblock's turnstiles and one fetch-add on the persistent end. The
 *       cell is the bytes the bump covered, and its header and owner word go down once the take reads the
 *       room off the top. A bump the gap cannot meet adds its bytes to the tier's overrun. The take then
 *       closes the cellblock, drains the turnstiles and walks the tier alone.
 * @note The walk merges every run of free cells into one. It takes the first run that holds the take and
 *       splits it where its rest holds a cell. It returns the free run at the end to the gap, and bumps the
 *       gap where no run fits. Only then does the take fail, and no path branches on what another context
 *       did.
 * @warning The bytes stay as they are, and a cell laid over bytes a prisoner occupied before still
 *          carries what that prisoner left. A cellblock declared MMGR_MAXIMUM_SECURITY zeroes, and it
 *          zeroes on release.
 */
MMGR_ALLOC_SIZE(2) void *mmgr_persistent_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner);

/**
 * @brief Takes size bytes from a maximum security cellblock's persistent tier.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The registered owner that counts the cell [BORROWS].
 * @return               As mmgr_persistent_buf_alloc answers [RETURNS OWNERSHIP].
 * @note The take mmgr_persistent_buf_alloc makes. A maximum security cellblock zeroes on release.
 */
MMGR_ALLOC_SIZE(2)
void *mmgr_persistent_max_security_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner);

/**
 * @brief Releases a prisoner and leaves the cell's bytes as they are.
 *
 * @param[in,out] series   Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner First byte of the cell [TAKES OWNERSHIP].
 * @param[in]     owner    The owner the cell counts to, as its owner word names it [BORROWS].
 * @note The cell's offset against the persistent end says which tier it lies in. A NULL prisoner or a
 *       NULL owner changes nothing.
 * @note The give is one fetch-or on the cell's owner word, which sets its completion bit, and one store on the
 *       cell's header, which goes free. The cell's payload bytes, read off its header, come off owner's in_use
 *       in the context whose fetch-or found the bit clear. A release that names an owner other than the cell's
 *       changes nothing, and mmgr_carcer_hw_misnamed hears of it. One of a cell given back already changes
 *       nothing either. No pass and no wait: the cell's words are its holder's alone.
 * @note A free cell stays in its tier for the compaction that a take the gap cannot meet runs. The compaction
 *       merges it with its free neighbors, takes it where it fits, or returns it to the gap where it reaches
 *       the end.
 * @warning A prisoner outside this cellblock's own bytes stops the program with MMGR_FATAL: there is no state
 *          to carry on from.
 * @warning prisoner is dead once this returns and its bytes stay as they are. The cellblock may hand
 *          them out again.
 * @warning The bound is the cellblock's storage and never a cell boundary. An address inside these
 *          bytes that is not the first byte of a cell is still read as a cell's word.
 */
void mmgr_persistent_buf_release(SeriesCellularum *series, void *prisoner, const CellOwner *owner);

/**
 * @brief Zeroes a cell, then releases the prisoner.
 *
 * @param[in,out] series   Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner First byte of the cell, zeroed before release [TAKES OWNERSHIP].
 * @param[in]     owner    The owner the cell counts to, as its owner word names it [BORROWS].
 * @note The extent comes from the cell's own header. A caller cannot under-zero a cell with a count of
 *       fewer bytes than it carries. A NULL prisoner or a NULL owner changes nothing.
 * @note The zero fill is the only step that separates this from mmgr_persistent_buf_release. It runs
 *       once the owner named proves to be the cell's and the cell still counts. A release that names
 *       another owner, or one of a cell given back already, zeroes nothing.
 * @warning A prisoner outside this cellblock's own bytes stops the program with MMGR_FATAL.
 * @warning prisoner is dead once this returns. The cellblock may hand those bytes out again.
 * @warning The bound is the cellblock's storage and never a cell boundary. An address inside these
 *          bytes that is not the first byte of a cell still gets its extent from the bytes ahead
 *          of it. The zero fill then runs for whatever length those hold.
 */
void mmgr_persistent_max_security_buf_release(SeriesCellularum *series, void *prisoner, const CellOwner *owner);

/**
 * @brief Grows a cell to size payload bytes in place, where it is the newest cell of its tier.
 *
 * @param[in,out] series   Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner First byte of the cell [BORROWS].
 * @param[in]     size     Payload bytes the cell is to carry, rounded as a take rounds them.
 * @param[in]     owner    The owner the cell counts to, as its owner word names it, which also counts
 *                         the growth [BORROWS].
 * @return                 The cell's first byte after the growth: the prisoner itself for a persistent
 *                         cell, or for one that carries size already. A temporary cell, whose bytes
 *                         moved down, gets a lower address. NULL with nothing changed where the cell is
 *                         not the newest of its tier, since a later take stands above it. NULL also
 *                         where the gap cannot meet the growth. A NULL prisoner, a NULL owner, or an
 *                         owner other than the cell's returns NULL. The wrong owner reaches
 *                         mmgr_carcer_hw_misnamed [RETURNS OWNERSHIP].
 * @note The growth closes the cellblock, drains the turnstiles and works alone. The newest cell of a tier is
 *       the cell its boundary stands against. That is the persistent end at the cell's end, or the temporary
 *       top at the cell's header. The growth reads the gap exactly, stores the boundary past the growth, and
 *       moves no other cell. A caller whose growth fails takes a larger cell, copies into it and releases
 *       the first, as it would without this entry.
 * @note The added bytes are as they lie, and mmgr_max_security_buf_grow zeroes them. A persistent growth's
 *       hold the word that stood behind the payload, a temporary growth's the cell's own old header and
 *       payload.
 * @warning The prisoner's address is dead after a temporary growth that answers a lower one: the
 *          answered address is the cell's from then on. A prisoner outside this cellblock's own
 *          bytes stops with MMGR_FATAL, as a release does.
 */
MMGR_ALLOC_SIZE(3) void *mmgr_buf_grow(SeriesCellularum *series, void *prisoner, size_t size, const CellOwner *owner);

/**
 * @brief Grows a cell in place as mmgr_buf_grow does, and zeroes the bytes the growth adds.
 *
 * @param[in,out] series   Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner First byte of the cell [BORROWS].
 * @param[in]     size     Payload bytes the cell is to carry.
 * @param[in]     owner    The owner that counts a persistent cell's growth [BORROWS].
 * @return                 As mmgr_buf_grow answers [RETURNS OWNERSHIP].
 * @note The zero fill covers the growth's bytes alone: the payload the cell carried keeps its bytes, moved
 *       for a temporary cell.
 */
MMGR_ALLOC_SIZE(3)
void *mmgr_max_security_buf_grow(SeriesCellularum *series, void *prisoner, size_t size, const CellOwner *owner);

/**
 * @brief Takes size bytes from a cellblock's temporary tier.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The registered owner that counts the cell [BORROWS].
 * @return               Start of the cell, or NULL when owner is NULL or not registered or the
 *                       cellblock cannot meet the take [RETURNS OWNERSHIP].
 * @note The caller gives the cell back through mmgr_persistent_buf_release, as with a persistent cell,
 *       which takes the bytes off owner. The cell carries owner in the word behind its payload.
 * @note The take is mmgr_persistent_buf_alloc's with the arithmetic mirrored. It is one pass through the
 *       cellblock's turnstiles and one fetch-add on the temporary top, which moves it down by the cell. The
 *       take reads the room off the persistent end, and a bump that crosses it adds its bytes to the tier's
 *       overrun. The take then closes the cellblock, drains the turnstiles and walks the tier alone.
 * @note The walk merges every run of free cells into one. It takes the run that fits nearest the cellblock's
 *       size and splits it where its rest holds a cell. It returns the free run at the top to the gap, and
 *       bumps the top where no run fits. Only then does the take fail.
 * @warning The bytes stay as they are, and an allocation returns whatever the last prisoner left. A
 *          cellblock declared MMGR_MAXIMUM_SECURITY zeroes, and it zeroes on release.
 */
void *mmgr_temporary_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner);

/**
 * @brief Takes size bytes from a maximum security cellblock's temporary tier.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The registered owner that counts the cell [BORROWS].
 * @return               As mmgr_temporary_buf_alloc answers [RETURNS OWNERSHIP].
 * @note The take mmgr_temporary_buf_alloc makes. A maximum security cellblock zeroes on release.
 */
MMGR_ALLOC_SIZE(2)
void *mmgr_temporary_max_security_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner);

/**
 * @brief Whether at lies inside the cellblock's bytes.
 *
 * @param[in] series Cellblock to test against [BORROWS].
 * @param[in] at     Address to test [BORROWS].
 * @return           EMBED_TRUE when at lies in the cellblock's storage, which is [base, base + size).
 * @note One unsigned compare covers both ends, since an address below base wraps to a difference
 *       larger than any size.
 * @warning Any address in the cellblock answers true, the first byte of a cell or any other. This says
 *          where an address is and nothing of what is there. A true answer is not a warrant for a
 *          release of at. The releases read a cell's word from whatever address they receive.
 */
embed_bool mmgr_who_owns_buf(const SeriesCellularum *series, const void *at);

/**
 * @brief The owner a cell counts to, read off the word behind its payload.
 *
 * @param[in] series   Cellblock the prisoner came from [BORROWS].
 * @param[in] prisoner First byte of the cell [BORROWS].
 * @return             The registered owner the cell's owner word names. NULL where prisoner is NULL, lies
 *                     outside the cellblock, is a cell no context holds, or its word names no registered owner
 *                     [BORROWS].
 * @note A holder that grows or gives back a cell another part of the program took names the owner this
 *       answers. Such a line grows in the hands of whoever writes it.
 * @warning The bound is the cellblock's storage and never a cell boundary, as for a release. An address
 *          inside these bytes that is not the first byte of a cell reads as a cell's word.
 */
const CellOwner *mmgr_owner_of_buf(const SeriesCellularum *series, const void *prisoner);

/**
 * @brief The bytes between the two tiers.
 *
 * @param[in,out] series Cellblock to read [BORROWS].
 * @return               The temporary top less the persistent end, read off one word.
 * @note An allocation out of that gap needs a cell header from the same bytes, and the request
 *       rounds up to a whole word first. The gap cannot meet a request of exactly this many bytes.
 * @note The trims a give left owed on the open word run first, and the gap read has every owed trim run.
 * @warning The answer is a snapshot. Any allocation at all leaves it stale before the caller can act
 *          on it.
 */
size_t mmgr_buf_available(SeriesCellularum *series);

/**
 * @brief Zeroes size bytes at prisoner and releases nothing.
 *
 * @param[in,out] prisoner First byte to clear [BORROWS].
 * @param[in]     size     Bytes to clear.
 * @note The stores are volatile: the compiler cannot drop a clear of bytes nothing reads afterwards
 *       as dead work. The middle goes a word at a time, the two edges a byte at a time.
 * @note The extent is the caller's to state. A release that zeroes takes it from the cell's own word.
 * @warning prisoner must be writable for size bytes.
 */
void mmgr_zero_buf(void *prisoner, size_t size);

/**
 * @brief Rounds size up to a whole machine word.
 *
 * @param[in] size Count to round.
 * @return         size rounded up to a multiple of MMGR_CARCER_ALIGN, and size itself when it already
 *                 is one.
 * @note A size of 0 rounds to 0. The allocators do not round a request this way. They carry a
 *       request of 0 up to one word first: no cell goes out empty.
 * @warning The round adds MMGR_CARCER_ALIGN - 1 before the mask. A size within a word of SIZE_MAX wraps
 *          to 0.
 */
size_t mmgr_align_up_buf(size_t size);

/**
 * @brief Puts a cellblock back to the state of its declaration. Both tiers are empty, the closed word and
 *        the turnstiles at rest, the option unassigned, and no byte of its storage touched.
 *
 * @param[in,out] series Cellblock to empty [BORROWS].
 * @note Every cell the cellblock handed out is dead once this returns. A suite calls it between
 *       cases over one declared cellblock. A program with a cellblock to empty while another context
 *       reaches it has a cellblock whose cells that context still holds.
 * @note Nothing outside the tiers reads as a cell, and so no cell's words change.
 */
void mmgr_series_cellularum_emptied(SeriesCellularum *series);

/**
 * @brief Counts bytes that lie in no cellblock to an owner, as a ring's or a page's are.
 *
 * @param[in]     owner The registered owner [BORROWS].
 * @param[in]     bytes The bytes.
 * @note An owner's counts are the program's total, and memory a program takes outside a
 *       cellblock belongs in it. No cellblock's own total moves.
 */
void mmgr_owner_bytes_taken(const CellOwner *owner, size_t bytes);

/**
 * @brief Takes bytes mmgr_owner_bytes_taken counted off their owner.
 *
 * @param[in]     owner The owner that counted them [BORROWS].
 * @param[in]     bytes The bytes.
 */
void mmgr_owner_bytes_given(const CellOwner *owner, size_t bytes);

/**
 * @brief A cellblock's fragmentation: the bytes its persistent tier occupies and no cell's payload holds, as
 *        a percentage of the bytes it occupies.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The percentage, 0 to 100, read against the hundredth of the bytes occupied rounded up, and 0
 *                   below a hundred bytes occupied.
 * @note Read off the persistent end and the tier's total, with no walk. Each cell's words count among
 *       the bytes no payload holds, and a tier of small cells reads some fragmentation with no free cell.
 * @note The temporary tier reads nothing: its cells last one call, and its top comes back as they end. A
 *       free cell no movable cell stands above is no work for a mover either. The mover remembers a sort
 *       that finds nothing to move, and does not run it again until the reading rises.
 */
size_t mmgr_series_cellularum_fragmentation(const SeriesCellularum *series);

/**
 * @brief Assigns a cellblock's movable option: the translation table, its claim bits, the rows of touches and
 *        the mover's thread. A cellblock's guard calls this, as movable_init.
 *
 * @param[in,out] series Cellblock the option serves [BORROWS].
 * @param[in]     args   The storage, the sizes and the thread's calls [BORROWS].
 * @param[in]     wipes  Whether a cell a move leaves or a release gives back gets a zero fill first.
 * @return               EMBED_TRUE where the option is on and, where the caller gave a thread, the
 *                       thread started. EMBED_FALSE where it is on already, the storage is short or a
 *                       size is zero, nothing changed. EMBED_FALSE also where the thread did not
 *                       start, which leaves the option on and nothing to move a cell.
 * @note Assigned once, at a time no context reaches a movable cell. The cellblock's turnstiles are what the
 *       mover reads as occupancy: the lanes open against the option's width.
 */
embed_bool mmgr_movable_init(SeriesCellularum *series, const CarcerMovableInitCfg *args, embed_bool wipes);

/**
 * @brief Takes a persistent cell a move may move, and names it in the table.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The registered owner that counts the cell [BORROWS].
 * @return               The cell's identity. NULL where the option is off, the cellblock cannot meet
 *                       the take or the table has no free entry, nothing counted.
 * @note The identity is a const pointer to the cell's entry. It stands for the cell's life whatever moves
 *       the cell, and the movable_buf_ calls are the only way from it to the bytes.
 * @warning A pointer kept past mmgr_movable_buf_release names an entry a later take may reuse.
 */
const CarcerMovableEntry *mmgr_movable_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner);

/**
 * @brief Gives a movable cell back and frees its entry.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     cell   The cell [BORROWS].
 * @param[in]     owner  The owner that counts the cell [BORROWS].
 * @return               EMBED_TRUE where it went back. EMBED_FALSE where cell is NULL, the entry is free or
 *                       owner is not the cell's, of which mmgr_carcer_hw_misnamed hears, nothing changed.
 * @note One pass through the cellblock's turnstiles: the entry read, the cell given back as a release does,
 *       the entry's word and claim bit cleared. A move is a writer, which drains the passes before it and
 *       closes the cellblock to those after. The two never reach the entry at once.
 */
embed_bool mmgr_movable_buf_release(SeriesCellularum *series, const CarcerMovableEntry *cell, const CellOwner *owner);

/**
 * @brief Copies args->bytes from args->offset of a movable cell to args->dst.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     args   The cell, offset, dst and bytes [BORROWS].
 * @return               EMBED_TRUE where copied, EMBED_FALSE where the span passes the cell's end or
 *                       the entry is free, nothing copied.
 * @note Thin: one pass through the cellblock's turnstiles, badged with the id of the thread that calls. The
 *       entry's word read, the bytes copied, and the lane let go. A pass that finds the cellblock closed to a move
 *       backs off one step and tries once more.
 */
embed_bool mmgr_movable_buf_read(const SeriesCellularum *series, const CarcerMovableCfg *args);

/**
 * @brief Copies args->bytes from args->src to args->offset of a movable cell.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     args   The cell, offset, src and bytes [BORROWS].
 * @return               As mmgr_movable_buf_read answers.
 * @note Atomic against a move and not against another writer: two contexts that write the same bytes order
 *       themselves.
 */
embed_bool mmgr_movable_buf_write(SeriesCellularum *series, const CarcerMovableCfg *args);

/**
 * @brief Orders args->bytes of a movable cell against args->src, or against another cell where
 *        args->other_cell holds one.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     args   The cell, offset, and src or other_cell and other_offset, and bytes [BORROWS].
 * @return               As memor.cmp orders them, and 1 where a span passes its cell's end or an entry is
 *                       free.
 * @note Two cells are the most one call reaches, both read under the one pass.
 */
embed_iword mmgr_movable_buf_compare(const SeriesCellularum *series, const CarcerMovableCfg *args);

/**
 * @brief The bytes from args->offset of a movable cell to the next zero byte.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     args   The cell and offset [BORROWS].
 * @return               The bytes, or the bytes to the cell's end where no zero stands before it.
 */
size_t mmgr_movable_buf_measure(const SeriesCellularum *series, const CarcerMovableCfg *args);

/**
 * @brief Moves one movable cell into the first free cell below it, highest cell first.
 *
 * @param[in,out] series Cellblock whose cells to move [BORROWS].
 * @return               EMBED_TRUE where a cell moved. EMBED_FALSE where none could at this reading: no
 *                       free run fits below any of them.
 * @note The mover's step, a writer's. It closes the cellblock, drains the turnstiles, and reads the highest
 *       named cell. It walks the persistent tier below it for the first free run that holds the cell. It
 *       copies the cell, carries its owner word over, flips the entry to the copy, frees the cell left behind,
 *       and opens the cellblock. The same state moves the same cell to the same place.
 * @note Called until it answers EMBED_FALSE, the cells stand as low as the free runs let them. The free run
 *       that reaches the tier's end returns to the gap with each walk.
 * @note The copy carries the cell's owner word, and so the owner's count stands as it was through the move.
 * @note A cell no free run below takes rules out every cell at least its size under it. Those look
 *       through the same free runs for as much room.
 * @warning Each cell moved, and each smaller size refused, costs a read of the named part of the table.
 *          It costs a walk of the tier below the cell too.
 */
embed_bool mmgr_movable_buf_move(SeriesCellularum *series);

/**
 * @brief The mover: what the thread the program gave runs, until its thread_wait answers EMBED_FALSE.
 *
 * @param[in,out] series Cellblock with the option assigned and thread_wait given [BORROWS].
 * @note Each MMGR_MOVABLE_READ_MS it reads the cellblock, and fragmentation under MMGR_MOVABLE_SETTLE_PERCENT
 *       ends the reading. Over it, MMGR_MOVABLE_QUIET_READINGS in a row must find the cellblock quiet. Quiet
 *       is twice the lanes its turnstiles have open at or under the option's width plus one, with no pass
 *       since the last reading. It then moves cells one at a time, the cellblock open and the processor
 *       yielded between moves. It reads the same after each: a pass, or lanes open past quiet, says the moves
 *       meet pressure, and it backs off.
 * @note Each backoff doubles the wait, up to MMGR_MOVABLE_BACKOFF_MOST_MS. A sort that ends with nothing left
 *       to move resets the wait, and the mover remembers the fragmentation it left. Nothing runs again until
 *       the reading rises past that or falls under the threshold first.
 */
void mmgr_movable_run(SeriesCellularum *series);

/**
 * @brief The contexts queued at the cellblock's turnstiles now: those that found every open lane busy and wait for
 *        a lane.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The queued contexts, 0 without the option.
 */
size_t mmgr_movable_queue_depth(const SeriesCellularum *series);

/**
 * @brief One row of touches a lane, counted as passes go through it.
 *
 * @param[in,out] series  Cellblock to read [BORROWS].
 * @param[in]     at      The row, from 0: the lane of the same place.
 * @param[out]    id      The id of the last badge through the lane. That is an owner's place, or a thread's id
 *                        plus one for a pass that names no owner [BORROWS].
 * @param[out]    touches The passes through the lane so far [BORROWS].
 * @return                EMBED_TRUE where the row holds a lane's passes, EMBED_FALSE past the rows in use or
 *                        for a lane with no pass through it yet.
 * @note A telemetry row, counted by one add as each pass badges in, and nothing decides on it. The option's last
 *       row counts every lane past the rest, and answers an id of SIZE_MAX.
 */
embed_bool mmgr_movable_touches(const SeriesCellularum *series, size_t at, size_t *id, size_t *touches);

EMBED_END_DECLS

#endif
