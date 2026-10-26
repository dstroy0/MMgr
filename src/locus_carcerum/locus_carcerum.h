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
 *       The linker resolves it, so no call computes a cellblock's address from a site and an index,
 *       and which memory a cellblock sits in was settled where its pool was declared.
 * @note A cellblock has two tiers. The persistent tier runs up from base and the temporary tier runs
 *       down from size, and a CarcerTier names the extent of one of them.
 * @note A cellblock's declaration picks its security level. MMGR_MINIMUM_SECURITY leaves a cell's
 *       bytes as they are on release. MMGR_MAXIMUM_SECURITY zeroes them. The warden is const, so
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
 *       raises them in carcer_grow, which both tiers reach. One mark per tier, so neither counts the
 *       other's bytes.
 * @note The #ifndef leaves a build's own definition standing, whether it arrives on the command line
 *       or from a header included ahead of this one.
 * @note Takes 0 or 1 and nothing else. Both readers are #if, which would take any non-zero as set, so
 *       the check below is what keeps a mistyped value from quietly widening every CarcerCellBlock.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif
#if (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 0) && (MMGR_ENABLE_HW_MEM_CAPACITY_CB != 1)
#error "MMGR_ENABLE_HW_MEM_CAPACITY_CB must be 0 or 1"
#endif

/**
 * @brief Set to 1 to have every custodia take its cellblock's spinlock around each call.
 *
 * @note Off by default. A cellblock reached from one context needs no lock and pays nothing for it.
 * @note On, each cellblock carries an embed_spinlock (embed_spinlock.h) beside its state, and every
 *       entry of its guard but who_owns_buf takes it for the call and frees it after. The lock spins
 *       and never sleeps, so the same build serves a host thread, a part's task and a GPU thread.
 * @note who_owns_buf takes no lock. It compares an address against a base and a size that do not
 *       change after the declaration.
 * @note Takes 0 or 1 and nothing else, for the reason MMGR_ENABLE_HW_MEM_CAPACITY_CB gives.
 */
#ifndef MMGR_ENABLE_LOCK
#define MMGR_ENABLE_LOCK 0
#endif
#if (MMGR_ENABLE_LOCK != 0) && (MMGR_ENABLE_LOCK != 1)
#error "MMGR_ENABLE_LOCK must be 0 or 1"
#endif

/**
 * @brief Set to 1 to have each cellblock list the owners that take cells from it.
 *
 * @note Off by default because the list costs MMGR_CARCER_OWNERS pointers and a count in every
 *       CarcerCellBlock, and a build that never reads it should not carry it.
 * @note An owner's own counts, its bytes in use and its peak, are kept in its CellOwner either way.
 *       This adds the cellblock's list of every owner that took a cell, in the order of each one's
 *       first take, so a reader finds them all from the cellblock.
 * @note Takes 0 or 1 and nothing else, for the reason MMGR_ENABLE_HW_MEM_CAPACITY_CB gives.
 */
#ifndef MMGR_ENABLE_OWNERS
#define MMGR_ENABLE_OWNERS 0
#endif
#if (MMGR_ENABLE_OWNERS != 0) && (MMGR_ENABLE_OWNERS != 1)
#error "MMGR_ENABLE_OWNERS must be 0 or 1"
#endif

/**
 * @brief The most owners one cellblock's list carries.
 *
 * @note Read only where MMGR_ENABLE_OWNERS is 1. An owner past the list's room still has its own
 *       counts kept in its CellOwner; it is only absent from the list.
 */
#ifndef MMGR_CARCER_OWNERS
#define MMGR_CARCER_OWNERS 128u
#endif

#if MMGR_ENABLE_LOCK
#include "embed_spinlock.h"
#endif

/**
 * @brief Alignment every prisoner is handed out at, which is one machine word.
 *
 * @note Derived from the word rather than named as a number. A build at another width gets the
 *       alignment that width actually needs.
 * @note LocusCarcerum() puts this alignment on the storage it declares, so what this rounds is only
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
 * @brief One owner of persistent cells: its name, the payload bytes of the cells it has now, and the
 *        most it had at once.
 *
 * @note The caller's storage, named on every persistent take and give. A take adds the cell's payload
 *       bytes to in_use and raises hw to it; a give takes the same bytes off. NULL in place of an
 *       owner counts nothing.
 * @note The bytes counted are the cell's own, read off its header: a reused cell larger than the
 *       request counts its whole payload on the take and the give alike, so a matched pair returns
 *       in_use to where it stood.
 * @note listed_in is the cellblock whose owner list carries this owner, NULL until one does. An owner
 *       is listed by the first cellblock it takes from, where MMGR_ENABLE_OWNERS is 1.
 * @warning Counts the persistent tier alone. A temporary cell given back through the persistent
 *          release with an owner named takes bytes off that owner which no take added.
 */
typedef struct
{
    const char *name;      /**< The owner's name, as a reader prints it [BORROWS]. */
    size_t in_use;         /**< Payload bytes of the cells the owner has now. */
    size_t hw;             /**< Running maximum of in_use. */
    const void *listed_in; /**< The cellblock whose owner list carries this owner, NULL until listed. */
} CellOwner;

/**
 * @brief An owner with nothing counted yet, named name_.
 *
 * @param[in] name_ The owner's name [BORROWS].
 */
#define MMGR_CELL_OWNER(name_) {(name_), 0u, 0u, NULL}

/**
 * @brief One cellblock's state: its bytes and the two tiers that grow toward each other.
 *
 * @note Written by the declaration that emits the cellblock and by the entries that cellblock owns.
 *       The base is the cellblock's own storage, so nothing derives an address from a site and an
 *       index.
 * @note persistent_end bounds that tier rather than counting what is in use. A freed cell inside the
 *       tier stays in it until a release trims the end.
 */
typedef struct
{
    uint8_t *const base;   /**< First byte of the cellblock [BORROWS]. */
    const size_t size;     /**< Bytes in the cellblock. */
    size_t persistent_end; /**< Offset just past the last persistent cell, counting up from base. */
    size_t temporary_top;  /**< Offset of the lowest temporary byte, counting down from size. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    size_t persistent_hw; /**< Running maximum of persistent_end. */
    size_t temporary_hw;  /**< Running maximum of the bytes taken from the top. */
#endif
#if MMGR_ENABLE_OWNERS
    CellOwner *owners[MMGR_CARCER_OWNERS]; /**< Every owner that took a cell, in the order of its first take. */
    size_t owner_count;                    /**< Owners in the list, at most MMGR_CARCER_OWNERS. */
#endif
} CarcerCellBlock;

/**
 * @brief The minimum security guard over one cellblock: a released cell keeps whatever is in it.
 *
 * @note EMBED_TABLE_LAYOUT asserts the eight members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 * @note Every entry is bound to the cellblock its declaration named, so no call carries a cellblock
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
    void *(*temporary_buf_alloc)(size_t size);  /**< Takes size bytes from the top, unzeroed [RETURNS OWNERSHIP]. */
    size_t (*temporary_buf_mark)(void);         /**< The current top, for temporary_buf_release. */
    void (*temporary_buf_release)(size_t mark); /**< Restores the top a mark reported, zeroing nothing. */
    void (*temporary_buf_reset)(void);          /**< Releases the whole temporary tier at once. */
    embed_bool (*who_owns_buf)(const void *at); /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);              /**< Bytes between the two tiers. */
} MinimumSecurityGuard;
EMBED_TABLE_LAYOUT(MinimumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   temporary_buf_mark, temporary_buf_release, temporary_buf_reset, who_owns_buf, buf_available);

/**
 * @brief The maximum security guard over one cellblock: every release zeroes the cell first.
 *
 * @note EMBED_TABLE_LAYOUT asserts the eight members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 * @note The same eight entries, and no unzeroed release among them. Each zeroing runs before the
 *       boundary moves, so the bytes are zero at the instant they become available. Where the
 *       zeroing is skipped and the boundary still moves, mmgr_max_security_buf_return carries it in
 *       its warnings.
 */
typedef struct
{
    void *(*persistent_buf_alloc)(
        size_t size, CellOwner *owner); /**< Takes size bytes from the bottom for owner [RETURNS OWNERSHIP]. */
    void (*persistent_buf_release)(void *prisoner,
                                   CellOwner *owner); /**< Zeroes owner's cell, then releases it [TAKES OWNERSHIP]. */
    void *(*temporary_buf_alloc)(size_t size);        /**< Takes size bytes from the top [RETURNS OWNERSHIP]. */
    size_t (*temporary_buf_mark)(void);               /**< The current top, for temporary_buf_release. */
    void (*temporary_buf_release)(size_t mark);       /**< Zeroes back to the mark, then restores the top. */
    void (*temporary_buf_reset)(void);                /**< Zeroes the whole temporary tier, then releases it. */
    embed_bool (*who_owns_buf)(const void *at);       /**< Whether at lies in this cellblock's bytes [BORROWS]. */
    size_t (*buf_available)(void);                    /**< Bytes between the two tiers. */
} MaximumSecurityGuard;
EMBED_TABLE_LAYOUT(MaximumSecurityGuard, persistent_buf_alloc, persistent_buf_release, temporary_buf_alloc,
                   temporary_buf_mark, temporary_buf_release, temporary_buf_reset, who_owns_buf, buf_available);

#if MMGR_ENABLE_LOCK
/**
 * @brief Declares a cellblock's spinlock, free as it is declared.
 *
 * @param[in] lock_ The lock's name, the site's and the cellblock's spliced together.
 * @note Static beside the cellblock's state and outside it, so a host site made anew in the place
 *       of the block before keeps the lock its callers spin on.
 */
#define MMGR_CARCER_LOCK(lock_) static embed_spinlock lock_ = EMBED_SPINLOCK_INIT;

/**
 * @brief Takes a cellblock's spinlock, spinning until it is free.
 *
 * @param[in] lock_ The lock MMGR_CARCER_LOCK declared.
 */
#define MMGR_CARCER_ACQUIRE(lock_) EMBED_SPINLOCK_ACQUIRE(&(lock_))

/**
 * @brief Frees a cellblock's spinlock.
 *
 * @param[in] lock_ The lock MMGR_CARCER_LOCK declared.
 */
#define MMGR_CARCER_RELEASE(lock_) EMBED_SPINLOCK_RELEASE(&(lock_))
#else
/** @brief Declares nothing where MMGR_ENABLE_LOCK is 0. */
#define MMGR_CARCER_LOCK(lock_)

/** @brief Takes nothing where MMGR_ENABLE_LOCK is 0. */
#define MMGR_CARCER_ACQUIRE(lock_) ((void)0)

/** @brief Frees nothing where MMGR_ENABLE_LOCK is 0. */
#define MMGR_CARCER_RELEASE(lock_) ((void)0)
#endif

/**
 * @brief Everything one cellblock is: its bytes, its state, and the entries bound to it.
 *
 * @param[in] prisonsite_ Site the cellblock is built at, which every emitted symbol carries.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, read by MMGR_CARCER_MEM.
 * @param[in] name_       Pool the cellblock is built over, which is also the name it is reached by
 *                        as a member of its site [BORROWS].
 * @param[in] wipe_to_zero_on_release_ 1 where a release zeroes the cell first, 0 where it does not.
 * @note The site name is part of every symbol, so two sites may each hold a cellblock of the same
 *       name, at different security levels, without colliding.
 * @note wipe_to_zero_on_release_ is a literal, so the branch it guards folds away and the security
 *       level costs nothing to read.
 * @note The extent comes from the pool two ways. name_##_bytes is the count its declaration was
 *       handed and is what the record carries, and sizeof is what the compiler laid down. The
 *       asserts test the second, so they say something the first cannot say about itself.
 * @warning A pool too small to hold one cell fails the build. Only the alignment is a power of two,
 *          and nothing reads the size as a mask. A bad size is the one mistake the language would
 *          otherwise accept. A wrong name or guard type fails on its own, and two pools are separate
 *          objects, so they cannot share an address and there is no overlap to check.
 * @note No upper bound. The pool states how much storage the cellblock gets, and nothing in the
 *       library sizes anything from a ceiling over it.
 */
#define MMGR_CARCER_BODY(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    MMGR_PARS_CLAIMED_ONCE(name_);                                                                                     \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) >= (2u * MMGR_CARCER_ALIGN),                                 \
                        #prisonsite_ "." #name_ " is too small for one cell");                                         \
    static CarcerCellBlock prisonsite_##_##name_##_ctx = {mmgr_pars_storage_##name_, name_##_bytes, 0u,                \
                                                          name_##_bytes};                                              \
    MMGR_CARCER_LOCK(prisonsite_##_##name_##_lock)                                                                     \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_persistent_buf_alloc(size_t size, CellOwner *owner)        \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        void *const prisoner = mmgr_persistent_buf_alloc(&prisonsite_##_##name_##_ctx, size, owner);                   \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return prisoner;                                                                                               \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_persistent_buf_release(void *prisoner, CellOwner *owner)                       \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_persistent_max_security_buf_release(&prisonsite_##_##name_##_ctx, prisoner, owner);                   \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_persistent_buf_release(&prisonsite_##_##name_##_ctx, prisoner, owner);                                \
        }                                                                                                              \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_temporary_buf_alloc(size_t size)                           \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        void *const prisoner = mmgr_temporary_buf_alloc(&prisonsite_##_##name_##_ctx, size);                           \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return prisoner;                                                                                               \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_temporary_buf_mark(void)                                                     \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        const size_t mark = mmgr_temporary_buf_mark(&prisonsite_##_##name_##_ctx);                                     \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return mark;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_release(size_t mark)                                             \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_max_security_buf_return(&prisonsite_##_##name_##_ctx, mark);                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_temporary_buf_release(&prisonsite_##_##name_##_ctx, mark);                                            \
        }                                                                                                              \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
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
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        const size_t available = mmgr_buf_available(&prisonsite_##_##name_##_ctx);                                     \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return available;                                                                                              \
    }

/**
 * @brief One cellblock as a member of its site's type.
 *
 * @param[in] prisonsite_ Site the cellblock is built at, unused here.
 * @param[in] type_       MinimumSecurityGuard or MaximumSecurityGuard, the member's type.
 * @param[in] name_       Pool the cellblock is built over, which is the name the member is given.
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
 * @note MinimumSecurityGuard and MaximumSecurityGuard declare the same eight members in the same
 *       order, so one initializer serves either level.
 */
#define MMGR_CARCER_SEAT(prisonsite_, type_, name_, wipe_to_zero_on_release_)                                          \
    {prisonsite_##_##name_##_persistent_buf_alloc,  prisonsite_##_##name_##_persistent_buf_release,                    \
     prisonsite_##_##name_##_temporary_buf_alloc,   prisonsite_##_##name_##_temporary_buf_mark,                        \
     prisonsite_##_##name_##_temporary_buf_release, prisonsite_##_##name_##_temporary_buf_reset,                       \
     prisonsite_##_##name_##_who_owns_buf,          prisonsite_##_##name_##_buf_available},

/**
 * @brief Declares a minimum security cellblock over a pool.
 *
 * @param[in] name_ Pool the cellblock is built over, which is also the name it is reached by as a
 *                  member of its site [BORROWS].
 * @note The pool carries its own extent, so no size is written here. A cellblock is a dressing over
 *       bytes that already exist, and where those bytes live was settled by which of
 *       ParsMemoriaeInternae or ParsMemoriaeExternum declared them.
 * @note Expands to a tuple rather than to code. The site reads the same list three times, once for
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
 * @note Written MMGR_UNTUPLE tuple_, with no parentheses of its own, so the tuple supplies them.
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
 *       is what lets the flattened tuple reach the reader as separate arguments.
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
 *       substituted, so MMGR_UNTUPLE runs here and the tuple's elements reach the reader as
 *       separate arguments rather than as one.
 */
#define MMGR_CARCER_APPLY_(what_, prisonsite_, ...) what_(prisonsite_, __VA_ARGS__)

/**
 * @brief Applies the reader to a site's one cellblock tuple.
 *
 * @param[in] what_       Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] prisonsite_ Site, which reaches the reader ahead of the tuple's own elements.
 * @param[in] CellBlock1_ The cellblock's tuple.
 * @note The base of the walk. Every longer line ends in this one, so it is the only line here that
 *       names no other.
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
 *       more. The preprocessor cannot walk a list, so each cellblock count needs a line of its own.
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
 * @note A cellblock's entries are bound to that cellblock, so no call names one and none can reach
 *       another's bytes. A minimum security cellblock has no zeroing release and a maximum security
 *       one has no plain release.
 * @warning The bytes, the state and the warden are all static. A declaration in a header gives
 *          every translation unit that includes it a site of its own rather than one they share.
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
 * @param[in,out] owner     The owner the cell is counted to, or NULL to count it to none [BORROWS].
 * @return                  Start of the cell, or NULL when the cellblock cannot meet it [RETURNS OWNERSHIP].
 * @note Reached through the cellblock's own guard rather than called by name.
 * @note The cell goes back through the same cellblock's persistent_buf_release, which takes it.
 * @note A cell handed out adds its payload bytes, read off its header, to owner's in_use and raises
 *       owner's hw to it. Where MMGR_ENABLE_OWNERS is 1, an owner no cellblock lists yet is added to
 *       this one's list while the list has room. A refused take counts nothing.
 * @warning The bytes are not zeroed, and a reused cell still carries what the last prisoner left. A
 *          cellblock declared MMGR_MAXIMUM_SECURITY is the one that zeroes, and it zeroes on release.
 */
MMGR_ALLOC_SIZE(2) void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner);

/**
 * @brief Releases a prisoner, leaving the cell's bytes as they are.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner  First byte of the cell [TAKES OWNERSHIP].
 * @param[in,out] owner     The owner the cell was counted to, or NULL where it was counted to none
 *                          [BORROWS].
 * @note Which tier the cell came from is read from its address. A release cannot be given to the
 *       wrong tier. A NULL prisoner returns without touching the cellblock.
 * @note The cell's payload bytes, read off its header, come off owner's in_use before the release.
 * @note A prisoner outside this cellblock's own bytes returns without touching it. A pointer from
 *       another cellblock cannot move this one's boundaries.
 * @warning prisoner is dead once this returns and its bytes are not zeroed. The cellblock may hand
 *          them out again.
 * @warning The bound is the cellblock's storage, not a cell boundary. An address inside these bytes
 *          that is not the first byte of a cell is still read as a header.
 */
void mmgr_persistent_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner);

/**
 * @brief Zeroes a cell, then releases the prisoner.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner  First byte of the cell, zeroed before release [TAKES OWNERSHIP].
 * @param[in,out] owner     The owner the cell was counted to, or NULL where it was counted to none
 *                          [BORROWS].
 * @note The extent comes from the cell's own header. A caller cannot under-zero a cell by naming
 *       fewer bytes than it carries. A NULL prisoner returns without touching the cellblock.
 * @note The zeroing is the only step that separates this from mmgr_persistent_buf_release, which it
 *       calls to do the release.
 * @note A prisoner outside this cellblock's own bytes returns before the zeroing. A pointer from
 *       another cellblock is neither cleared nor released.
 * @warning prisoner is dead once this returns. The cellblock may hand those bytes out again.
 * @warning The bound is the cellblock's storage, not a cell boundary. An address inside these bytes
 *          that is not the first byte of a cell still has its extent read from the bytes ahead of it,
 *          and is zeroed for whatever length those hold.
 */
void mmgr_persistent_max_security_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner);

/**
 * @brief Takes size bytes from a cellblock's temporary tier.
 *
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size      Bytes wanted.
 * @return                  Start of the cell, or NULL when the cellblock cannot meet it [RETURNS OWNERSHIP].
 * @note The bytes normally go back by mark, through mmgr_temporary_buf_release or
 *       mmgr_temporary_buf_reset.
 * @note A single release through mmgr_persistent_buf_release works here too, but trims the top only
 *       when the freed cell sits at it. Anything freed below stays in the tier, and no allocation at
 *       this tier walks it, so nothing reuses that cell before the next rewind.
 * @warning The bytes are not zeroed, and an allocation returns whatever the last prisoner left. A
 *          cellblock declared MMGR_MAXIMUM_SECURITY is the one that zeroes, and it zeroes on release.
 */
MMGR_ALLOC_SIZE(2) void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size);

/**
 * @brief The temporary tier's current top, to hand back to mmgr_temporary_buf_release.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The value of temporary_top.
 * @note Good against this cellblock alone, and only until a restore to an older mark. The restore
 *       assigns the value it is handed without testing it.
 * @warning The value is a snapshot. An allocation from a preempting handler lowers the top after
 *          this has read it, and restoring the mark then releases that allocation's bytes as well.
 */
size_t mmgr_temporary_buf_mark(const CarcerCellBlock *cellblock);

/**
 * @brief Restores the temporary top a mark reported, zeroing nothing.
 *
 * @param[in,out] cellblock Cellblock to rewind [BORROWS].
 * @param[in]     mark      Top to restore, as mmgr_temporary_buf_mark reported it.
 * @note The top is assigned, not tested, so the mark must be one this cellblock reported, and it
 *       must not lie below the current top. A mark below it lowers the tier onto bytes an earlier
 *       restore already released.
 * @warning A mark this cellblock never reported, or one past its size, is taken as given and moves
 *          the tier there.
 * @warning Every temporary cell taken since mark is dead once this returns. Nothing is zeroed, so
 *          such a pointer still dereferences and reads whatever the next allocation puts there.
 */
void mmgr_temporary_buf_release(CarcerCellBlock *cellblock, size_t mark);

/**
 * @brief Zeroes every temporary byte taken since mark, then restores the top.
 *
 * @param[in,out] cellblock Cellblock to rewind [BORROWS].
 * @param[in]     mark      Top to restore, as mmgr_temporary_buf_mark reported it.
 * @note The order is the point. The temporary tier grows down, so the live bytes are [top, mark) and
 *       reclaiming means raising the top. Zeroing first means they are already zero at the instant
 *       they become available. Reclaiming first would leave a window in which the next allocation,
 *       or a preempting handler, sees what the last prisoner left.
 * @warning Every temporary cell taken since mark is dead once this returns.
 * @warning The zeroing is skipped when mark is not above the current top, or lies past the
 *          cellblock's size, and the top is assigned either way. A mark this cellblock did not
 *          report can rewind the tier without zeroing anything.
 * @warning The top is read once, ahead of the zeroing. An allocation from a preempting handler
 *          landing between that read and the restore is dropped unzeroed, since the extent was
 *          settled from the older top.
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
 * @warning Any address in the cellblock answers true, not only the first byte of a cell. This says
 *          where an address is, not what is there. A true answer is not a warrant that at may be
 *          released. The releases read a header from whatever address they are handed.
 */
embed_bool mmgr_who_owns_buf(const CarcerCellBlock *cellblock, const void *at);

/**
 * @brief The bytes lying between the two tiers.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              temporary_top minus persistent_end, or 0 once the two tiers have met.
 * @note An allocation out of that gap needs a cell header from the same bytes, and the request is
 *       rounded up to a whole word first. A request of exactly this many bytes cannot be met.
 * @warning The two tiers are read one after the other and the answer is a snapshot. An allocation
 *          from a preempting handler landing between the reads gives a value matching neither state,
 *          and any allocation at all leaves it stale before the caller can act on it.
 */
size_t mmgr_buf_available(const CarcerCellBlock *cellblock);

/**
 * @brief Zeroes size bytes at prisoner, releasing nothing.
 *
 * @param[in,out] prisoner First byte to clear [BORROWS].
 * @param[in]     size     Bytes to clear.
 * @note The stores are volatile, so clearing bytes nothing reads afterwards is not dropped as dead
 *       work. The middle is stored a word at a time, the two edges a byte at a time.
 * @note The extent is the caller's to state. A zeroing release takes it from the cell's own header.
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
 *       request of 0 up to one word first, so no cell is handed out empty.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask. A size within a word of SIZE_MAX wraps
 *          to 0.
 */
size_t mmgr_align_up_buf(size_t size);

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
 *       MMGR_ALIGN_BYTES boundary, and frees the block held before; prisonsite_##_freed() frees the
 *       block. A site made anew holds nothing from before, so there is nothing to collect from it.
 * @note The entries read the record through one pointer, so a site made anew serves the same calls,
 *       and prisonsite_##_cellblock() reads the record, NULL where no block is held.
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
    MMGR_CARCER_LOCK(prisonsite_##_##name_##_lock)                                                                     \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_persistent_buf_alloc(size_t size, CellOwner *owner)        \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        void *const prisoner = mmgr_persistent_buf_alloc(prisonsite_##_##name_##_ctx, size, owner);                    \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return prisoner;                                                                                               \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_persistent_buf_release(void *prisoner, CellOwner *owner)                       \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_persistent_max_security_buf_release(prisonsite_##_##name_##_ctx, prisoner, owner);                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_persistent_buf_release(prisonsite_##_##name_##_ctx, prisoner, owner);                                 \
        }                                                                                                              \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
    }                                                                                                                  \
    MMGR_ALLOC_SIZE(1) static void *prisonsite_##_##name_##_temporary_buf_alloc(size_t size)                           \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        void *const prisoner = mmgr_temporary_buf_alloc(prisonsite_##_##name_##_ctx, size);                            \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return prisoner;                                                                                               \
    }                                                                                                                  \
    static size_t prisonsite_##_##name_##_temporary_buf_mark(void)                                                     \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        const size_t mark = mmgr_temporary_buf_mark(prisonsite_##_##name_##_ctx);                                      \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return mark;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_##name_##_temporary_buf_release(size_t mark)                                             \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        if (wipe_to_zero_on_release_)                                                                                  \
        {                                                                                                              \
            mmgr_max_security_buf_return(prisonsite_##_##name_##_ctx, mark);                                           \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_temporary_buf_release(prisonsite_##_##name_##_ctx, mark);                                             \
        }                                                                                                              \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
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
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        const size_t available = mmgr_buf_available(prisonsite_##_##name_##_ctx);                                      \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return available;                                                                                              \
    }                                                                                                                  \
    EMBED_TABLE_STORAGE struct                                                                                         \
    {                                                                                                                  \
        type_ name_;                                                                                                   \
    } prisonsite_ EMBED_UNUSED = {MMGR_CARCER_SEAT(prisonsite_, type_, name_, wipe_to_zero_on_release_)};              \
    static embed_bool prisonsite_##_made(size_t bytes)                                                                 \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        const embed_bool made = mmgr_host_cellblock_made(&prisonsite_##_##name_##_ctx, bytes);                         \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
        return made;                                                                                                   \
    }                                                                                                                  \
    static void prisonsite_##_freed(void)                                                                              \
    {                                                                                                                  \
        MMGR_CARCER_ACQUIRE(prisonsite_##_##name_##_lock);                                                             \
        mmgr_host_cellblock_freed(&prisonsite_##_##name_##_ctx);                                                       \
        MMGR_CARCER_RELEASE(prisonsite_##_##name_##_lock);                                                             \
    }                                                                                                                  \
    static const CarcerCellBlock *prisonsite_##_cellblock(void)                                                        \
    {                                                                                                                  \
        return prisonsite_##_##name_##_ctx;                                                                            \
    }

/**
 * @brief Makes a cellblock over a block of size bytes the host hands out zero, in the place of the one
 *        held, which is freed.
 *
 * @param[in,out] held  Where the cellblock's record is held: the one before freed, the one made put
 *                      there [BORROWS].
 * @param[in]     bytes Cell bytes the block holds.
 * @return              1 where the block was handed out, 0 where the host gave none or bytes holds no
 *                      cell.
 * @note The record is laid into the block's first bytes, which calloc left zero, and the cells start
 *       after it on an MMGR_ALIGN_BYTES boundary: the whole block is one allocation.
 */
embed_bool mmgr_host_cellblock_made(CarcerCellBlock **held, size_t bytes);

/**
 * @brief Frees the block a cellblock stands over, record and cells together, and holds none.
 *
 * @param[in,out] held Where the record is held, NULL after [BORROWS].
 */
void mmgr_host_cellblock_freed(CarcerCellBlock **held);

/**
 * @brief Whether a cellblock stands as the host handed it out: no cell taken from either tier and
 *        every byte of it zero.
 *
 * @param[in] cellblock Record to read, NULL reads as not [BORROWS].
 * @return              1 where it does, 0 where it does not.
 * @note Walks every byte, so it is a check for a test to make, not a step of a take.
 */
embed_bool mmgr_host_cellblock_zero(const CarcerCellBlock *cellblock);
#endif

EMBED_END_DECLS

#endif
