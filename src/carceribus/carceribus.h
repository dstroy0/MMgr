/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file carceribus.h
 * @brief The prison: a caller's storage divided into pools, and the table that works them.
 *
 * @note One declaration does everything. Carceribus() emits the bytes, their alignment, what each
 *       pool is, and the region record, all as initialized data - nothing runs at startup and a
 *       configuration that does not add up fails the build.
 * @note Each pool gets its own aligned array, named after the region and the pool, and its CarcerCtx
 *       holds that array's address. The linker resolves it, so no call computes a pool's address
 *       from a region and an index.
 * @note Each pool is held under one of two watches, fixed at the declaration: MMGR_SOLUTA leaves
 *       bytes as they are, MMGR_SECURA zeroes them on release. Everything in the region is const,
 *       so a pool cannot change watch after the intent was stated.
 * @note Spans over those bytes are spatium's.
 */
#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Set to 1 to track each end's high-water figure.
 *
 * @note Adds CarcerCtx::persist_hw and CarcerCtx::interim_hw, and the blend that raises them in
 *       carcer_grow, which both takes reach. One figure per end, so neither is a maximum over the other.
 * @note The #ifndef leaves a build's own definition standing, whether it arrives on the command line
 *       or from a header included ahead of this one.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif

/**
 * @brief Alignment every tenancy is handed out at, which is one machine word.
 *
 * @note Derived from the word rather than named as a number, so a build at another width gets the
 *       alignment that width actually needs.
 * @note Carceribus() puts this alignment on the storage it declares, so what this rounds is only
 *       the running offsets inside a pool.
 */
#define MMGR_CARCER_ALIGN ((size_t)sizeof(mmgr_word))

/**
 * @brief Asserts MMGR_CARCER_ALIGN is a power of two.
 *
 * @note An offset is rounded by masking off its low bits, which lands on a multiple only for a power
 *       of two. carcer_round and CARCER_HDR in carceribus.c both round that way.
 */
MMGR_STATIC_ASSERT((MMGR_CARCER_ALIGN & (MMGR_CARCER_ALIGN - 1u)) == 0u,
                   "the pool rounds offsets by masking, which needs a power of two alignment");

/**
 * @brief One pool's state: its bytes and the two ends that grow toward each other.
 *
 * @note Written by the declaration that emits the pool and by the entries that pool owns. The base
 *       is the pool's own storage, so nothing derives an address from a region and an index.
 * @note persist_end bounds the block chain rather than counting what is held: a freed block inside
 *       the chain stays in it until a release trims the end.
 */
typedef struct
{
    uint8_t *const base; /**< First byte of the pool [BORROWS]. */
    const size_t size;   /**< Bytes in the pool. */
    size_t persist_end;  /**< Offset just past the last persistent block, counting up from base. */
    size_t interim_top;  /**< Offset of the lowest interim byte, counting down from size. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    size_t persist_hw; /**< Running maximum of persist_end. */
    size_t interim_hw; /**< Running maximum of the bytes taken from the top. */
#endif
} CarcerCtx;

/**
 * @brief The loose watch over one pool: takes and releases leave the bytes as they are.
 *
 * @note MMGR_NS_LAYOUT asserts the eight members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note Every entry is bound to the pool its declaration named, so no call carries a pool argument
 *       and there is nothing to pass wrongly. A pool under this watch has no wiping release; the
 *       difference between the watches is what exists, not what a caller remembers to reach for.
 */
typedef struct
{
    void *(*persist_capio)(size_t size);  /**< Takes size bytes from the bottom, not zeroed. */
    void (*persist_reddo)(void *tenancy); /**< Gives a tenancy back, unwiped. */
    void *(*interim_capio)(size_t size);  /**< Takes size bytes from the top, not zeroed. */
    size_t (*interim_mark)(void);         /**< The current top, for interim_reddo. */
    void (*interim_reddo)(size_t mark);   /**< Restores the top a mark reported, scrubbing nothing. */
    void (*interim_reset)(void);          /**< Gives the whole interim end back at once. */
    mmgr_bool (*owns)(const void *at);    /**< Whether at lies in this pool's bytes. */
    size_t (*octas_praesto)(void);        /**< Bytes between the two ends. */
} SolutaCustodiae;
MMGR_NS_LAYOUT(SolutaCustodiae, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto);

/**
 * @brief The close watch over one pool: every release zeroes the bytes before giving them back.
 *
 * @note MMGR_NS_LAYOUT asserts the eight members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The same eight entries, and no unwiped release among them. The wipe happens before the top
 *       moves, so the bytes are already zero at the instant they become available and neither a
 *       preempting handler nor the next take can see what the last tenant left.
 */
typedef struct
{
    void *(*persist_capio)(size_t size);  /**< Takes size bytes from the bottom. */
    void (*persist_reddo)(void *tenancy); /**< Zeroes a tenancy, then gives it back. */
    void *(*interim_capio)(size_t size);  /**< Takes size bytes from the top. */
    size_t (*interim_mark)(void);         /**< The current top, for interim_reddo. */
    void (*interim_reddo)(size_t mark);   /**< Zeroes back to the mark, then restores the top. */
    void (*interim_reset)(void);          /**< Zeroes the whole interim end, then gives it back. */
    mmgr_bool (*owns)(const void *at);    /**< Whether at lies in this pool's bytes. */
    size_t (*octas_praesto)(void);        /**< Bytes between the two ends. */
} SecuraCustodiae;
MMGR_NS_LAYOUT(SecuraCustodiae, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto);

/**
 * @brief Everything one pool is: its bytes, its state, and the entries bound to it.
 *
 * @param[in] r_     Region the pool belongs to, which every emitted symbol carries.
 * @param[in] type_  SolutaCustodiae or SecuraCustodiae, read by MMGR_CARCER_MEM and unused here.
 * @param[in] name_  Name the pool is reached by, as a member of its region.
 * @param[in] row_   Bytes in the pool.
 * @param[in] wipe_  1 where a release zeroes first, 0 where it does not.
 * @note The region name is part of every symbol, so two regions may each hold a pool of the same
 *       name, under different watches, without colliding.
 * @note wipe_ is a literal, so the branch it guards folds away and the watch costs nothing to read.
 * @warning A pool whose size is not a power of two, or is too small to hold one block, fails the
 *          build. The asserts sit here because this is the one place a size is stated, and the size
 *          is the only thing a declaration can get wrong: two pools are separate objects, so they
 *          cannot share an address and there is no overlap to check.
 */
#define MMGR_CARCER_BODY(r_, type_, name_, row_, wipe_)                                                                \
    MMGR_STATIC_ASSERT(((row_) & ((row_) - 1u)) == 0u, #r_ "." #name_ " is not a power of two");                       \
    MMGR_STATIC_ASSERT((row_) >= (2u * MMGR_CARCER_ALIGN), #r_ "." #name_ " is too small to hold one block");          \
    MMGR_ALIGN(MMGR_CARCER_ALIGN) static uint8_t r_##_##name_##_bytes[row_];                                           \
    static CarcerCtx r_##_##name_##_ctx = {r_##_##name_##_bytes, (row_), 0u, (row_)};                                  \
    static void *r_##_##name_##_persist_capio(size_t size)                                                             \
    {                                                                                                                  \
        return mmgr_carcer_persist_capio(&r_##_##name_##_ctx, size);                                                   \
    }                                                                                                                  \
    static void r_##_##name_##_persist_reddo(void *tenancy)                                                            \
    {                                                                                                                  \
        if (wipe_)                                                                                                     \
        {                                                                                                              \
            mmgr_carcer_secura_reddo(&r_##_##name_##_ctx, tenancy);                                                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_carcer_persist_reddo(&r_##_##name_##_ctx, tenancy);                                                   \
        }                                                                                                              \
    }                                                                                                                  \
    static void *r_##_##name_##_interim_capio(size_t size)                                                             \
    {                                                                                                                  \
        return mmgr_carcer_interim_capio(&r_##_##name_##_ctx, size);                                                   \
    }                                                                                                                  \
    static size_t r_##_##name_##_interim_mark(void)                                                                    \
    {                                                                                                                  \
        return mmgr_carcer_interim_mark(&r_##_##name_##_ctx);                                                          \
    }                                                                                                                  \
    static void r_##_##name_##_interim_reddo(size_t mark)                                                              \
    {                                                                                                                  \
        if (wipe_)                                                                                                     \
        {                                                                                                              \
            mmgr_carcer_interim_secura_reddo(&r_##_##name_##_ctx, mark);                                               \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_carcer_interim_reddo(&r_##_##name_##_ctx, mark);                                                      \
        }                                                                                                              \
    }                                                                                                                  \
    static void r_##_##name_##_interim_reset(void)                                                                     \
    {                                                                                                                  \
        r_##_##name_##_interim_reddo(r_##_##name_##_ctx.size);                                                         \
    }                                                                                                                  \
    static mmgr_bool r_##_##name_##_owns(const void *at)                                                               \
    {                                                                                                                  \
        return mmgr_carcer_owns(&r_##_##name_##_ctx, at);                                                              \
    }                                                                                                                  \
    static size_t r_##_##name_##_octas_praesto(void)                                                                   \
    {                                                                                                                  \
        return mmgr_carcer_octas_praesto(&r_##_##name_##_ctx);                                                         \
    }

/**
 * @brief One pool as a member of its region's type.
 *
 * @param[in] r_     Region the pool belongs to, unused here.
 * @param[in] type_  SolutaCustodiae or SecuraCustodiae, which the member takes as its type.
 * @param[in] name_  Name the member is given.
 * @param[in] row_   Bytes in the pool, unused here.
 * @param[in] wipe_  Watch flag, unused here.
 * @note Takes all five because the three readers share one tuple shape, as MMGR_SOLUTA describes.
 */
#define MMGR_CARCER_MEM(r_, type_, name_, row_, wipe_) type_ name_;

/**
 * @brief One pool's entries, in the order its accessor declares them.
 *
 * @param[in] r_     Region whose name every entry symbol carries.
 * @param[in] type_  Watch type, unused here.
 * @param[in] name_  Pool whose name the symbols carry alongside the region's.
 * @param[in] row_   Bytes in the pool, unused here.
 * @param[in] wipe_  Watch flag, unused here.
 * @note SolutaCustodiae and SecuraCustodiae declare the same eight members in the same order, so one
 *       initializer serves either watch.
 */
#define MMGR_CARCER_SEAT(r_, type_, name_, row_, wipe_)                                                                \
    {r_##_##name_##_persist_capio, r_##_##name_##_persist_reddo, r_##_##name_##_interim_capio,                         \
     r_##_##name_##_interim_mark,  r_##_##name_##_interim_reddo, r_##_##name_##_interim_reset,                         \
     r_##_##name_##_owns,          r_##_##name_##_octas_praesto},

/**
 * @brief Declares a pool under the loose watch, by name and size.
 *
 * @param[in] name_ Name the pool is reached by, as a member of its region.
 * @param[in] row_  Bytes in it.
 * @note Expands to a tuple rather than to code. The region reads the same list three times - once
 *       for the pools' bodies, once for its own members, once for their entries - so an element has
 *       to stay data until the region says which of the three it is being read as.
 */
#define MMGR_SOLUTA(name_, row_) (SolutaCustodiae, name_, row_, 0)

/**
 * @brief Declares a pool under the close watch, by name and size.
 *
 * @param[in] name_ Name the pool is reached by, as a member of its region.
 * @param[in] row_  Bytes in it.
 */
#define MMGR_SECURA(name_, row_) (SecuraCustodiae, name_, row_, 1)

/**
 * @brief Strips a tuple's parentheses.
 *
 * @param[in] ... The tuple's elements, which its own parentheses deliver as this macro's arguments.
 * @return        Those elements, comma separated, with nothing around them.
 * @note Written MMGR_UNTUPLE tuple_, with no parentheses of its own, so the tuple supplies them.
 */
#define MMGR_UNTUPLE(...) __VA_ARGS__

/**
 * @brief Applies one reader to one pool tuple, with the region spliced in ahead of it.
 *
 * @param[in] what_  Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] r_     Region, which reaches the reader ahead of the tuple's own elements.
 * @param[in] tuple_ One MMGR_SOLUTA or MMGR_SECURA tuple.
 * @return           What the reader expands to.
 * @note Two steps, because a macro's arguments are counted before they are expanded: the inner call
 *       is what lets the flattened tuple reach the reader as separate arguments.
 */
#define MMGR_CARCER_APPLY(what_, r_, tuple_) MMGR_CARCER_APPLY_(what_, r_, MMGR_UNTUPLE tuple_)

/**
 * @brief Expands to what_(r_, __VA_ARGS__).
 *
 * @param[in] what_ Reader to apply.
 * @param[in] r_    Region, which reaches the reader ahead of the elements.
 * @param[in] ...   The tuple's elements, already flattened by MMGR_UNTUPLE.
 * @return          What the reader expands to.
 * @note Called by MMGR_CARCER_APPLY.
 */
#define MMGR_CARCER_APPLY_(what_, r_, ...) what_(r_, __VA_ARGS__)

/**
 * @brief Reads a pool list with one reader, one line per pool count.
 *
 * @param[in] w_ Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] r_ Region, which reaches the reader with every tuple.
 * @param[in] a  Pool tuples in order; MMGR_CARCER_W1 takes one, MMGR_CARCER_W8 takes eight.
 * @note The preprocessor cannot walk a list, so the walk is written out. These lines stop at eight
 *       pools, and a region declaring nine pastes a MMGR_CARCER_W9 that does not exist. Writing that
 *       line is the whole fix; nothing else changes and no configured ceiling is involved.
 */
#define MMGR_CARCER_W1(w_, r_, a) MMGR_CARCER_APPLY(w_, r_, a)
#define MMGR_CARCER_W2(w_, r_, a, b) MMGR_CARCER_W1(w_, r_, a) MMGR_CARCER_APPLY(w_, r_, b)
#define MMGR_CARCER_W3(w_, r_, a, b, args) MMGR_CARCER_W2(w_, r_, a, b) MMGR_CARCER_APPLY(w_, r_, args)
#define MMGR_CARCER_W4(w_, r_, a, b, args, d) MMGR_CARCER_W3(w_, r_, a, b, args) MMGR_CARCER_APPLY(w_, r_, d)
#define MMGR_CARCER_W5(w_, r_, a, b, args, d, e) MMGR_CARCER_W4(w_, r_, a, b, args, d) MMGR_CARCER_APPLY(w_, r_, e)
#define MMGR_CARCER_W6(w_, r_, a, b, args, d, e, f)                                                                    \
    MMGR_CARCER_W5(w_, r_, a, b, args, d, e) MMGR_CARCER_APPLY(w_, r_, f)
#define MMGR_CARCER_W7(w_, r_, a, b, args, d, e, f, g)                                                                 \
    MMGR_CARCER_W6(w_, r_, a, b, args, d, e, f) MMGR_CARCER_APPLY(w_, r_, g)
#define MMGR_CARCER_W8(w_, r_, a, b, args, d, e, f, g, h)                                                              \
    MMGR_CARCER_W7(w_, r_, a, b, args, d, e, f, g) MMGR_CARCER_APPLY(w_, r_, h)

/**
 * @brief Expands the walk matching the pool count.
 *
 * @param[in] what_ Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] r_    Region, forwarded to the walk ahead of the tuples.
 * @param[in] ...   MMGR_SOLUTA and MMGR_SECURA tuples, one per pool.
 * @note MMGR_CAT builds the line's name from MMGR_NARG's count of the tuples.
 * @warning MMGR_NARG gives 1 for an empty list, so a region declaring no pools reaches
 *          MMGR_CARCER_W1 and fails there with too few arguments for the reader.
 */
#define MMGR_CARCER_WALK(what_, r_, ...) MMGR_CAT(MMGR_CARCER_W, MMGR_NARG(__VA_ARGS__))(what_, r_, __VA_ARGS__)

/**
 * @brief Declares a region and the pools it holds.
 *
 * @param[in] region_ Name of the region. Its pools are reached as members of it.
 * @param[in] ...     MMGR_SOLUTA and MMGR_SECURA declarations, one per pool.
 * @note Everything it emits is initialized data. Nothing runs at startup, and a pool's first byte is
 *       the address of its own storage, which the linker resolves.
 * @note Every emitted symbol carries the region's name, so a program may declare as many regions as
 *       it likes and two of them may each hold a pool called the same thing.
 * @note A pool's entries are bound to that pool, so no call names one and none can reach another's
 *       bytes. A soluta pool has no wiping release and a secura pool has no plain one.
 * @warning The bytes, the state and the region are all static, so a declaration in a header gives
 *          every translation unit that includes it a region of its own rather than one they share.
 */
#define Carceribus(region_, ...)                                                                                       \
    MMGR_CARCER_WALK(MMGR_CARCER_BODY, region_, __VA_ARGS__)                                                           \
    MMGR_NS struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_WALK(MMGR_CARCER_MEM, region_, __VA_ARGS__)                                                        \
    } region_ MMGR_UNUSED = {MMGR_CARCER_WALK(MMGR_CARCER_SEAT, region_, __VA_ARGS__)}

/**
 * @brief Takes size bytes from a pool's persistent end.
 *
 * @param[in,out] w    Pool to take from [BORROWS].
 * @param[in]     size Bytes wanted.
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [RETURNS OWNERSHIP].
 * @note Reached through the pool's own accessor rather than called by name.
 * @note The tenancy goes back through the same pool's persist_reddo, which takes it.
 * @warning The bytes are not zeroed, and a reused block still holds what the last tenant left. A
 *          pool declared MMGR_SECURA is the one that scrubs, and it scrubs on release.
 */
void *mmgr_carcer_persist_capio(CarcerCtx *w, size_t size);

/**
 * @brief Gives a tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] w       Pool the tenancy came from [BORROWS].
 * @param[in]     tenancy First byte of the tenancy [TAKES OWNERSHIP].
 * @note Which end the tenancy came from is read from its address, so a release cannot be given to the
 *       wrong end. A NULL tenancy returns without touching the pool.
 * @warning tenancy is dead once this returns and its bytes are not scrubbed; the pool may hand them
 *          out again.
 * @warning Nothing tests that tenancy came from this pool. An address from elsewhere is read as a
 *          header and freed into a chain it never belonged to.
 */
void mmgr_carcer_persist_reddo(CarcerCtx *w, void *tenancy);

/**
 * @brief Zeroes a tenancy, then gives it back.
 *
 * @param[in,out] w       Pool the tenancy came from [BORROWS].
 * @param[in,out] tenancy First byte of the tenancy, zeroed before it is released [TAKES OWNERSHIP].
 * @note The extent comes from the block's own header, so a caller cannot under-wipe a tenancy by
 *       naming fewer bytes than it holds. A NULL tenancy returns without touching the pool.
 * @note The wipe is the only step that separates this from mmgr_carcer_persist_reddo, which it calls
 *       to do the give-back.
 * @warning tenancy is dead once this returns; the pool may hand those bytes out again.
 * @warning Nothing tests that tenancy came from this pool, and the extent is read from the bytes
 *          lying ahead of it. An address from elsewhere is wiped for whatever length those bytes
 *          happen to hold.
 */
void mmgr_carcer_secura_reddo(CarcerCtx *w, void *tenancy);

/**
 * @brief Takes size bytes from a pool's interim end.
 *
 * @param[in,out] w    Pool to take from [BORROWS].
 * @param[in]     size Bytes wanted.
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [RETURNS OWNERSHIP].
 * @note The bytes normally go back by mark, through mmgr_carcer_interim_reddo or
 *       mmgr_carcer_interim_reset.
 * @note A single release through mmgr_carcer_persist_reddo works here too, but trims the top only
 *       when the freed block sits at it. Anything freed below stays in the chain, and no take at
 *       this end walks that chain, so nothing reuses it before the next rewind.
 * @warning The bytes are not zeroed, and a take returns whatever the last tenant left. A pool
 *          declared MMGR_SECURA is the one that scrubs, and it scrubs on release.
 */
void *mmgr_carcer_interim_capio(CarcerCtx *w, size_t size);

/**
 * @brief The interim end's current top, to hand back to mmgr_carcer_interim_reddo.
 *
 * @param[in] w Pool to read [BORROWS].
 * @return      The value of interim_top.
 * @note Good against this pool alone, and only until a restore to an older mark. The restore assigns
 *       the value it is handed without testing it.
 * @warning The figure is a snapshot. A take from a preempting handler lowers the top after this has
 *          read it, and restoring the mark then gives that take's bytes back as well.
 */
size_t mmgr_carcer_interim_mark(const CarcerCtx *w);

/**
 * @brief Restores the interim top a mark reported, scrubbing nothing.
 *
 * @param[in,out] w    Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore, as mmgr_carcer_interim_mark reported it.
 * @note The top is assigned, not tested, so the mark must be one this pool reported and must not be
 *       older than a restore already made.
 * @warning A mark this pool never reported, or one past its size, is taken as given and moves the
 *          end there.
 * @warning Every interim tenancy taken since mark is dead once this returns. Nothing is scrubbed, so
 *          such a pointer still dereferences and reads whatever the next take puts there.
 */
void mmgr_carcer_interim_reddo(CarcerCtx *w, size_t mark);

/**
 * @brief Zeroes every interim byte taken since mark, then restores the top.
 *
 * @param[in,out] w    Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore, as mmgr_carcer_interim_mark reported it.
 * @note The order is the point. The interim end grows down, so the live bytes are [top, mark) and
 *       reclaiming means raising the top. Wiping first means they are already zero at the instant
 *       they become available; reclaiming first would leave a window in which the next take, or a
 *       preempting handler, sees what the last tenant left.
 * @warning Every interim tenancy taken since mark is dead once this returns.
 * @warning The wipe is skipped when mark is not above the current top, or lies past the pool's size,
 *          and the top is assigned either way. A mark this pool did not report can rewind the end
 *          without scrubbing anything.
 * @warning The top is read once, ahead of the wipe. A take from a preempting handler landing between
 *          that read and the restore is dropped unscrubbed, since the extent was settled from the
 *          older top.
 */
void mmgr_carcer_interim_secura_reddo(CarcerCtx *w, size_t mark);

/**
 * @brief Gives the whole interim end back at once, scrubbing nothing.
 *
 * @param[in,out] w Pool to reset [BORROWS].
 * @note Restores the top to the pool's own size, which is where the end starts.
 * @note No pool entry reaches this one. A region's reset is its own generated wrapper, and that goes
 *       to the wiping rewind wherever the pool was declared MMGR_SECURA.
 * @warning Every interim tenancy the pool has handed out is dead once this returns, and none of
 *          those bytes are scrubbed.
 */
void mmgr_carcer_interim_reset(CarcerCtx *w);

/**
 * @brief Whether at lies inside the pool's bytes.
 *
 * @param[in] w  Pool to test against [BORROWS].
 * @param[in] at Address to test [BORROWS].
 * @return       MMGR_TRUE when at lies in the pool's storage, which is [base, base + size).
 * @note One unsigned compare covers both ends, since an address below base wraps to a difference
 *       larger than any size.
 * @warning Any address in the pool answers true, not only the first byte of a tenancy. This says
 *          where an address is, not what is there, so a true answer is not a warrant that at may be
 *          released: the releases read a header from whatever address they are handed.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCtx *w, const void *at);

/**
 * @brief The bytes lying between the two ends.
 *
 * @param[in] w Pool to read [BORROWS].
 * @return      interim_top minus persist_end, or 0 once the two ends have met.
 * @note A take carved from the middle needs a block header out of the same bytes, and the request is
 *       rounded up to a whole word first, so a take of exactly this many bytes cannot be met.
 * @warning The two ends are read one after the other and the answer is a snapshot. A take from a
 *          preempting handler landing between the reads gives a figure matching neither state, and
 *          any take at all leaves it stale before the caller can act on it.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCtx *w);

/**
 * @brief Zeroes size bytes at tenancy, without giving anything back.
 *
 * @param[in,out] tenancy First byte to clear [BORROWS].
 * @param[in]     size    Bytes to clear.
 * @note The stores are volatile, so clearing bytes nothing reads afterwards is not dropped as dead
 *       work. The middle is stored a word at a time, the two edges a byte at a time.
 * @note The extent is the caller's to state; a wiping release takes it from the block's own header.
 * @warning tenancy must be writable for size bytes.
 */
void mmgr_carcer_wipe(void *tenancy, size_t size);

/**
 * @brief Rounds size up to a whole machine word.
 *
 * @param[in] size Count to round.
 * @return         size rounded up to a multiple of MMGR_CARCER_ALIGN, and size itself when it already
 *                 is one.
 * @note A size of 0 rounds to 0. The takes do not round a request this way; they carry a request of 0
 *       up to one word first, so no tenancy is handed out empty.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask, so a size within a word of SIZE_MAX wraps
 *          to 0.
 */
size_t mmgr_carcer_align_up(size_t size);

MMGR_FINIS_DECLS

#endif
