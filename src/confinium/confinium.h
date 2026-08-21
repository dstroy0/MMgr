// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_H
#define MMGR_CONFINIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file confinium.h
 * @brief One tenant. Two ends, growing toward each other, and nothing in between is ever freed.
 *
 * Persist grows up from the base. Interim grows down from the top. They meet in the middle and a
 * take fails rather than overrun.
 *
 * Interim is released by mark, not by pointer. Nothing is reallocated and nothing moves, so a
 * pointer handed out after a mark is dead the moment that mark is released.
 *
 * The tables are the whole surface. There are no free functions to call.
 *
 * Two tables, not one, because this module has two types. A tenant and a set of tenants take
 * different first arguments and answer different questions, and the twenty five entries between
 * them do not fit one MMGR_NS_LAYOUT, which stops at twenty four. Splitting them by the thing they
 * act on is what the split would have been anyway.
 *
 * The entries that are one bound check or one subtraction are defined here rather than in the .c,
 * because a call frame costs more than the body and every one of them sits inside a take. They
 * call each other by name: the table is not formed until the bottom of this header, so a body
 * above it has nothing to dispatch through yet.
 */

/** @brief Minimum alignment of anything handed out. */
#define MMGR_CONFIN_ALIGN 8u

/** @brief Largest alignment a take will honor. Anything above is clamped to it. */
#define MMGR_CONFIN_MAX_ALIGN 16u

/**
 * @brief One tenant.
 *
 * @c persist_end is where the upward end stops. @c scratch_top is where the downward end starts.
 * The high water fields never fall.
 */
typedef struct
{
    uint8_t *base;      /**< The buffer. */
    size_t size;        /**< Its size. */
    size_t persist_end; /**< Where the upward end stops. */
    size_t scratch_top; /**< Where the downward end starts. */
    size_t persist_used;/**< What the upward end holds. */
    size_t persist_hw;  /**< The most it has ever held. Never falls. */
    size_t scratch_hw;  /**< The most the downward end has ever held. Never falls. */
} mmgr_confin;

/**
 * @brief Round @p n up to MMGR_CONFIN_ALIGN.
 * @param n Byte count.
 * @return Rounded count.
 */
MMGR_INLINE size_t mmgr_confin_align_up(size_t n)
{
    return (n + (MMGR_CONFIN_ALIGN - 1)) & ~(size_t)(MMGR_CONFIN_ALIGN - 1);
}

/**
 * @brief Take @p n bytes from the interim end, aligned.
 * @param a Tenant.
 * @param n Byte count.
 * @param align Alignment, clamped into MMGR_CONFIN_ALIGN..MMGR_CONFIN_MAX_ALIGN.
 * @return The bytes, or NULL if the two ends would meet.
 */
MMGR_INLINE void *mmgr_confin_interim_capio_aligned(mmgr_confin *const a, size_t n, size_t align)
{
    if (align < MMGR_CONFIN_ALIGN)
    {
        align = MMGR_CONFIN_ALIGN;
    }
    if (align > MMGR_CONFIN_MAX_ALIGN)
    {
        align = MMGR_CONFIN_MAX_ALIGN;
    }
    n = mmgr_confin_align_up(n ? n : MMGR_CONFIN_ALIGN);
    if (a->scratch_top < n)
    {
        return NULL;
    }

    size_t nt = (a->scratch_top - n) & ~(size_t)(align - 1);

    /* The second half cannot fire: nt is scratch_top less a positive size and then masked down,
       so it is below scratch_top by construction. Kept as the other half of the bound, because
       reading it as one range check is what makes it obvious. */
    if (nt < a->persist_end || nt > a->scratch_top) /* GCOVR_EXCL_BR_LINE */
    {
        return NULL;
    }
    a->scratch_top = nt;
    size_t used = a->size - a->scratch_top;
    if (used > a->scratch_hw)
    {
        a->scratch_hw = used;
    }
    return a->base + a->scratch_top;
}

/**
 * @brief Current interim fill point.
 * @param a Tenant.
 * @return The mark.
 */
MMGR_INLINE size_t mmgr_confin_interim_mark(mmgr_confin *const a)
{
    return a->scratch_top;
}

/**
 * @brief Release interim back to @p mark.
 * @param a Tenant.
 * @param mark A mark from this tenant. A stale or forward mark is ignored.
 */
MMGR_INLINE void mmgr_confin_interim_reddo(mmgr_confin *const a, size_t mark)
{

    if (mark >= a->scratch_top && mark <= a->size)
    {
        a->scratch_top = mark;
    }
}

/**
 * @brief Release all interim.
 * @param a Tenant.
 */
MMGR_INLINE void mmgr_confin_interim_reset(mmgr_confin *const a)
{
    a->scratch_top = a->size;
}

/**
 * @brief Is @p p inside this tenant.
 * @param a Tenant.
 * @param p Pointer.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool mmgr_confin_owns(mmgr_confin *const a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

/**
 * @brief How much the interim end holds.
 * @param a Tenant.
 * @return Byte count.
 */
MMGR_INLINE size_t mmgr_confin_interim_used(mmgr_confin *const a)
{
    return a->size - a->scratch_top;
}

/** @name The entries the tenant table points at that are not inline above.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
void mmgr_confin_init(mmgr_confin *const a, void *base, size_t size);
void *mmgr_confin_persist_capio(mmgr_confin *const a, size_t n);
void mmgr_confin_persist_reddo(mmgr_confin *const a, void *p);
void *mmgr_confin_interim_capio(mmgr_confin *const a, size_t n);
size_t mmgr_confin_octas_praesto(mmgr_confin *const a);
size_t mmgr_confin_persist_used(mmgr_confin *const a);
/** @} */

/** @brief Tenant dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*align_up)(size_t n);
    void (*init)(mmgr_confin *const a, void *base, size_t size);
    void *(*persist_capio)(mmgr_confin *const a, size_t n);
    void (*persist_reddo)(mmgr_confin *const a, void *p);
    void *(*interim_capio_aligned)(mmgr_confin *const a, size_t n, size_t align);
    void *(*interim_capio)(mmgr_confin *const a, size_t n);
    size_t (*interim_mark)(mmgr_confin *const a);
    void (*interim_reddo)(mmgr_confin *const a, size_t mark);
    void (*interim_reset)(mmgr_confin *const a);
    mmgr_bool (*owns)(mmgr_confin *const a, const void *p);
    size_t (*octas_praesto)(mmgr_confin *const a);
    size_t (*persist_used)(mmgr_confin *const a);
    size_t (*interim_used)(mmgr_confin *const a);
} ConfiniumNs;
MMGR_NS_LAYOUT(ConfiniumNs, align_up, init, persist_capio, persist_reddo, interim_capio_aligned, interim_capio,
               interim_mark, interim_reddo, interim_reset, owns, octas_praesto, persist_used, interim_used);

/**
 * @brief Tenant namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS ConfiniumNs confin MMGR_UNUSED = {
    .align_up = mmgr_confin_align_up,
    .init = mmgr_confin_init,
    .persist_capio = mmgr_confin_persist_capio,
    .persist_reddo = mmgr_confin_persist_reddo,
    .interim_capio_aligned = mmgr_confin_interim_capio_aligned,
    .interim_capio = mmgr_confin_interim_capio,
    .interim_mark = mmgr_confin_interim_mark,
    .interim_reddo = mmgr_confin_interim_reddo,
    .interim_reset = mmgr_confin_interim_reset,
    .owns = mmgr_confin_owns,
    .octas_praesto = mmgr_confin_octas_praesto,
    .persist_used = mmgr_confin_persist_used,
    .interim_used = mmgr_confin_interim_used,
};

/** @brief How many tenants a set may hold. */
#ifndef MMGR_CONFIN_MAX_REGIONS
#define MMGR_CONFIN_MAX_REGIONS 2u
#endif

/** @brief Several tenants taken from as one. */
typedef struct
{
    mmgr_confin region[MMGR_CONFIN_MAX_REGIONS]; /**< The tenants. */
    size_t count;                                /**< How many of them are bound. */
} mmgr_confin_set;

/** @brief A mark across a whole set. Every region's fill point at once. */
typedef struct
{
    size_t top[MMGR_CONFIN_MAX_REGIONS]; /**< Each region's fill point. */
    size_t count;                        /**< How many were recorded. */
} mmgr_confin_mark;

/** @name The entries the set table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *
 *  A take walks the regions and stops at the first one with room. A question walks all of them and
 *  adds the answers up.
 *  @{ */
void mmgr_confin_set_init(mmgr_confin_set *const s);
mmgr_bool mmgr_confin_set_add(mmgr_confin_set *const s, void *base, size_t size);
void *mmgr_confin_set_persist_capio(mmgr_confin_set *const s, size_t n);
void mmgr_confin_set_persist_reddo(mmgr_confin_set *const s, void *p);
void *mmgr_confin_set_interim_capio_aligned(mmgr_confin_set *const s, size_t n, size_t align);
void *mmgr_confin_set_interim_capio(mmgr_confin_set *const s, size_t n);
mmgr_confin_mark mmgr_confin_set_interim_mark(mmgr_confin_set *const s);
void mmgr_confin_set_interim_reddo(mmgr_confin_set *const s, const mmgr_confin_mark *m);
void mmgr_confin_set_interim_reset(mmgr_confin_set *const s);
size_t mmgr_confin_set_octas_praesto(mmgr_confin_set *const s);
size_t mmgr_confin_set_persist_used(mmgr_confin_set *const s);
size_t mmgr_confin_set_interim_used(mmgr_confin_set *const s);
/** @} */

/** @brief Set dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*init)(mmgr_confin_set *const s);
    mmgr_bool (*add)(mmgr_confin_set *const s, void *base, size_t size);
    void *(*persist_capio)(mmgr_confin_set *const s, size_t n);
    void (*persist_reddo)(mmgr_confin_set *const s, void *p);
    void *(*interim_capio_aligned)(mmgr_confin_set *const s, size_t n, size_t align);
    void *(*interim_capio)(mmgr_confin_set *const s, size_t n);
    mmgr_confin_mark (*interim_mark)(mmgr_confin_set *const s);
    void (*interim_reddo)(mmgr_confin_set *const s, const mmgr_confin_mark *m);
    void (*interim_reset)(mmgr_confin_set *const s);
    size_t (*octas_praesto)(mmgr_confin_set *const s);
    size_t (*persist_used)(mmgr_confin_set *const s);
    size_t (*interim_used)(mmgr_confin_set *const s);
} ConfiniumSetNs;
MMGR_NS_LAYOUT(ConfiniumSetNs, init, add, persist_capio, persist_reddo, interim_capio_aligned, interim_capio,
               interim_mark, interim_reddo, interim_reset, octas_praesto, persist_used, interim_used);

/**
 * @brief Set namespace.
 *
 * static const, for the same reason the tenant's is.
 */
MMGR_NS ConfiniumSetNs confin_set MMGR_UNUSED = {
    .init = mmgr_confin_set_init,
    .add = mmgr_confin_set_add,
    .persist_capio = mmgr_confin_set_persist_capio,
    .persist_reddo = mmgr_confin_set_persist_reddo,
    .interim_capio_aligned = mmgr_confin_set_interim_capio_aligned,
    .interim_capio = mmgr_confin_set_interim_capio,
    .interim_mark = mmgr_confin_set_interim_mark,
    .interim_reddo = mmgr_confin_set_interim_reddo,
    .interim_reset = mmgr_confin_set_interim_reset,
    .octas_praesto = mmgr_confin_set_octas_praesto,
    .persist_used = mmgr_confin_set_persist_used,
    .interim_used = mmgr_confin_set_interim_used,
};

MMGR_FINIS_DECLS

#endif
