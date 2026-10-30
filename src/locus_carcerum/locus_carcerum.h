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
 * @note The storage is the caller's. A cellblock is declared over a pool - ParsMemoriaeInternae or
 *       ParsMemoriaeExternum in mmgr.h - and its CarcerCellBlock record holds that pool's address.
 *       The linker resolves it. No call computes a cellblock's address from a site and an index,
 *       and which memory a cellblock sits in was settled where its pool was declared.
 * @note A cellblock has two tiers. The persistent tier runs up from base and the temporary tier runs
 *       down from size, and one atomic word holds both boundaries.
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
 * @note Off by default because the two marks cost a size_t each in every CarcerCellBlock, and a
 *       build that never reads them should not carry them.
 * @note Adds CarcerCellBlock::persistent_hw and CarcerCellBlock::temporary_hw, and the update that
 *       raises them in carcer_grow, which both tiers reach. One mark per tier: neither counts the
 *       other's bytes.
 * @note The #ifndef leaves a build's own definition standing, whether it arrives on the command line
 *       or from a header included ahead of this one.
 * @note Takes 0 or 1 alone. Both readers are #if, which would take any non-zero as set. The check
 *       below keeps a mistyped value from quietly widening every CarcerCellBlock.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif
#if (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 0) && (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 1)
#error "MMGR_ENABLE_HW_MEM_CAPACITY_CB must be 0 or 1"
#endif

/**
 * @brief The most owners the owner queue holds.
 *
 * @note The queue is the library's one list of owners, in the order they registered, and an owner's
 *       place in it is half its key. A registration past the queue's room is refused, and that owner
 *       takes nothing.
 */
#ifndef MMGR_CARCER_OWNERS
#define MMGR_CARCER_OWNERS 128u
#endif

#include "embed_atomic.h"

/**
 * @brief Alignment every prisoner is handed out at, which is one machine word.
 *
 * @note Derived from the word in place of a number. A build at another width gets the alignment
 *       that width actually needs.
 * @note LocusCarcerum() puts this alignment on the storage it declares. This rounds only
 *       running offsets inside a cellblock.
 */
#define MMGR_CARCER_ALIGN ((size_t)sizeof(embed_word))

/**
 * @brief Asserts MMGR_CARCER_ALIGN is a power of two.
 *
 * @note An offset is rounded by masking off its low bits, which lands on a multiple only for a power
 *       of two. carcer_round and CARCER_HDR in locus_carcerum.c both round that way.
 */
EMBED_STATIC_ASSERT((MMGR_CARCER_ALIGN & (MMGR_CARCER_ALIGN - 1u)) == 0u,
                    "the cellblock rounds offsets by masking, which needs a power of two alignment");

/**
 * @brief One owner of cells: its name, its key, the payload bytes of the cells it has now on both
 *        tiers, and the most it had at once.
 *
 * @note The caller's storage, registered once through mmgr_owner_register and named on every take,
 *       give and growth after. Every entry refuses NULL in place of an owner, and every take refuses
 *       an owner not registered.
 * @note The key is the owner's place in the owner queue, counted from 1, and the time it registered,
 *       in milliseconds on the clock mmgr_carcer_hw_clock_ms reads. place is 0 until the registration
 *       lands, and registered_ms is written before place is.
 * @note A take adds the cell's payload bytes to in_use and raises hw to it; a give takes the same
 *       bytes off. Every count is an atomic word, added to and raised in one step each, and an owner
 *       shared between contexts counts exactly.
 * @note The bytes counted are the cell's own: the request rounded to a whole word, counted on the take
 *       and read off the cell's word on the give, and a matched pair returns in_use to where it stood.
 * @note A persistent cell comes off the owner its give names. A temporary cell carries its owner in
 *       the word behind its payload, and comes off that owner whether a single give, a rewind or its
 *       lifetime's end drops it; the owner a single give names for a temporary cell is not read.
 * @warning An owner must outlive every cell counted to it: the queue holds its address, and a rewind
 *          writes the owner through the address a temporary cell carries.
 */
typedef struct
{
    const char *name;           /**< The owner's name, as a reader prints it, unique in the queue [BORROWS]. */
    embed_atomic_usize place;   /**< The owner's place in the owner queue, from 1; 0 until registered. */
    uint32_t registered_ms;     /**< When the owner registered, in milliseconds on the library's clock. */
    embed_atomic_usize in_use;  /**< Payload bytes of the cells the owner has now. */
    embed_atomic_usize hw;      /**< Running maximum of in_use. */
} CellOwner;

/**
 * @brief An owner named name_, not yet registered, with nothing counted.
 *
 * @param[in] name_ The owner's name [BORROWS].
 */
#define MMGR_CELL_OWNER(name_) {(name_), EMBED_ATOMIC_INIT(0u), 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)}

/**
 * @brief Registers an owner: puts it at the back of the owner queue and gives it its key.
 *
 * @param[in,out] owner The owner to register [BORROWS].
 * @return              EMBED_TRUE where the owner registered; EMBED_FALSE, with nothing changed, where
 *                      owner or its name is NULL, the owner is registered already, an owner of the same
 *                      name is, or the queue is full.
 * @note An owner registers once. Registrations from two contexts at once are taken one at a time, and
 *       the second of two registering one owner, or two owners of one name, is refused.
 * @note The time is read once, as the registration lands, through mmgr_carcer_hw_clock_ms.
 */
embed_bool mmgr_owner_register(CellOwner *owner);

/**
 * @brief The owners registered, as a reader counts them.
 *
 * @return The owners in the queue, at most MMGR_CARCER_OWNERS.
 */
size_t mmgr_owner_count(void);

/**
 * @brief The owner at one place in the owner queue.
 *
 * @param[in] at The place, from 0, below mmgr_owner_count.
 * @return       The owner, whose own place is at + 1, or NULL past the count [BORROWS].
 */
CellOwner *mmgr_owner_at(size_t at);

/**
 * @brief The library's clock, in milliseconds, which an owner's key and a temporary cell's lifetime
 *        are read on.
 *
 * @return The milliseconds since a start the port chooses, wrapping at 2^32.
 * @note Weak, and the default answers 0 every time: a build that defines none has every owner
 *       registered at 0 and no temporary cell whose lifetime ends. A port defines this name to replace
 *       it.
 * @note Read as a 32-bit count that wraps. A lifetime is measured as the difference of two readings, and
 *       holds across the wrap while it is below 2^31 milliseconds.
 */
uint32_t mmgr_carcer_hw_clock_ms(void);

/**
 * @brief Told of each temporary cell given back because its lifetime ended before its holder gave it.
 *
 * @param[in] owner      The owner the cell was counted to [BORROWS].
 * @param[in] bytes      The cell's payload bytes, taken off owner.
 * @param[in] overdue_ms Milliseconds the cell stood past its lifetime when it was given back.
 * @note The end of a lifetime is a backstop: a holder gives its temporary cells back itself, by a give
 *       or a rewind. Every time the backstop gives one back it calls this, for a port to log.
 * @note Weak, and the default does nothing. A port defines this name to replace it.
 */
void mmgr_carcer_hw_expired(const CellOwner *owner, size_t bytes, uint32_t overdue_ms);

/**
 * @brief Bits each tier's boundary takes in the word that holds both: half the word.
 *
 * @note The word is embed_atomic_usize, keyed on SIZE_MAX the way that type is. The half follows the
 *       machine word.
 */
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
#define MMGR_CARCER_TIER_BITS 32u
#else
#define MMGR_CARCER_TIER_BITS 16u
#endif

/**
 * @brief The shift that turns an offset in bytes into the units a boundary is kept in, which are
 *        machine words.
 *
 * @note Every offset in a cellblock is a multiple of MMGR_CARCER_ALIGN: the low bits it drops are
 *       zero, and keeping offsets in words lets a half word reach MMGR_CARCER_ALIGN times further.
 */
#define MMGR_CARCER_UNIT_SHIFT ((MMGR_CARCER_ALIGN == 8u) ? 3u : ((MMGR_CARCER_ALIGN == 4u) ? 2u : 1u))
EMBED_STATIC_ASSERT(((size_t)1u << MMGR_CARCER_UNIT_SHIFT) == MMGR_CARCER_ALIGN,
                    "a boundary is kept in machine words, and the alignment must be 2, 4 or 8 bytes");

/** @brief The mask of one boundary's half of the word. */
#define MMGR_CARCER_TIER_MASK (((embed_atomic_usize_value)1u << MMGR_CARCER_TIER_BITS) - 1u)

/**
 * @brief The most bytes one cellblock may hold: the largest offset a half word names, in bytes.
 *
 * @note The temporary top stands at the cellblock's size while that tier is empty: the size itself
 *       must fit a half. LocusCarcerum() asserts each pool against this, and mmgr_host_cellblock_made
 *       refuses a block past it.
 */
#define MMGR_CARCER_BYTES_MOST ((size_t)MMGR_CARCER_TIER_MASK << MMGR_CARCER_UNIT_SHIFT)

/**
 * @brief Both boundaries as one word: the persistent end in the low half and the temporary top in the
 *        high half, each in machine words.
 *
 * @param[in] end_ Offset just past the last persistent cell, in bytes.
 * @param[in] top_ Offset of the lowest temporary cell, in bytes.
 * @return         The word, a constant expression where both arguments are.
 */
#define MMGR_CARCER_TIERS(end_, top_)                                                                                  \
    ((((embed_atomic_usize_value)(top_) >> MMGR_CARCER_UNIT_SHIFT) << MMGR_CARCER_TIER_BITS) |                        \
     ((embed_atomic_usize_value)(end_) >> MMGR_CARCER_UNIT_SHIFT))

/**
 * @brief The persistent end a tiers word holds, in bytes.
 *
 * @param[in] tiers The word.
 * @return          Offset just past the last persistent cell.
 */
EMBED_INLINE size_t mmgr_tiers_persistent_end(embed_atomic_usize_value tiers)
{
    // Explicit cast reads the low half, machine words, back as the byte offset it names
    return (size_t)(tiers & MMGR_CARCER_TIER_MASK) << MMGR_CARCER_UNIT_SHIFT;
}

/**
 * @brief The temporary top a tiers word holds, in bytes.
 *
 * @param[in] tiers The word.
 * @return          Offset of the lowest temporary cell.
 */
EMBED_INLINE size_t mmgr_tiers_temporary_top(embed_atomic_usize_value tiers)
{
    // Explicit cast reads the high half, machine words, back as the byte offset it names
    return (size_t)(tiers >> MMGR_CARCER_TIER_BITS) << MMGR_CARCER_UNIT_SHIFT;
}

/**
 * @brief One cellblock's state: its bytes and the two tiers that grow toward each other, both
 *        boundaries in one atomic word.
 *
 * @note Written by the declaration that emits the cellblock and by the entries that cellblock owns.
 *       The base is the cellblock's own storage, and nothing derives an address from a site and an
 *       index.
 * @note tiers holds the persistent end and the temporary top together, MMGR_CARCER_TIERS's way. A
 *       take reads the word, finds the room between the two, and moves one boundary by one swap on
 *       the word read; a swap that fails is a take whose room another context took first, and it
 *       reads the word again and decides over what it says now. The tiers cannot cross, since every
 *       move of either is one swap on the word that names both.
 * @note The persistent end bounds that tier and does not count what is in use. A freed cell inside
 *       the tier stays in it until a release trims the end.
 */
typedef struct
{
    uint8_t *const base;     /**< First byte of the cellblock [BORROWS]. */
    const size_t size;       /**< Bytes in the cellblock. */
    embed_atomic_usize tiers; /**< Both boundaries, MMGR_CARCER_TIERS's way: the persistent end low, the temporary top high. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    embed_atomic_usize persistent_hw; /**< Running maximum of persistent_end. */
    embed_atomic_usize temporary_hw;  /**< Running maximum of the bytes taken from the top. */
#endif
} CarcerCellBlock;

/** @brief The high-water members' first values, or nothing where the build tracks none. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_CARCER_STATE_HW , EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)
#else
#define MMGR_CARCER_STATE_HW
#endif

/**
 * @brief A cellblock's state as it stands over bytes_ bytes at base_ with no cell taken: both tiers
 *        empty and every mark at zero.
 *
 * @param[in] base_  First byte of the cellblock [BORROWS].
 * @param[in] bytes_ Bytes in the cellblock.
 */
#define MMGR_CARCER_STATE(base_, bytes_)                                                                               \
    {                                                                                                                  \
        (base_), (bytes_), EMBED_ATOMIC_INIT(MMGR_CARCER_TIERS(0u, bytes_)) MMGR_CARCER_STATE_HW                        \
    }

/**
 * @brief Offset just past the last persistent cell, read off the tiers word.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The persistent end, in bytes.
 */
EMBED_INLINE size_t mmgr_cellblock_persistent_end(const CarcerCellBlock *cellblock)
{
    return mmgr_tiers_persistent_end(embed_atomic_usize_load(&cellblock->tiers));
}

/**
 * @brief Offset of the lowest temporary cell, read off the tiers word.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The temporary top, in bytes.
 */
EMBED_INLINE size_t mmgr_cellblock_temporary_top(const CarcerCellBlock *cellblock)
{
    return mmgr_tiers_temporary_top(embed_atomic_usize_load(&cellblock->tiers));
}

/**
 * @brief The minimum security guard over one cellblock: a released cell keeps whatever is in it.
 *
 * @note EMBED_TABLE_LAYOUT asserts the nine members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 * @note Every entry is bound to the cellblock its declaration named. No call carries a cellblock
 *       argument and there is nothing to pass wrongly. A cellblock at this level has no zeroing
 *       release. The difference between the two levels is what exists, not what a caller remembers
 *       to reach for.
 */
typedef struct
{
    void *(*persistent_buf_alloc)(
        size_t size,
        CellOwner *owner); /**< Takes size bytes from the bottom for owner, unzeroed [RETURNS OWNERSHIP]. */
    void (*persistent_buf_release)(
        void *prisoner, CellOwner *owner);      /**< Releases owner's prisoner, cell left as it is [TAKES OWNERSHIP]. */
    void *(*temporary_buf_alloc)(size_t size, CellOwner *owner,
                                 uint32_t lifetime_ms); /**< Takes size bytes from the top for owner, for lifetime_ms, unzeroed [RETURNS OWNERSHIP]. */
    size_t (*temporary_buf_mark)(void);         /**< The current top, for temporary_buf_release. */
    void (*temporary_buf_release)(size_t mark); /**< Restores the top a mark reported, zeroing nothing. */
    void (*temporary_buf_reset)(void);          /**< Releases the whole temporary tier at once. */
    embed_bool (*who_owns_buf)(const void *at); /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);              /**< Bytes between the two tiers. */
    void *(*buf_grow)(void *prisoner, size_t size,
                      CellOwner *owner); /**< Grows the newest cell of its tier to size bytes in place, unzeroed. */
} MinimumSecurityGuard;
EMBED_TABLE_LAYOUT(MinimumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   temporary_buf_mark, temporary_buf_release, temporary_buf_reset, who_owns_buf, buf_available,
                   buf_grow);

/**
 * @brief The maximum security guard over one cellblock: every release zeroes the cell first.
 *
 * @note EMBED_TABLE_LAYOUT asserts the nine members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 * @note The same nine entries, and no unzeroed release among them. Each zeroing runs before the
 *       boundary moves: the bytes are zero at the instant they become available. Where the
 *       zeroing is skipped and the boundary still moves, mmgr_max_security_buf_return carries it in
 *       its warnings.
 */
typedef struct
{
    void *(*persistent_buf_alloc)(
        size_t size, CellOwner *owner); /**< Takes size bytes from the bottom for owner [RETURNS OWNERSHIP]. */
    void (*persistent_buf_release)(void *prisoner,
                                   CellOwner *owner); /**< Zeroes owner's cell, then releases it [TAKES OWNERSHIP]. */
    void *(*temporary_buf_alloc)(size_t size, CellOwner *owner,
                                 uint32_t lifetime_ms); /**< Takes size bytes from the top for owner, for lifetime_ms [RETURNS OWNERSHIP]. */
    size_t (*temporary_buf_mark)(void);               /**< The current top, for temporary_buf_release. */
    void (*temporary_buf_release)(size_t mark);       /**< Zeroes back to the mark, then restores the top. */
    void (*temporary_buf_reset)(void);                /**< Zeroes the whole temporary tier, then releases it. */
    embed_bool (*who_owns_buf)(const void *at);       /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);                    /**< Bytes between the two tiers. */
    void *(*buf_grow)(void *prisoner, size_t size,
                      CellOwner *owner); /**< Grows the newest cell of its tier to size bytes in place, the added bytes zeroed. */
} MaximumSecurityGuard;
EMBED_TABLE_LAYOUT(MaximumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   temporary_buf_mark, temporary_buf_release, temporary_buf_reset, who_owns_buf, buf_available,
                   buf_grow);

/**
 * @brief Everything one cellblock is: its bytes, its state, and the entries bound to it.
 *
 * @param[in] prisonsite_ Site the cellblock is built at, which every emitted symbol carries.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, read by MMGR_CARCER_MEM.
 * @param[in] name_       Pool the cellblock is built over, which is also the name it is reached by
 *                        as a member of its site [BORROWS].
 * @param[in] wipe_to_zero_on_release_ 1 where a release zeroes the cell first, 0 where it does not.
 * @note The site name is part of every symbol. Two sites may each hold a cellblock of the same name,
 *       at different security levels, without colliding.
 * @note wipe_to_zero_on_release_ is a literal: the branch it guards folds away and the security
 *       level costs nothing to read.
 * @note The extent comes from the pool two ways. name_##_bytes is the count its declaration was
 *       handed and is what the record carries, and sizeof is what the compiler laid down. The
 *       asserts test the second: they say something the first cannot say about itself.
 * @warning A pool too small to hold one cell fails the build. Only the alignment is a power of two,
 *          and nothing reads the size as a mask. A bad size is a mistake the language would
 *          otherwise accept. A wrong name or guard type fails on its own, and two pools are separate
 *          objects: they cannot share an address and there is no overlap to check.
 * @note No upper bound. The pool states how much storage the cellblock gets, and nothing in the
 *       library sizes anything from a ceiling over it.
 */
#define MMGR_CARCER_BODY(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    MMGR_PARS_CLAIMED_ONCE(name_);                                                                                     \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) >= (2u * MMGR_CARCER_ALIGN),                                 \
                        #prisonsite_ "." #name_ " is too small for one cell");                                         \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) <= MMGR_CARCER_BYTES_MOST,                                   \
                        #prisonsite_ "." #name_ " is larger than a tier boundary's half word can name");                \
    static CarcerCellBlock prisonsite_##_##name_##_ctx = MMGR_CARCER_STATE(mmgr_pars_storage_##name_, name_##_bytes);  \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_persistent_buf_alloc(size_t size, CellOwner *owner)        \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_persistent_max_security_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                  \
        }                                                                                                              \
        return mmgr_persistent_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_persistent_buf_release(void *prisoner, CellOwner *owner)                       \
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
    static void *prisonsite_##_##name_##_temporary_buf_alloc(size_t size, CellOwner *owner, uint32_t lifetime_ms)      \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_temporary_max_security_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner, lifetime_ms);      \
        }                                                                                                              \
        return mmgr_temporary_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner, lifetime_ms);                       \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_temporary_buf_mark(void)                                                     \
    {                                                                                                                  \
        const size_t mark = mmgr_temporary_buf_mark(&prisonsite_##_##name_##_ctx);                                     \
        return mark;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_release(size_t mark)                                             \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_max_security_buf_return(&prisonsite_##_##name_##_ctx, mark);                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_temporary_buf_release(&prisonsite_##_##name_##_ctx, mark);                                            \
        }                                                                                                              \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_reset(void)                                                      \
    {                                                                                                                  \
        prisonsite_##_##name_##_temporary_buf_release(prisonsite_##_##name_##_ctx.size);                               \
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
    MMGR_ALLOC_SIZE(2) static void *prisonsite_##_##name_##_buf_grow(void *prisoner, size_t size, CellOwner *owner)    \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_max_security_buf_grow(&prisonsite_##_##name_##_ctx, prisoner, size, owner);                    \
        }                                                                                                              \
        return mmgr_buf_grow(&prisonsite_##_##name_##_ctx, prisoner, size, owner);                                     \
    }

/**
 * @brief One cellblock as a member of its site's type.
 *
 * @param[in] prisonsite_ Site the cellblock is built at, unused here.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, the member's type.
 * @param[in] name_       Pool the cellblock is built over, and the name the member is given.
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
 * @note MinimumSecurityGuard and MaximumSecurityGuard declare the same nine members in the same
 *       order. One initializer serves either level.
 */
#define MMGR_CARCER_SEAT(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    {prisonsite_##_##name_##_persistent_buf_alloc,  prisonsite_##_##name_##_persistent_buf_release,                    \
     prisonsite_##_##name_##_temporary_buf_alloc,   prisonsite_##_##name_##_temporary_buf_mark,                        \
     prisonsite_##_##name_##_temporary_buf_release, prisonsite_##_##name_##_temporary_buf_reset,                       \
     prisonsite_##_##name_##_who_owns_buf,          prisonsite_##_##name_##_buf_available,                             \
     prisonsite_##_##name_##_buf_grow},

/**
 * @brief Declares a minimum security cellblock over a pool.
 *
 * @param[in] name_ Pool the cellblock is built over, which is also the name it is reached by as a
 *                  member of its site [BORROWS].
 * @note The pool carries its own extent: no size is written here. A cellblock is a dressing over
 *       bytes that already exist, and where those bytes live was settled by which of
 *       ParsMemoriaeInternae or ParsMemoriaeExternum declared them.
 * @note Expands to a tuple in place of code. The site reads the same list three times, once for
 *       the cellblocks' bodies, once for its own members, once for their entries. An element has to
 *       stay data until the site says which of the three it is being read as.
 */
#define MMGR_MINIMUM_SECURITY(name_) (MinimumSecurityGuard, name_, 0)

/**
 * @brief Declares a maximum security cellblock over a pool.
 *
 * @param[in] name_ Pool the cellblock is built over, which is also the name it is reached by as a
 *                  member of its site [BORROWS].
 * @note The trailing 1 is the wipe flag MMGR_CARCER_BODY reads to pick the zeroing release on both
 *       tiers. Choosing this over MMGR_MINIMUM_SECURITY is where a cellblock's level is settled, and
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
 * @note Two steps, because a macro's arguments are counted before they are expanded. The inner call
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
 * @note The inner half of MMGR_CARCER_APPLY's two steps. Its arguments are expanded before they are
 *       substituted: MMGR_UNTUPLE runs here and the tuple's elements reach the reader as separate
 *       arguments in place of one.
 */
#define MMGR_CARCER_APPLY_(what_, prisonsite_, ...) what_(prisonsite_, __VA_ARGS__)

/**
 * @brief Applies the reader to a site's one cellblock tuple.
 *
 * @param[in] what_       Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_ Site, which reaches the reader ahead of the tuple's own elements.
 * @param[in] CellBlock1_ The cellblock's tuple.
 * @note The base of the expansion. Every longer line ends in this one, and it alone names no other.
 */
#define MMGR_CARCER_W1(what_, prisonsite_, CellBlock1_) MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock1_)

/**
 * @brief Applies the reader to a site's two cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple, which MMGR_CARCER_W1 takes.
 * @param[in] CellBlock2_ Second cellblock's tuple, applied after it.
 * @note The step every longer line repeats: expand the line one shorter, then apply the reader once
 *       more. The preprocessor cannot step through a list: each cellblock count needs a line of its
 *       own.
 */
#define MMGR_CARCER_W2(what_, prisonsite_, CellBlock1_, CellBlock2_)                                                   \
    MMGR_CARCER_W1(what_, prisonsite_, CellBlock1_)                                                                    \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock2_)

/**
 * @brief Applies the reader to a site's three cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple, applied after MMGR_CARCER_W2 expands the first two.
 * @note MMGR_CARCER_WALK selects this line for a site declaring three cellblocks.
 */
#define MMGR_CARCER_W3(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_)                                      \
    MMGR_CARCER_W2(what_, prisonsite_, CellBlock1_, CellBlock2_)                                                       \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock3_)

/**
 * @brief Applies the reader to a site's four cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple.
 * @param[in] CellBlock4_ Fourth cellblock's tuple, applied after MMGR_CARCER_W3 expands the first three.
 * @note MMGR_CARCER_WALK selects this line for a site declaring four cellblocks.
 */
#define MMGR_CARCER_W4(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_)                         \
    MMGR_CARCER_W3(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_)                                          \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock4_)

/**
 * @brief Applies the reader to a site's five cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple.
 * @param[in] CellBlock4_ Fourth cellblock's tuple.
 * @param[in] CellBlock5_ Fifth cellblock's tuple, applied after MMGR_CARCER_W4 expands the first four.
 * @note MMGR_CARCER_WALK selects this line for a site declaring five cellblocks.
 */
#define MMGR_CARCER_W5(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_)            \
    MMGR_CARCER_W4(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_)                             \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock5_)

/**
 * @brief Applies the reader to a site's six cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple.
 * @param[in] CellBlock4_ Fourth cellblock's tuple.
 * @param[in] CellBlock5_ Fifth cellblock's tuple.
 * @param[in] CellBlock6_ Sixth cellblock's tuple, applied after MMGR_CARCER_W5 expands the first five.
 * @note MMGR_CARCER_WALK selects this line for a site declaring six cellblocks.
 */
#define MMGR_CARCER_W6(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_,            \
                       CellBlock6_)                                                                                    \
    MMGR_CARCER_W5(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_)                \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock6_)

/**
 * @brief Applies the reader to a site's seven cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple.
 * @param[in] CellBlock4_ Fourth cellblock's tuple.
 * @param[in] CellBlock5_ Fifth cellblock's tuple.
 * @param[in] CellBlock6_ Sixth cellblock's tuple.
 * @param[in] CellBlock7_ Seventh cellblock's tuple, applied after MMGR_CARCER_W6 expands the first six.
 * @note MMGR_CARCER_WALK selects this line for a site declaring seven cellblocks.
 */
#define MMGR_CARCER_W7(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_,            \
                       CellBlock6_, CellBlock7_)                                                                       \
    MMGR_CARCER_W6(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_, CellBlock6_)   \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock7_)

/**
 * @brief Applies the reader to a site's eight cellblock tuples.
 *
 * @param[in] what_       Reader to apply.
 * @param[in] prisonsite_ Site, which reaches the reader with every tuple.
 * @param[in] CellBlock1_ First cellblock's tuple.
 * @param[in] CellBlock2_ Second cellblock's tuple.
 * @param[in] CellBlock3_ Third cellblock's tuple.
 * @param[in] CellBlock4_ Fourth cellblock's tuple.
 * @param[in] CellBlock5_ Fifth cellblock's tuple.
 * @param[in] CellBlock6_ Sixth cellblock's tuple.
 * @param[in] CellBlock7_ Seventh cellblock's tuple.
 * @param[in] CellBlock8_ Eighth cellblock's tuple, applied after MMGR_CARCER_W7 expands the first seven.
 * @note MMGR_CARCER_WALK selects this line for a site declaring eight cellblocks.
 * @warning The last line written. A site declaring nine cellblocks pastes MMGR_CARCER_W9, which does
 *          not exist. Writing that line is the whole fix. Nothing else changes and no configured
 *          ceiling is involved.
 */
#define MMGR_CARCER_W8(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_,            \
                       CellBlock6_, CellBlock7_, CellBlock8_)                                                          \
    MMGR_CARCER_W7(what_, prisonsite_, CellBlock1_, CellBlock2_, CellBlock3_, CellBlock4_, CellBlock5_, CellBlock6_,   \
                   CellBlock7_)                                                                                        \
    MMGR_CARCER_APPLY(what_, prisonsite_, CellBlock8_)

/**
 * @brief Expands the walk matching the cellblock count.
 *
 * @param[in] what_       Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_ Site, forwarded to the walk ahead of the tuples.
 * @param[in] ...         MMGR_MINIMUM_SECURITY and MMGR_MAXIMUM_SECURITY tuples, one per cellblock.
 * @note EMBED_CAT builds the line's name from EMBED_NARG's count of the tuples.
 * @warning EMBED_NARG gives 1 for an empty list. A site declaring no cellblocks reaches
 *          MMGR_CARCER_W1 and fails there with too few arguments for the reader.
 */
#define MMGR_CARCER_WALK(what_, prisonsite_, ...)                                                                      \
    EMBED_CAT(MMGR_CARCER_W, EMBED_NARG(__VA_ARGS__))(what_, prisonsite_, __VA_ARGS__)

/**
 * @brief Declares a prison site and the cellblocks built at it.
 *
 * @param[in] prisonsite_ Name of the site. Its cellblocks are reached as members of it.
 * @param[in] ...         MMGR_MINIMUM_SECURITY and MMGR_MAXIMUM_SECURITY declarations, one each.
 * @note Everything it emits is initialized data. Nothing runs at startup, and a cellblock's first
 *       byte is the address of its own storage, which the linker resolves.
 * @note The warden is the const struct this emits under the site's name. Every call reaches a
 *       cellblock through it.
 * @note Every emitted symbol carries the site's name. A program may declare as many sites as it
 *       likes and two of them may each hold a cellblock called the same thing.
 * @note A cellblock's entries are bound to that cellblock. No call names one and none can reach
 *       another's bytes. A minimum security cellblock has no zeroing release and a maximum security
 *       one has no plain release.
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
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size      Bytes wanted.
 * @param[in,out] owner     The registered owner the cell is counted to [BORROWS].
 * @return                  Start of the cell, or NULL when owner is NULL or not registered or the
 *                          cellblock cannot meet the take [RETURNS OWNERSHIP].
 * @note Reached through the cellblock's own guard, never called by name.
 * @note A persistent cell has no lifetime: it stands until the same cellblock's persistent_buf_release
 *       takes it.
 * @note A cell handed out adds its payload bytes, the request rounded to a word, to owner's in_use
 *       and raises owner's hw to it. A refused take counts nothing.
 * @note The take is one swap on the tiers word, which moves the persistent end up by the cell. No take
 *       steps through the tier for a cell given back below the end: such a cell waits, free, until the
 *       cells above it are given back, and the give at the end trims them all off together.
 * @note A take the gap cannot meet gives back the temporary cells at the top whose lifetimes have
 *       ended, as a temporary take does, and tries once more.
 * @warning The bytes are not zeroed, and a cell laid over bytes a prisoner occupied before still
 *          carries what that prisoner left. A cellblock declared MMGR_MAXIMUM_SECURITY zeroes, and it
 *          zeroes on release.
 */
MMGR_ALLOC_SIZE(2) void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner);

/**
 * @brief Takes size bytes from a maximum security cellblock's persistent tier.
 *
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size      Bytes wanted.
 * @param[in,out] owner     The registered owner the cell is counted to [BORROWS].
 * @return                  As mmgr_persistent_buf_alloc answers [RETURNS OWNERSHIP].
 * @note The take mmgr_persistent_buf_alloc makes, but for the backstop it runs where the gap cannot
 *       meet it: each temporary cell it gives back is zeroed first.
 */
MMGR_ALLOC_SIZE(2)
void *mmgr_persistent_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner);

/**
 * @brief Releases a prisoner, leaving the cell's bytes as they are.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner  First byte of the cell [TAKES OWNERSHIP].
 * @param[in,out] owner     The owner the cell was counted to [BORROWS].
 * @note Which tier the cell came from is read from its address. A release cannot be given to the
 *       wrong tier. A NULL prisoner or a NULL owner returns without touching the cellblock.
 * @note The cell's payload bytes, read off its header, come off owner's in_use before the release.
 * @note A prisoner outside this cellblock's own bytes returns without touching it. A pointer from
 *       another cellblock cannot move this one's boundaries.
 * @warning prisoner is dead once this returns and its bytes are not zeroed. The cellblock may hand
 *          them out again.
 * @warning The bound is the cellblock's storage and never a cell boundary. An address inside these
 *          bytes that is not the first byte of a cell is still read as a cell's word.
 */
void mmgr_persistent_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner);

/**
 * @brief Zeroes a cell, then releases the prisoner.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner  First byte of the cell, zeroed before release [TAKES OWNERSHIP].
 * @param[in,out] owner     The owner the cell was counted to [BORROWS].
 * @note The extent comes from the cell's own header. A caller cannot under-zero a cell by naming
 *       fewer bytes than it carries. A NULL prisoner or a NULL owner returns without touching the
 *       cellblock.
 * @note The zeroing is the only step that separates this from mmgr_persistent_buf_release, which it
 *       calls to do the release.
 * @note A prisoner outside this cellblock's own bytes returns before the zeroing. A pointer from
 *       another cellblock is neither cleared nor released.
 * @warning prisoner is dead once this returns. The cellblock may hand those bytes out again.
 * @warning The bound is the cellblock's storage and never a cell boundary. An address inside these
 *          bytes that is not the first byte of a cell still has its extent read from the bytes ahead
 *          of it, and is zeroed for whatever length those hold.
 */
void mmgr_persistent_max_security_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner);

/**
 * @brief Grows a cell to size payload bytes in place, where it is the newest cell of its tier.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner  First byte of the cell [BORROWS].
 * @param[in]     size      Payload bytes the cell is to carry, rounded as a take rounds them.
 * @param[in,out] owner     The owner a persistent cell's growth is counted to [BORROWS]. A temporary
 *                          cell's growth is counted to the owner its trailing word names, whatever is
 *                          passed.
 * @return                  The cell's first byte after the growth: the prisoner itself for a persistent
 *                          cell, or for one that carries size already; a lower address for a temporary
 *                          cell, whose bytes moved down. NULL with nothing changed where the cell is not
 *                          the newest of its tier, a cell having been taken above it since, or where
 *                          the gap cannot meet the growth. A NULL prisoner or a NULL owner returns NULL
 *                          [RETURNS OWNERSHIP].
 * @note A temporary cell keeps the lifetime its take gave it: the growth moves no end of it.
 * @note The newest cell of a tier is the cell its boundary stands against: the persistent end at the
 *       cell's end, the temporary top at the cell's word. The boundary moves by one swap on the tiers
 *       word, by the growth alone, and no other cell moves. A caller whose growth is refused takes a
 *       larger cell, copies into it and releases the first, as it would without this entry.
 * @note The added bytes are as they lie: a persistent growth's hold the word that stood behind the
 *       payload, a temporary growth's the cell's own old word and payload. mmgr_max_security_buf_grow
 *       zeroes them.
 * @warning The prisoner's address is dead after a temporary growth that answers a lower one: the cell
 *          is reached at the address answered from then on. A prisoner outside this cellblock's own
 *          bytes stops with MMGR_FATAL, as a release does.
 */
MMGR_ALLOC_SIZE(3) void *mmgr_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner);

/**
 * @brief Grows a cell in place as mmgr_buf_grow does, and zeroes the bytes the growth adds.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner  First byte of the cell [BORROWS].
 * @param[in]     size      Payload bytes the cell is to carry.
 * @param[in,out] owner     The owner a persistent cell's growth is counted to [BORROWS].
 * @return                  As mmgr_buf_grow answers [RETURNS OWNERSHIP].
 * @note The zeroing covers the growth's bytes alone: the payload the cell carried keeps its bytes, moved
 *       for a temporary cell.
 */
MMGR_ALLOC_SIZE(3)
void *mmgr_max_security_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner);

/**
 * @brief Takes size bytes from a cellblock's temporary tier.
 *
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size        Bytes wanted.
 * @param[in,out] owner       The registered owner the cell is counted to [BORROWS].
 * @param[in]     lifetime_ms Milliseconds the cell is the holder's, on mmgr_carcer_hw_clock_ms, from 1
 *                            up to 2^31 - 1.
 * @return                    Start of the cell, or NULL when owner is NULL or not registered, the
 *                            lifetime is out of its range, or the cellblock cannot meet the take
 *                            [RETURNS OWNERSHIP].
 * @note The caller gives the cell its lifetime, and gives the cell back itself within it, by mark
 *       through mmgr_temporary_buf_release or mmgr_temporary_buf_reset, or by a single give. The
 *       give or the rewind takes the bytes off owner. The cell carries owner in the word behind its
 *       payload and the end of its lifetime in the word behind that.
 * @note The end of a lifetime is a backstop. Every take first gives back the cells at the top whose
 *       lifetimes have ended, one after another while the top reads so, as their holders' give would,
 *       and calls mmgr_carcer_hw_expired for each. A cell below a cell still in its lifetime waits.
 * @note The take is one swap on the tiers word, which moves the top down by the cell. A rewind landing
 *       between that swap and the cell's words drops the cell, and the take then answers NULL with
 *       nothing counted.
 * @note A single release through mmgr_persistent_buf_release works here too, but trims the top only
 *       when the freed cell sits at it. Anything freed below stays in the tier, free, until a rewind
 *       or the backstop reaches it from the top.
 * @warning A cell is not the holder's past its lifetime: the backstop gives it back and the cellblock
 *          may hand its bytes out again. A holder that reads or writes it after is reading or writing
 *          another holder's cell.
 * @warning The bytes are not zeroed, and an allocation returns whatever the last prisoner left. A
 *          cellblock declared MMGR_MAXIMUM_SECURITY zeroes, and it zeroes on release.
 */
MMGR_ALLOC_SIZE(2)
void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner, uint32_t lifetime_ms);

/**
 * @brief Takes size bytes from a maximum security cellblock's temporary tier.
 *
 * @param[in,out] cellblock   Cellblock to take from [BORROWS].
 * @param[in]     size        Bytes wanted.
 * @param[in,out] owner       The registered owner the cell is counted to [BORROWS].
 * @param[in]     lifetime_ms Milliseconds the cell is the holder's, from 1 up to 2^31 - 1.
 * @return                    As mmgr_temporary_buf_alloc answers [RETURNS OWNERSHIP].
 * @note The take mmgr_temporary_buf_alloc makes, but for the backstop: each cell past its lifetime is
 *       zeroed before it is given back, as a maximum security give zeroes it.
 */
MMGR_ALLOC_SIZE(2)
void *mmgr_temporary_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner,
                                            uint32_t lifetime_ms);

/**
 * @brief The temporary tier's current top, to hand back to mmgr_temporary_buf_release.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The temporary top.
 * @note Good against this cellblock alone, and only until a restore to an older mark. The restore
 *       refuses a mark below the top as it stands and assigns any other.
 * @warning The value is a snapshot. An allocation from another context lowers the top after this has
 *          read it, and restoring the mark then releases that allocation's bytes as well.
 */
size_t mmgr_temporary_buf_mark(const CarcerCellBlock *cellblock);

/**
 * @brief Restores the temporary top a mark reported, zeroing nothing.
 *
 * @param[in,out] cellblock Cellblock to rewind [BORROWS].
 * @param[in]     mark      Top to restore, as mmgr_temporary_buf_mark reported it.
 * @note Every cell from the top to mark is dropped first: its word is swapped from busy or free to
 *       dead, and its payload bytes come off the owner its trailing word names. Then the top moves to
 *       mark by one swap on the tiers word. Where a take lowered the top meanwhile, its cells are
 *       dropped too and the swap runs again. A mark below the top as it stands is refused, since it
 *       would lower the tier onto bytes an earlier restore already released, and a mark past the
 *       cellblock's size is refused too.
 * @warning A mark this cellblock never reported, above the top and inside the size, is taken as given
 *          and moves the tier there.
 * @warning Every temporary cell taken since mark is dead once this returns. Nothing is zeroed: such a
 *          pointer still dereferences and reads whatever the next allocation puts there.
 */
void mmgr_temporary_buf_release(CarcerCellBlock *cellblock, size_t mark);

/**
 * @brief Zeroes every temporary byte taken since mark, then restores the top.
 *
 * @param[in,out] cellblock Cellblock to rewind [BORROWS].
 * @param[in]     mark      Top to restore, as mmgr_temporary_buf_mark reported it.
 * @note The order is the point. The temporary tier grows down: the live bytes are [top, mark) and
 *       reclaiming means raising the top. The cells are dropped off their owners first, as
 *       mmgr_temporary_buf_release drops them, then zeroed, then the top moves: the bytes are already
 *       zero at the instant they become available. Where a take lowered the top meanwhile, its cells
 *       are dropped and zeroed too and the swap runs again.
 * @warning Every temporary cell taken since mark is dead once this returns.
 * @warning A mark not above the top as it stands, or past the cellblock's size, is refused: nothing is
 *          zeroed and the top stays.
 */
void mmgr_max_security_buf_return(CarcerCellBlock *cellblock, size_t mark);

/**
 * @brief Releases the whole temporary tier at once, zeroing nothing.
 *
 * @param[in,out] cellblock Cellblock to reset [BORROWS].
 * @note Restores the top to the cellblock's own size, which is where the tier starts.
 * @note No cellblock entry reaches this one. A site's reset is its own generated wrapper, and that
 *       goes to the zeroing rewind wherever the cellblock was declared MMGR_MAXIMUM_SECURITY.
 * @warning Every temporary cell the cellblock has handed out is dead once this returns, and none of
 *          those bytes are zeroed.
 */
void mmgr_temporary_buf_reset(CarcerCellBlock *cellblock);

/**
 * @brief Whether at lies inside the cellblock's bytes.
 *
 * @param[in] cellblock Cellblock to test against [BORROWS].
 * @param[in] at        Address to test [BORROWS].
 * @return              EMBED_TRUE when at lies in the cellblock's storage, which is [base, base + size).
 * @note One unsigned compare covers both ends, since an address below base wraps to a difference
 *       larger than any size.
 * @warning Any address in the cellblock answers true, the first byte of a cell or any other. This says
 *          where an address is and nothing of what is there. A true answer is not a warrant that at
 *          may be released. The releases read a cell's word from whatever address they are handed.
 */
embed_bool mmgr_who_owns_buf(const CarcerCellBlock *cellblock, const void *at);

/**
 * @brief The bytes lying between the two tiers.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The temporary top less the persistent end, read off one word.
 * @note An allocation out of that gap needs a cell header from the same bytes, and the request is
 *       rounded up to a whole word first. A request of exactly this many bytes cannot be met.
 * @warning The answer is a snapshot. Any allocation at all leaves it stale before the caller can act
 *          on it.
 */
size_t mmgr_buf_available(const CarcerCellBlock *cellblock);

/**
 * @brief Zeroes size bytes at prisoner, releasing nothing.
 *
 * @param[in,out] prisoner First byte to clear [BORROWS].
 * @param[in]     size     Bytes to clear.
 * @note The stores are volatile: clearing bytes nothing reads afterwards is not dropped as dead
 *       work. The middle is stored a word at a time, the two edges a byte at a time.
 * @note The extent is the caller's to state. A zeroing release takes it from the cell's own word.
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
 *       request of 0 up to one word first: no cell is handed out empty.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask. A size within a word of SIZE_MAX wraps
 *          to 0.
 */
size_t mmgr_align_up_buf(size_t size);

/**
 * @brief Puts a cellblock back to the state it was declared in: both tiers empty, every free cell the
 *        persistent tier holds marked dead and no other byte of its storage touched.
 *
 * @param[in,out] cellblock Cellblock to empty [BORROWS].
 * @note Every cell the cellblock has handed out is dead once this returns. A suite calls it between
 *       cases over one declared cellblock; a program with a cellblock to empty while another context
 *       reaches it has a cellblock whose cells that context still holds.
 * @note The free cells are marked dead because bytes outside the tiers must never read as a free
 *       cell: a give that trims the cell above them reads the cell below through the word behind it,
 *       and trims it only where it reads free.
 */
void mmgr_cellblock_emptied(CarcerCellBlock *cellblock);

#if MMGR_ENABLE_HOST
/**
 * @brief Declares a prison site of one cellblock over a block the host's allocator hands out as the
 *        site is made, and takes back whole as it is freed.
 *
 * @param[in] prisonsite_ Name of the site. Its cellblock is reached as a member of it.
 * @param[in] ...         One MMGR_MINIMUM_SECURITY or MMGR_MAXIMUM_SECURITY declaration, whose name is
 *                        the member's; it names no pool, the block being the pool.
 * @note prisonsite_##_made(bytes) takes a block for the bytes asked through calloc, which hands it out
 *       zero, lays the cellblock's record at the block's start and the cell bytes after it on an
 *       MMGR_ALIGN_BYTES boundary, and frees the block kept before; prisonsite_##_freed() frees the
 *       block. A site made anew holds nothing from before: there is nothing to collect from it.
 * @note The entries read the record through one pointer. A site made anew serves the same calls,
 *       and prisonsite_##_cellblock() reads the record, NULL where no block is kept.
 * @warning Every entry reads the pointer: a site is made before its first take, and the block of a
 *          site freed holds no cell anyone still reads.
 */
#define LocusCarcerumHospes(prisonsite_, ...) MMGR_CARCER_APPLY(MMGR_CARCER_HOST, prisonsite_, __VA_ARGS__)

/**
 * @brief The host site's cellblock: its record pointer, its entries bound through that pointer, the
 *        warden, and the calls that make and free the block.
 */
#define MMGR_CARCER_HOST(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    static CarcerCellBlock *prisonsite_##_##name_##_ctx;                                                               \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_persistent_buf_alloc(size_t size, CellOwner *owner)        \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_persistent_max_security_buf_alloc(prisonsite_##_##name_##_ctx, size, owner);                   \
        }                                                                                                              \
        return mmgr_persistent_buf_alloc(prisonsite_##_##name_##_ctx, size, owner);                                    \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_persistent_buf_release(void *prisoner, CellOwner *owner)                       \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_persistent_max_security_buf_release(prisonsite_##_##name_##_ctx, prisoner, owner);                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_persistent_buf_release(prisonsite_##_##name_##_ctx, prisoner, owner);                                 \
        }                                                                                                              \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(1)                                                                                                 \
    static void *prisonsite_##_##name_##_temporary_buf_alloc(size_t size, CellOwner *owner, uint32_t lifetime_ms)      \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_temporary_max_security_buf_alloc(prisonsite_##_##name_##_ctx, size, owner, lifetime_ms);       \
        }                                                                                                              \
        return mmgr_temporary_buf_alloc(prisonsite_##_##name_##_ctx, size, owner, lifetime_ms);                        \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_temporary_buf_mark(void)                                                     \
    {                                                                                                                  \
        const size_t mark = mmgr_temporary_buf_mark(prisonsite_##_##name_##_ctx);                                      \
        return mark;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_release(size_t mark)                                             \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_max_security_buf_return(prisonsite_##_##name_##_ctx, mark);                                           \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_temporary_buf_release(prisonsite_##_##name_##_ctx, mark);                                             \
        }                                                                                                              \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_reset(void)                                                      \
    {                                                                                                                  \
        prisonsite_##_##name_##_temporary_buf_release(prisonsite_##_##name_##_ctx->size);                              \
    }                                                                                                                  \
    static embed_bool prisonsite_##_##name_##_who_owns_buf(const void *at)                                             \
    {                                                                                                                  \
        return mmgr_who_owns_buf(prisonsite_##_##name_##_ctx, at);                                                     \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_buf_available(void)                                                          \
    {                                                                                                                  \
        const size_t available = mmgr_buf_available(prisonsite_##_##name_##_ctx);                                      \
        return available;                                                                                              \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(2) static void *prisonsite_##_##name_##_buf_grow(void *prisoner, size_t size, CellOwner *owner)    \
    {                                                                                                                  \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            return mmgr_max_security_buf_grow(prisonsite_##_##name_##_ctx, prisoner, size, owner);                     \
        }                                                                                                              \
        return mmgr_buf_grow(prisonsite_##_##name_##_ctx, prisoner, size, owner);                                      \
    }                                                                                                                  \
    EMBED_TABLE_STORAGE struct                                                                                         \
    {                                                                                                                  \
        type_ name_;                                                                                                   \
    } prisonsite_ EMBED_UNUSED = {MMGR_CARCER_SEAT(prisonsite_, type_, name_, wipe_to_zero_on_release_)};              \
    static embed_bool prisonsite_##_made(size_t bytes)                                                                 \
    {                                                                                                                  \
        const embed_bool made = mmgr_host_cellblock_made(&prisonsite_##_##name_##_ctx, bytes);                         \
        return made;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_freed(void)                                                                              \
    {                                                                                                                  \
        mmgr_host_cellblock_freed(&prisonsite_##_##name_##_ctx);                                                       \
    }                                                                                                                  \
    static const CarcerCellBlock *prisonsite_##_cellblock(void)                                                        \
    {                                                                                                                  \
        return prisonsite_##_##name_##_ctx;                                                                            \
    }

/**
 * @brief Makes a cellblock over a block of size bytes the host hands out zero, in the place of the one
 *        kept, which is freed.
 *
 * @param[in,out] kept  Where the cellblock's record is kept: the one before freed, the one made put
 *                      there [BORROWS].
 * @param[in]     bytes Cell bytes the block holds.
 * @return              1 where the block was handed out, 0 where the host gave none, bytes holds no
 *                      cell, or bytes is past MMGR_CARCER_BYTES_MOST.
 * @note The record is laid into the block's first bytes, which calloc left zero, and the cells start
 *       after it on an MMGR_ALIGN_BYTES boundary: the whole block is one allocation.
 */
embed_bool mmgr_host_cellblock_made(CarcerCellBlock **kept, size_t bytes);

/**
 * @brief Frees the block a cellblock stands over, record and cells together, and holds none.
 *
 * @param[in,out] kept Where the record is kept, NULL after [BORROWS].
 */
void mmgr_host_cellblock_freed(CarcerCellBlock **kept);

/**
 * @brief Whether a cellblock stands as the host handed it out: no cell taken from either tier and
 *        every byte of it zero.
 *
 * @param[in] cellblock Record to read, NULL reads as not [BORROWS].
 * @return              1 where it does, 0 where it does not.
 * @note Reads every byte: a check for a test to make, never a step of a take.
 */
embed_bool mmgr_host_cellblock_zero(const CarcerCellBlock *cellblock);
#endif

EMBED_END_DECLS

#endif
