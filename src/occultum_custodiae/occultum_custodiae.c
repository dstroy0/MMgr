// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "occultum_custodiae/occultum_custodiae.h"
#include "confinium/confinium.h"

/**
 * @file occultum_custodiae.c
 * @brief The secure guardian. Same shape as clarus, but released bytes are wiped.
 *
 * Every entry below takes one parameter, a pointer to OccultCtx. A take is a byte count, an
 * alignment and the pool it comes from, so they are one context.
 */

#define SEC_BLOCK_BYTES ((uintptr_t)MMGR_SECURE_CONFIN_SIZE)

#define SEC_NO_OFFSET (~(uintptr_t)0)

struct SecureStorage
{
    _Alignas(32) uint8_t mem[MMGR_SECURE_CONFIN_SIZE];
};

struct SecureInternal
{
    struct SecureStorage *store;
    mmgr_confin pool;
};

static struct SecureStorage s_store;

struct SecureInternal mmgr_occult_state;

/** @brief One take, return or question of the pool. */
typedef struct
{
    struct SecureInternal *pool; /**< The pool. */
    size_t n;                    /**< Bytes wanted. */
    size_t align;                /**< Alignment wanted. */
    size_t mark;                 /**< The mark being rewound to. */
    const void *p;               /**< The pointer being asked about. */
} OccultCtx;

/**
 * @brief Point the context at the pool.
 * @param c In/out. The take.
 */
MMGR_INLINE void occult_self(OccultCtx *c)
{
    c->pool = &mmgr_occult_state;
}

/**
 * @brief The tenant, bound to its storage on first use.
 * @param c In/out. The take.
 * @return The tenant.
 */
MMGR_INLINE mmgr_confin *occult_bind(OccultCtx *c)
{
    occult_self(c);

    mmgr_confin *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_store;
        mmgr_confin_init(a, c->pool->store->mem, MMGR_SECURE_CONFIN_SIZE);
    }
    return a;
}

/**
 * @brief The tenant, if it has been bound.
 * @param c In/out. The take.
 * @return The tenant, or NULL.
 */
MMGR_INLINE mmgr_confin *occult_peek(OccultCtx *c)
{
    occult_self(c);

    mmgr_confin *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

/**
 * @brief Offset of @c p into the pool's store.
 * @param c In/out. The take.
 * @return The offset, or SEC_NO_OFFSET when the pool has no store.
 */
MMGR_INLINE uintptr_t occult_offset(OccultCtx *c)
{
    occult_self(c);

    if (c->pool->store == NULL)
    {
        return SEC_NO_OFFSET;
    }
    return (uintptr_t)c->p - (uintptr_t)c->pool->store->mem;
}

/**
 * @brief Wipe from the fill point down to @c mark, then move it there.
 * @param c In/out. The take.
 *
 * This is what makes the secure pool secure. Releasing a mark hands the bytes back for reuse, so
 * they are zeroed before the fill point moves.
 */
MMGR_INLINE void occult_wipe_down_to(OccultCtx *c)
{
    mmgr_confin *a = occult_bind(c);
    const size_t top = mmgr_confin_interim_mark(a);

    if ((c->mark > top) && (c->mark <= a->size))
    {
        mmgr_occult_wipe(a->base + top, c->mark - top);
    }
    mmgr_confin_interim_reddo(a, c->mark);
}

/**
 * @brief Take @c n bytes that a mark release will reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *occult_capio(OccultCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "secure alignment must be a power of two");
    return mmgr_confin_interim_capio_aligned(occult_bind(c), c->n, c->align);
}

/**
 * @brief Take @c n bytes that a mark release will not reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *occult_persist(OccultCtx *c)
{
    return mmgr_confin_persist_capio(occult_bind(c), c->n);
}

/**
 * @brief Where the down-growing end is now.
 * @param c In/out. The take.
 * @return The mark.
 */
MMGR_INLINE size_t occult_mark(OccultCtx *c)
{
    return mmgr_confin_interim_mark(occult_bind(c));
}

/**
 * @brief Wipe everything the tenant holds and empty it.
 * @param c In/out. The take.
 */
MMGR_INLINE void occult_reset(OccultCtx *c)
{
    mmgr_confin *a = occult_peek(c);

    if (a != NULL)
    {
        c->mark = a->size;
        occult_wipe_down_to(c);
    }
}

/**
 * @brief How much the down-growing end holds.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t occult_used(OccultCtx *c)
{
    mmgr_confin *const a = occult_peek(c);

    return (a != NULL) ? mmgr_confin_interim_used(a) : 0;
}

/**
 * @brief The most it ever held.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t occult_high_water(OccultCtx *c)
{
    mmgr_confin *const a = occult_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

/**
 * @brief Is this pointer inside the pool at all.
 * @param c In/out. The take.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool occult_owns(OccultCtx *c)
{
    return occult_offset(c) < SEC_BLOCK_BYTES;
}

void *mmgr_occult_capio(size_t n, size_t align)
{
    return MMGR_CALL(occult_capio, OccultCtx, .n = n, .align = align);
}

mmgr_spat mmgr_occult_span(size_t n, size_t align)
{
    return spat.from((uint8_t *)mmgr_occult_capio(n, align), n);
}

mmgr_spat mmgr_occult_persist_span(size_t n)
{
    return spat.from((uint8_t *)MMGR_CALL(occult_persist, OccultCtx, .n = n), n);
}

size_t mmgr_occult_mark(void)
{
    return MMGR_CALL(occult_mark, OccultCtx, .n = 0);
}

void mmgr_occult_reddo(size_t mark)
{
    MMGR_CALL(occult_wipe_down_to, OccultCtx, .mark = mark);
}

void mmgr_occult_reset(void)
{
    MMGR_CALL(occult_reset, OccultCtx, .n = 0);
}

size_t mmgr_occult_used(void)
{
    return MMGR_CALL(occult_used, OccultCtx, .n = 0);
}

size_t mmgr_occult_high_water(void)
{
    return MMGR_CALL(occult_high_water, OccultCtx, .n = 0);
}

size_t mmgr_occult_capacity(void)
{
    return MMGR_SECURE_CONFIN_SIZE;
}

mmgr_bool mmgr_occult_owns(const void *p)
{
    return MMGR_CALL(occult_owns, OccultCtx, .p = p);
}
