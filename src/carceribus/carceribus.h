/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief The prison: a caller's storage divided into pools, and the table that works them.
 *
 * @note One declaration does everything. Carceribus() emits the bytes, their alignment, what each
 *       pool is, and the region record, all as initialized data - nothing runs at startup and a
 *       configuration that does not add up fails the build.
 * @note A pool's bytes are init.at plus its number times init.row. The region is const and a pool
 *       number is a literal, so that address is a constant the compiler drops in; nothing stores
 *       it and nothing loads it.
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
 * @note A build sets this before including this header, the way it sets any other knob here.
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
 * @param[in] type_  SolutaCustodiae or SecuraCustodiae.
 * @param[in] name_  Name the pool is reached by, as a member of its region.
 * @param[in] row_   Bytes in the pool.
 * @param[in] wipe_  1 where a release zeroes first, 0 where it does not.
 * @note The region name is part of every symbol, so two regions may each hold a pool of the same
 *       name, under different watches, without colliding.
 * @note wipe_ is a literal, so the branch it guards folds away and the watch costs nothing to read.
 * @warning A pool whose size is not a power of two, or is too small to hold one block, fails the
 *          build. The size is the only thing a declaration can get wrong: two pools are separate
 *          objects, so they cannot share an address and there is no overlap to check.
 * @warning A pool whose size is not a power of two, or is too small to hold one block, fails the build.
 *          The assert sits here because this is the one place the size is stated.
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

/** @brief One pool as a member of its region's type. */
#define MMGR_CARCER_MEM(r_, type_, name_, row_, wipe_) type_ name_;

/** @brief One pool's entries, in the order its accessor declares them. */
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

/** @brief Strips a tuple's parentheses. */
#define MMGR_UNTUPLE(...) __VA_ARGS__

/**
 * @brief Applies one reader to one pool tuple, with the region spliced in ahead of it.
 *
 * @note Two steps, because a macro's arguments are counted before they are expanded: the inner call
 *       is what lets the flattened tuple reach the reader as separate arguments.
 */
#define MMGR_CARCER_APPLY(what_, r_, tuple_) MMGR_CARCER_APPLY_(what_, r_, MMGR_UNTUPLE tuple_)
#define MMGR_CARCER_APPLY_(what_, r_, ...) what_(r_, __VA_ARGS__)

/**
 * @brief Reads a pool list with one reader, one line per pool count.
 *
 * @note The preprocessor cannot walk a list, so the walk is written out. Another pool count is
 *       another line; nothing else changes and nothing is capped by a configured ceiling.
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

/** @brief Expands the walk matching the pool count. */
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
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note Reached through the pool's own accessor rather than called by name.
 * @warning The bytes are not zeroed, and a reused block still holds what the last tenant left. A
 *          pool declared MMGR_SECURA is the one that guarantees otherwise.
 */
void *mmgr_carcer_persist_capio(CarcerCtx *w, size_t size);

/** @brief Gives a tenancy back, leaving its bytes as they are [TAKES OWNERSHIP]. */
void mmgr_carcer_persist_reddo(CarcerCtx *w, void *tenancy);

/** @brief Zeroes a tenancy, then gives it back [TAKES OWNERSHIP]. */
void mmgr_carcer_secura_reddo(CarcerCtx *w, void *tenancy);

/** @brief Takes size bytes from a pool's interim end, or NULL [BORROWS]. */
void *mmgr_carcer_interim_capio(CarcerCtx *w, size_t size);

/** @brief The interim end's current top, for mmgr_carcer_interim_reddo. */
size_t mmgr_carcer_interim_mark(const CarcerCtx *w);

/** @brief Restores the interim top a mark reported, scrubbing nothing. */
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
 */
void mmgr_carcer_interim_secura_reddo(CarcerCtx *w, size_t mark);

/** @brief Gives the whole interim end back at once, scrubbing nothing. */
void mmgr_carcer_interim_reset(CarcerCtx *w);

/** @brief Whether at lies inside the pool's bytes. */
mmgr_bool mmgr_carcer_owns(const CarcerCtx *w, const void *at);

/** @brief The bytes lying between the two ends. */
size_t mmgr_carcer_octas_praesto(const CarcerCtx *w);

/** @brief Zeroes size bytes at tenancy, without giving anything back. */
void mmgr_carcer_wipe(void *tenancy, size_t size);

/** @brief Rounds size up to a whole machine word. */
size_t mmgr_carcer_align_up(size_t size);

MMGR_FINIS_DECLS

#endif
