// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_H
#define MMGR_CONFINIUM_H

#include "config/mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file confinium.h
 * @brief One tenant. Two ends, growing toward each other, and nothing in between is ever freed.
 *
 * Persist grows up from the base. Interim grows down from the top. They meet in the middle and a
 * take fails rather than overrun.
 *
 * Interim is released by mark, not by pointer. Nothing is reallocated and nothing moves, so a
 * pointer handed out after a mark is dead the moment that mark is released.
 */

/**
 * @brief How many workers this build has.
 * @return Worker count.
 */
int mmgr_worker_count(void);

/**
 * @brief Which worker is calling.
 * @return Worker id.
 *
 * A single worker build knows the answer at compile time, so it is a constant and there is no
 * platform hook to supply.
 */
#if MMGR_WORKER_COUNT == 1
MMGR_INLINE int mmgr_worker_self(void)
{
    return 0;
}
#else
int mmgr_worker_self(void);
void mmgr_worker_set_self(int id);
#endif

/** @brief Minimum alignment of anything handed out. */
#define MMGR_CONFIN_ALIGN 8u

/**
 * @brief Round @p n up to MMGR_CONFIN_ALIGN.
 * @param n Byte count.
 * @return Rounded count.
 */
MMGR_INLINE size_t mmgr_confin_align_up(size_t n)
{
    return (n + (MMGR_CONFIN_ALIGN - 1)) & ~(size_t)(MMGR_CONFIN_ALIGN - 1);
}

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
    uint8_t *base;
    size_t size;
    size_t persist_end;
    size_t scratch_top;
    size_t persist_used;
    size_t persist_hw;
    size_t scratch_hw;
} mmgr_confin;

/**
 * @brief Bind a tenant to a buffer.
 * @param a Tenant.
 * @param base Buffer.
 * @param size Its size.
 */
void mmgr_confin_init(mmgr_confin *a, void *base, size_t size);

/**
 * @brief Take @p n bytes that a mark release will not reclaim.
 * @param a Tenant.
 * @param n Byte count.
 * @return The bytes, or NULL if the two ends would meet.
 */
void *mmgr_confin_persist_capio(mmgr_confin *a, size_t n);

/**
 * @brief Give back a persist block.
 * @param a Tenant.
 * @param p A pointer from mmgr_confin_persist_capio.
 */
void mmgr_confin_persist_reddo(mmgr_confin *a, void *p);

/**
 * @brief Take @p n bytes from the interim end, aligned.
 * @param a Tenant.
 * @param n Byte count.
 * @param align Alignment, clamped into MMGR_CONFIN_ALIGN..MMGR_CONFIN_MAX_ALIGN.
 * @return The bytes, or NULL if the two ends would meet.
 */
MMGR_INLINE void *mmgr_confin_interim_capio_aligned(mmgr_confin *a, size_t n, size_t align)
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
 * @brief Take @p n bytes from the interim end.
 * @param a Tenant.
 * @param n Byte count.
 * @return The bytes, or NULL if the two ends would meet.
 */
void *mmgr_confin_interim_capio(mmgr_confin *a, size_t n);

/**
 * @brief Current interim fill point.
 * @param a Tenant.
 * @return The mark.
 */
MMGR_INLINE size_t mmgr_confin_interim_mark(const mmgr_confin *a)
{
    return a->scratch_top;
}

/**
 * @brief Release interim back to @p mark.
 * @param a Tenant.
 * @param mark A mark from this tenant. A stale or forward mark is ignored.
 */
MMGR_INLINE void mmgr_confin_interim_reddo(mmgr_confin *a, size_t mark)
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
MMGR_INLINE void mmgr_confin_interim_reset(mmgr_confin *a)
{
    a->scratch_top = a->size;
}

/**
 * @brief Is @p p inside this tenant.
 * @param a Tenant.
 * @param p Pointer.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool mmgr_confin_owns(const mmgr_confin *a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

/**
 * @brief How many bytes are still takeable.
 * @param a Tenant.
 * @return Byte count.
 */
size_t mmgr_confin_octas_praesto(const mmgr_confin *a);

/**
 * @brief How much the persist end holds.
 * @param a Tenant.
 * @return Byte count.
 */
size_t mmgr_confin_persist_used(const mmgr_confin *a);

/**
 * @brief How much the interim end holds.
 * @param a Tenant.
 * @return Byte count.
 */
MMGR_INLINE size_t mmgr_confin_interim_used(const mmgr_confin *a)
{
    return a->size - a->scratch_top;
}

/** @brief How many tenants a set may hold. */
#ifndef MMGR_CONFIN_MAX_REGIONS
#define MMGR_CONFIN_MAX_REGIONS 2u
#endif

/** @brief Several tenants taken from as one. */
typedef struct
{
    mmgr_confin region[MMGR_CONFIN_MAX_REGIONS];
    size_t count;
} mmgr_confin_set;

/** @brief A mark across a whole set. Every region's fill point at once. */
typedef struct
{
    size_t top[MMGR_CONFIN_MAX_REGIONS];
    size_t count;
} mmgr_confin_mark;

/**
 * @brief Empty a set.
 * @param s Set.
 */
void mmgr_confin_set_init(mmgr_confin_set *s);

/**
 * @brief Add a region.
 * @param s Set.
 * @param base Buffer.
 * @param size Its size.
 * @return MMGR_FALSE if the set is full or the region is too small to hold anything.
 */
mmgr_bool mmgr_confin_set_add(mmgr_confin_set *s, void *base, size_t size);

/**
 * @brief Take persist bytes from the first region with room.
 * @param s Set.
 * @param n Byte count.
 * @return The bytes, or NULL if no region has room.
 */
void *mmgr_confin_set_persist_capio(mmgr_confin_set *s, size_t n);

/**
 * @brief Give back a persist block to whichever region owns it.
 * @param s Set.
 * @param p A pointer from mmgr_confin_set_persist_capio.
 */
void mmgr_confin_set_persist_reddo(mmgr_confin_set *s, void *p);

/**
 * @brief Take interim bytes from the first region with room, aligned.
 * @param s Set.
 * @param n Byte count.
 * @param align Alignment.
 * @return The bytes, or NULL if no region has room.
 */
void *mmgr_confin_set_interim_capio_aligned(mmgr_confin_set *s, size_t n, size_t align);

/**
 * @brief Take interim bytes from the first region with room.
 * @param s Set.
 * @param n Byte count.
 * @return The bytes, or NULL if no region has room.
 */
void *mmgr_confin_set_interim_capio(mmgr_confin_set *s, size_t n);

/**
 * @brief Every region's interim fill point.
 * @param s Set.
 * @return The mark.
 */
mmgr_confin_mark mmgr_confin_set_interim_mark(const mmgr_confin_set *s);

/**
 * @brief Release every region's interim back to @p m.
 * @param s Set.
 * @param m A mark from this set.
 */
void mmgr_confin_set_interim_reddo(mmgr_confin_set *s, const mmgr_confin_mark *m);

/**
 * @brief Release all interim in every region.
 * @param s Set.
 */
void mmgr_confin_set_interim_reset(mmgr_confin_set *s);

/**
 * @brief How many bytes the set can still hand out.
 * @param s Set.
 * @return Byte count, summed over the regions.
 */
size_t mmgr_confin_set_octas_praesto(const mmgr_confin_set *s);

/**
 * @brief How much persist the set holds.
 * @param s Set.
 * @return Byte count, summed over the regions.
 */
size_t mmgr_confin_set_persist_used(const mmgr_confin_set *s);

/**
 * @brief How much interim the set holds.
 * @param s Set.
 * @return Byte count, summed over the regions.
 */
size_t mmgr_confin_set_interim_used(const mmgr_confin_set *s);

MMGR_END_DECLS

#endif
