/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file carceribus.h
 * @brief The prison: a caller's storage divided into pools, and the table that works them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
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
 * @note Off by default because the two figures cost a size_t each in every CarcerCtx, and a build that
 *       never reads them should not carry them.
 * @note Adds CarcerCtx::persist_hw and CarcerCtx::interim_hw, and the update that raises them in
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
    void *(*persist_capio)(size_t size);  /**< Takes size bytes from the bottom, not zeroed [RETURNS OWNERSHIP]. */
    void (*persist_reddo)(void *tenancy); /**< Gives a tenancy back, unwiped [TAKES OWNERSHIP]. */
    void *(*interim_capio)(size_t size);  /**< Takes size bytes from the top, not zeroed [RETURNS OWNERSHIP]. */
    size_t (*interim_mark)(void);         /**< The current top, for interim_reddo. */
    void (*interim_reddo)(size_t mark);   /**< Restores the top a mark reported, scrubbing nothing. */
    void (*interim_reset)(void);          /**< Gives the whole interim end back at once. */
    mmgr_bool (*owns)(const void *at);    /**< Whether at lies in this pool's bytes [BORROWS]. */
    size_t (*octas_praesto)(void);        /**< Bytes between the two ends. */
} SolutaCustodiae;
MMGR_NS_LAYOUT(SolutaCustodiae, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto);

/**
 * @brief The close watch over one pool: every release zeroes the bytes before giving them back.
 *
 * @note MMGR_NS_LAYOUT asserts the eight members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The same eight entries, and no unwiped release among them. Each wipe runs before the boundary
 *       moves, so the bytes are zero at the instant they become available. Where the wipe is skipped
 *       and the boundary still moves, mmgr_carcer_interim_secura_reddo carries it in its warnings.
 */
typedef struct
{
    void *(*persist_capio)(size_t size);  /**< Takes size bytes from the bottom [RETURNS OWNERSHIP]. */
    void (*persist_reddo)(void *tenancy); /**< Zeroes a tenancy, then gives it back [TAKES OWNERSHIP]. */
    void *(*interim_capio)(size_t size);  /**< Takes size bytes from the top [RETURNS OWNERSHIP]. */
    size_t (*interim_mark)(void);         /**< The current top, for interim_reddo. */
    void (*interim_reddo)(size_t mark);   /**< Zeroes back to the mark, then restores the top. */
    void (*interim_reset)(void);          /**< Zeroes the whole interim end, then gives it back. */
    mmgr_bool (*owns)(const void *at);    /**< Whether at lies in this pool's bytes [BORROWS]. */
    size_t (*octas_praesto)(void);        /**< Bytes between the two ends. */
} SecuraCustodiae;
MMGR_NS_LAYOUT(SecuraCustodiae, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto);

/**
 * @brief Everything one pool is: its bytes, its state, and the entries bound to it.
 *
 * @param[in] region_ Region the pool belongs to, which every emitted symbol carries.
 * @param[in] type_   SolutaCustodiae or SecuraCustodiae, read by MMGR_CARCER_MEM and unused here.
 * @param[in] name_   Name the pool is reached by, as a member of its region.
 * @param[in] row_    Bytes in the pool.
 * @param[in] wipe_   1 where a release zeroes first, 0 where it does not.
 * @note The region name is part of every symbol, so two regions may each hold a pool of the same
 *       name, under different watches, without colliding.
 * @note wipe_ is a literal, so the branch it guards folds away and the watch costs nothing to read.
 * @warning A pool whose size is not a power of two, or is too small to hold one block, fails the
 *          build. The asserts sit here because this is the one place a size is stated, and a bad
 *          size is the one mistake the language would otherwise accept: a wrong name or watch type
 *          fails on its own, and two pools are separate objects, so they cannot share an address
 *          and there is no overlap to check.
 */
#define MMGR_CARCER_BODY(region_, type_, name_, row_, wipe_)                                                           \
    MMGR_STATIC_ASSERT(((row_) & ((row_) - 1u)) == 0u, #region_ "." #name_ " is not a power of two");                   \
    MMGR_STATIC_ASSERT((row_) >= (2u * MMGR_CARCER_ALIGN), #region_ "." #name_ " is too small to hold one block");      \
    MMGR_ALIGN(MMGR_CARCER_ALIGN) static uint8_t region_##_##name_##_bytes[row_];                                       \
    static CarcerCtx region_##_##name_##_ctx = {region_##_##name_##_bytes, (row_), 0u, (row_)};                         \
    static void *region_##_##name_##_persist_capio(size_t size)                                                         \
    {                                                                                                                  \
        return mmgr_carcer_persist_capio(&region_##_##name_##_ctx, size);                                              \
    }                                                                                                                  \
    static void region_##_##name_##_persist_reddo(void *tenancy)                                                        \
    {                                                                                                                  \
        if (wipe_)                                                                                                     \
        {                                                                                                              \
            mmgr_carcer_secura_reddo(&region_##_##name_##_ctx, tenancy);                                               \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_carcer_persist_reddo(&region_##_##name_##_ctx, tenancy);                                              \
        }                                                                                                              \
    }                                                                                                                  \
    static void *region_##_##name_##_interim_capio(size_t size)                                                         \
    {                                                                                                                  \
        return mmgr_carcer_interim_capio(&region_##_##name_##_ctx, size);                                              \
    }                                                                                                                  \
    static size_t region_##_##name_##_interim_mark(void)                                                                \
    {                                                                                                                  \
        return mmgr_carcer_interim_mark(&region_##_##name_##_ctx);                                                     \
    }                                                                                                                  \
    static void region_##_##name_##_interim_reddo(size_t mark)                                                          \
    {                                                                                                                  \
        if (wipe_)                                                                                                     \
        {                                                                                                              \
            mmgr_carcer_interim_secura_reddo(&region_##_##name_##_ctx, mark);                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            mmgr_carcer_interim_reddo(&region_##_##name_##_ctx, mark);                                                 \
        }                                                                                                              \
    }                                                                                                                  \
    static void region_##_##name_##_interim_reset(void)                                                                 \
    {                                                                                                                  \
        region_##_##name_##_interim_reddo(region_##_##name_##_ctx.size);                                                \
    }                                                                                                                  \
    static mmgr_bool region_##_##name_##_owns(const void *at)                                                           \
    {                                                                                                                  \
        return mmgr_carcer_owns(&region_##_##name_##_ctx, at);                                                         \
    }                                                                                                                  \
    static size_t region_##_##name_##_octas_praesto(void)                                                               \
    {                                                                                                                  \
        return mmgr_carcer_octas_praesto(&region_##_##name_##_ctx);                                                     \
    }

/**
 * @brief One pool as a member of its region's type.
 *
 * @param[in] region_ Region the pool belongs to, unused here.
 * @param[in] type_   SolutaCustodiae or SecuraCustodiae, which the member takes as its type.
 * @param[in] name_   Name the member is given.
 * @param[in] row_    Bytes in the pool, unused here.
 * @param[in] wipe_   Watch flag, unused here.
 * @note Takes all five because the three readers share one tuple shape, as MMGR_SOLUTA describes.
 */
#define MMGR_CARCER_MEM(region_, type_, name_, row_, wipe_) type_ name_;

/**
 * @brief One pool's entries, in the order its accessor declares them.
 *
 * @param[in] region_ Region whose name every entry symbol carries.
 * @param[in] type_   Watch type, unused here.
 * @param[in] name_   Pool whose name the symbols carry alongside the region's.
 * @param[in] row_    Bytes in the pool, unused here.
 * @param[in] wipe_   Watch flag, unused here.
 * @note SolutaCustodiae and SecuraCustodiae declare the same eight members in the same order, so one
 *       initializer serves either watch.
 */
#define MMGR_CARCER_SEAT(region_, type_, name_, row_, wipe_)                                                           \
    {region_##_##name_##_persist_capio, region_##_##name_##_persist_reddo, region_##_##name_##_interim_capio,           \
     region_##_##name_##_interim_mark,  region_##_##name_##_interim_reddo, region_##_##name_##_interim_reset,           \
     region_##_##name_##_owns,          region_##_##name_##_octas_praesto},

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
 * @note The trailing 1 is the wipe flag MMGR_CARCER_BODY reads to pick the wiping release at both
 *       ends. Choosing this over MMGR_SOLUTA is where a pool's watch is settled, and nothing after
 *       the declaration can change it.
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
 * @param[in] what_   Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] region_ Region, which reaches the reader ahead of the tuple's own elements.
 * @param[in] tuple_  One MMGR_SOLUTA or MMGR_SECURA tuple.
 * @return            What the reader expands to.
 * @note Two steps, because a macro's arguments are counted before they are expanded: the inner call
 *       is what lets the flattened tuple reach the reader as separate arguments.
 */
#define MMGR_CARCER_APPLY(what_, region_, tuple_) MMGR_CARCER_APPLY_(what_, region_, MMGR_UNTUPLE tuple_)

/**
 * @brief Expands to what_(region_, __VA_ARGS__).
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader ahead of the elements.
 * @param[in] ...     The tuple's elements, already flattened by MMGR_UNTUPLE.
 * @return            What the reader expands to.
 * @note The inner half of MMGR_CARCER_APPLY's two steps. Its arguments are expanded before they are
 *       substituted, so MMGR_UNTUPLE runs here and the tuple's elements reach the reader as
 *       separate arguments rather than as one.
 */
#define MMGR_CARCER_APPLY_(what_, region_, ...) what_(region_, __VA_ARGS__)

/**
 * @brief Applies the reader to a region's one pool tuple.
 *
 * @param[in] what_   Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] region_ Region, which reaches the reader ahead of the tuple's own elements.
 * @param[in] pool1_  The pool's tuple.
 * @note The base of the walk. Every longer line ends in this one, so it is the only line here that
 *       names no other.
 */
#define MMGR_CARCER_W1(what_, region_, pool1_) MMGR_CARCER_APPLY(what_, region_, pool1_)

/**
 * @brief Applies the reader to a region's two pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple, which MMGR_CARCER_W1 takes.
 * @param[in] pool2_  Second pool's tuple, applied after it.
 * @note The step every longer line repeats: expand the line one shorter, then apply the reader once
 *       more. The preprocessor cannot walk a list, so each pool count needs a line of its own.
 */
#define MMGR_CARCER_W2(what_, region_, pool1_, pool2_)                                                                 \
    MMGR_CARCER_W1(what_, region_, pool1_)                                                                             \
    MMGR_CARCER_APPLY(what_, region_, pool2_)

/**
 * @brief Applies the reader to a region's three pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple, applied after MMGR_CARCER_W2 expands the first two.
 * @note MMGR_CARCER_WALK selects this line for a region declaring three pools.
 */
#define MMGR_CARCER_W3(what_, region_, pool1_, pool2_, pool3_)                                                         \
    MMGR_CARCER_W2(what_, region_, pool1_, pool2_)                                                                     \
    MMGR_CARCER_APPLY(what_, region_, pool3_)

/**
 * @brief Applies the reader to a region's four pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple.
 * @param[in] pool4_  Fourth pool's tuple, applied after MMGR_CARCER_W3 expands the first three.
 * @note MMGR_CARCER_WALK selects this line for a region declaring four pools.
 */
#define MMGR_CARCER_W4(what_, region_, pool1_, pool2_, pool3_, pool4_)                                                 \
    MMGR_CARCER_W3(what_, region_, pool1_, pool2_, pool3_)                                                             \
    MMGR_CARCER_APPLY(what_, region_, pool4_)

/**
 * @brief Applies the reader to a region's five pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple.
 * @param[in] pool4_  Fourth pool's tuple.
 * @param[in] pool5_  Fifth pool's tuple, applied after MMGR_CARCER_W4 expands the first four.
 * @note MMGR_CARCER_WALK selects this line for a region declaring five pools.
 */
#define MMGR_CARCER_W5(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_)                                         \
    MMGR_CARCER_W4(what_, region_, pool1_, pool2_, pool3_, pool4_)                                                     \
    MMGR_CARCER_APPLY(what_, region_, pool5_)
/**
 * @brief Applies the reader to a region's six pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple.
 * @param[in] pool4_  Fourth pool's tuple.
 * @param[in] pool5_  Fifth pool's tuple.
 * @param[in] pool6_  Sixth pool's tuple, applied after MMGR_CARCER_W5 expands the first five.
 * @note MMGR_CARCER_WALK selects this line for a region declaring six pools.
 */
#define MMGR_CARCER_W6(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_, pool6_)                                 \
    MMGR_CARCER_W5(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_)                                             \
    MMGR_CARCER_APPLY(what_, region_, pool6_)

/**
 * @brief Applies the reader to a region's seven pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple.
 * @param[in] pool4_  Fourth pool's tuple.
 * @param[in] pool5_  Fifth pool's tuple.
 * @param[in] pool6_  Sixth pool's tuple.
 * @param[in] pool7_  Seventh pool's tuple, applied after MMGR_CARCER_W6 expands the first six.
 * @note MMGR_CARCER_WALK selects this line for a region declaring seven pools.
 */
#define MMGR_CARCER_W7(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_, pool6_, pool7_)                         \
    MMGR_CARCER_W6(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_, pool6_)                                     \
    MMGR_CARCER_APPLY(what_, region_, pool7_)

/**
 * @brief Applies the reader to a region's eight pool tuples.
 *
 * @param[in] what_   Reader to apply.
 * @param[in] region_ Region, which reaches the reader with every tuple.
 * @param[in] pool1_  First pool's tuple.
 * @param[in] pool2_  Second pool's tuple.
 * @param[in] pool3_  Third pool's tuple.
 * @param[in] pool4_  Fourth pool's tuple.
 * @param[in] pool5_  Fifth pool's tuple.
 * @param[in] pool6_  Sixth pool's tuple.
 * @param[in] pool7_  Seventh pool's tuple.
 * @param[in] pool8_  Eighth pool's tuple, applied after MMGR_CARCER_W7 expands the first seven.
 * @note MMGR_CARCER_WALK selects this line for a region declaring eight pools.
 * @warning The last line written. A region declaring nine pools pastes MMGR_CARCER_W9, which does
 *          not exist. Writing that line is the whole fix; nothing else changes and no configured
 *          ceiling is involved.
 */
#define MMGR_CARCER_W8(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_, pool6_, pool7_, pool8_)                 \
    MMGR_CARCER_W7(what_, region_, pool1_, pool2_, pool3_, pool4_, pool5_, pool6_, pool7_)                             \
    MMGR_CARCER_APPLY(what_, region_, pool8_)

/**
 * @brief Expands the walk matching the pool count.
 *
 * @param[in] what_   Reader to apply: MMGR_CARCER_BODY, MMGR_CARCER_MEM or MMGR_CARCER_SEAT.
 * @param[in] region_ Region, forwarded to the walk ahead of the tuples.
 * @param[in] ...     MMGR_SOLUTA and MMGR_SECURA tuples, one per pool.
 * @note MMGR_CAT builds the line's name from MMGR_NARG's count of the tuples.
 * @warning MMGR_NARG gives 1 for an empty list, so a region declaring no pools reaches
 *          MMGR_CARCER_W1 and fails there with too few arguments for the reader.
 */
#define MMGR_CARCER_WALK(what_, region_, ...)                                                                          \
    MMGR_CAT(MMGR_CARCER_W, MMGR_NARG(__VA_ARGS__))(what_, region_, __VA_ARGS__)

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
 * @param[in,out] pool Pool to take from [BORROWS].
 * @param[in]     size Bytes wanted.
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [RETURNS OWNERSHIP].
 * @note Reached through the pool's own accessor rather than called by name.
 * @note The tenancy goes back through the same pool's persist_reddo, which takes it.
 * @warning The bytes are not zeroed, and a reused block still holds what the last tenant left. A
 *          pool declared MMGR_SECURA is the one that scrubs, and it scrubs on release.
 */
void *mmgr_carcer_persist_capio(CarcerCtx *pool, size_t size);

/**
 * @brief Gives a tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] pool    Pool the tenancy came from [BORROWS].
 * @param[in]     tenancy First byte of the tenancy [TAKES OWNERSHIP].
 * @note Which end the tenancy came from is read from its address, so a release cannot be given to the
 *       wrong end. A NULL tenancy returns without touching the pool.
 * @warning tenancy is dead once this returns and its bytes are not scrubbed; the pool may hand them
 *          out again.
 * @warning Nothing tests that tenancy came from this pool. An address from elsewhere is read as a
 *          header and freed into a chain it never belonged to.
 */
void mmgr_carcer_persist_reddo(CarcerCtx *pool, void *tenancy);

/**
 * @brief Zeroes a tenancy, then gives it back.
 *
 * @param[in,out] pool    Pool the tenancy came from [BORROWS].
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
void mmgr_carcer_secura_reddo(CarcerCtx *pool, void *tenancy);

/**
 * @brief Takes size bytes from a pool's interim end.
 *
 * @param[in,out] pool Pool to take from [BORROWS].
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
void *mmgr_carcer_interim_capio(CarcerCtx *pool, size_t size);

/**
 * @brief The interim end's current top, to hand back to mmgr_carcer_interim_reddo.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         The value of interim_top.
 * @note Good against this pool alone, and only until a restore to an older mark. The restore assigns
 *       the value it is handed without testing it.
 * @warning The figure is a snapshot. A take from a preempting handler lowers the top after this has
 *          read it, and restoring the mark then gives that take's bytes back as well.
 */
size_t mmgr_carcer_interim_mark(const CarcerCtx *pool);

/**
 * @brief Restores the interim top a mark reported, scrubbing nothing.
 *
 * @param[in,out] pool Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore, as mmgr_carcer_interim_mark reported it.
 * @note The top is assigned, not tested, so the mark must be one this pool reported, and it must
 *       not lie below the current top. A mark below it lowers the end onto bytes an earlier
 *       restore already gave back.
 * @warning A mark this pool never reported, or one past its size, is taken as given and moves the
 *          end there.
 * @warning Every interim tenancy taken since mark is dead once this returns. Nothing is scrubbed, so
 *          such a pointer still dereferences and reads whatever the next take puts there.
 */
void mmgr_carcer_interim_reddo(CarcerCtx *pool, size_t mark);

/**
 * @brief Zeroes every interim byte taken since mark, then restores the top.
 *
 * @param[in,out] pool Pool to rewind [BORROWS].
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
void mmgr_carcer_interim_secura_reddo(CarcerCtx *pool, size_t mark);

/**
 * @brief Gives the whole interim end back at once, scrubbing nothing.
 *
 * @param[in,out] pool Pool to reset [BORROWS].
 * @note Restores the top to the pool's own size, which is where the end starts.
 * @note No pool entry reaches this one. A region's reset is its own generated wrapper, and that goes
 *       to the wiping rewind wherever the pool was declared MMGR_SECURA.
 * @warning Every interim tenancy the pool has handed out is dead once this returns, and none of
 *          those bytes are scrubbed.
 */
void mmgr_carcer_interim_reset(CarcerCtx *pool);

/**
 * @brief Whether at lies inside the pool's bytes.
 *
 * @param[in] pool Pool to test against [BORROWS].
 * @param[in] at   Address to test [BORROWS].
 * @return         MMGR_TRUE when at lies in the pool's storage, which is [base, base + size).
 * @note One unsigned compare covers both ends, since an address below base wraps to a difference
 *       larger than any size.
 * @warning Any address in the pool answers true, not only the first byte of a tenancy. This says
 *          where an address is, not what is there, so a true answer is not a warrant that at may be
 *          released: the releases read a header from whatever address they are handed.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCtx *pool, const void *at);

/**
 * @brief The bytes lying between the two ends.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         interim_top minus persist_end, or 0 once the two ends have met.
 * @note A take carved from the middle needs a block header out of the same bytes, and the request is
 *       rounded up to a whole word first, so a take of exactly this many bytes cannot be met.
 * @warning The two ends are read one after the other and the answer is a snapshot. A take from a
 *          preempting handler landing between the reads gives a figure matching neither state, and
 *          any take at all leaves it stale before the caller can act on it.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCtx *pool);

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
