// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "custodia_secura/custodia_secura.h"
#include "carceribus/carceribus.h"

/**
 * @file occultum_custodiae.c
 * @brief The secure guardian. Same shape as custodia_soluta, but released bytes are wiped.
 *
 * Every entry below takes one parameter, a pointer to SecuraCtx. A take is a byte count, an
 * alignment and the pool it comes from, so they are one context.
 */

#define SEC_BLOCK_BYTES ((uintptr_t)MMGR_SECURE_CONFIN_SIZE)

#define SEC_NO_OFFSET (~(uintptr_t)0)

struct SecureStorage
{
    _Alignas(32) uint8_t mem[MMGR_SECURE_CONFIN_SIZE];
};

struct SecuraInternal
{
    struct SecureStorage *store;
    mmgr_carcer pool;
};

static struct SecureStorage s_store;

struct SecuraInternal mmgr_secura_state;

/** @brief One take, return or question of the pool. */
typedef struct
{
    struct SecuraInternal *pool; /**< The pool. */
    size_t n;                    /**< Bytes wanted. */
    size_t align;                /**< Alignment wanted. */
    size_t mark;                 /**< The mark being rewound to. */
    const void *p;               /**< The pointer being asked about. */
} SecuraCtx;

/**
 * @brief Point the context at the pool.
 * @param c In/out. The take.
 */
MMGR_INLINE void secura_self(SecuraCtx *c)
{
    c->pool = &mmgr_secura_state;
}

/**
 * @brief The tenant, bound to its storage on first use.
 * @param c In/out. The take.
 * @return The tenant.
 */
MMGR_INLINE mmgr_carcer *secura_bind(SecuraCtx *c)
{
    secura_self(c);

    mmgr_carcer *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_store;
        mmgr_carcer_init(a, c->pool->store->mem, MMGR_SECURE_CONFIN_SIZE);
    }
    return a;
}

/**
 * @brief The tenant, if it has been bound.
 * @param c In/out. The take.
 * @return The tenant, or NULL.
 */
MMGR_INLINE mmgr_carcer *secura_peek(SecuraCtx *c)
{
    secura_self(c);

    mmgr_carcer *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

/**
 * @brief Offset of @c p into the pool's store.
 * @param c In/out. The take.
 * @return The offset, or SEC_NO_OFFSET when the pool has no store.
 */
MMGR_INLINE uintptr_t secura_offset(SecuraCtx *c)
{
    secura_self(c);

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
MMGR_INLINE void secura_wipe_down_to(SecuraCtx *c)
{
    mmgr_carcer *a = secura_bind(c);
    const size_t top = mmgr_carcer_interim_mark(a);

    if ((c->mark > top) && (c->mark <= a->size))
    {
        mmgr_secura_wipe(a->base + top, c->mark - top);
    }
    mmgr_carcer_interim_reddo(a, c->mark);
}

/**
 * @brief Take @c n bytes that a mark release will reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *secura_capio(SecuraCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "secure alignment must be a power of two");
    return mmgr_carcer_interim_capio_aligned(secura_bind(c), c->n, c->align);
}

/**
 * @brief Take @c n bytes that a mark release will not reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *secura_persist(SecuraCtx *c)
{
    return mmgr_carcer_persist_capio(secura_bind(c), c->n);
}

/**
 * @brief Where the down-growing end is now.
 * @param c In/out. The take.
 * @return The mark.
 */
MMGR_INLINE size_t secura_mark(SecuraCtx *c)
{
    return mmgr_carcer_interim_mark(secura_bind(c));
}

/**
 * @brief Wipe everything the tenant holds and empty it.
 * @param c In/out. The take.
 */
MMGR_INLINE void secura_reset(SecuraCtx *c)
{
    mmgr_carcer *a = secura_peek(c);

    if (a != NULL)
    {
        c->mark = a->size;
        secura_wipe_down_to(c);
    }
}

/**
 * @brief How much the down-growing end holds.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t secura_used(SecuraCtx *c)
{
    mmgr_carcer *const a = secura_peek(c);

    return (a != NULL) ? mmgr_carcer_interim_used(a) : 0;
}

/**
 * @brief The most it ever held.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t secura_high_water(SecuraCtx *c)
{
    mmgr_carcer *const a = secura_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

/**
 * @brief Is this pointer inside the pool at all.
 * @param c In/out. The take.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool secura_owns(SecuraCtx *c)
{
    return secura_offset(c) < SEC_BLOCK_BYTES;
}

void *mmgr_secura_capio(size_t n, size_t align)
{
    return MMGR_CALL(secura_capio, SecuraCtx, .n = n, .align = align);
}

mmgr_spat mmgr_secura_span(size_t n, size_t align)
{
    return spat.init(&(SpatCfg){(uint8_t *)mmgr_secura_capio(n, align), n});
}

mmgr_spat mmgr_secura_persist_span(size_t n)
{
    return spat.init(&(SpatCfg){(uint8_t *)MMGR_CALL(secura_persist, SecuraCtx, .n = n), n});
}

size_t mmgr_secura_mark(void)
{
    return MMGR_CALL(secura_mark, SecuraCtx, .n = 0);
}

void mmgr_secura_reddo(size_t mark)
{
    MMGR_CALL(secura_wipe_down_to, SecuraCtx, .mark = mark);
}

void mmgr_secura_reset(void)
{
    MMGR_CALL(secura_reset, SecuraCtx, .n = 0);
}

size_t mmgr_secura_used(void)
{
    return MMGR_CALL(secura_used, SecuraCtx, .n = 0);
}

size_t mmgr_secura_high_water(void)
{
    return MMGR_CALL(secura_high_water, SecuraCtx, .n = 0);
}

size_t mmgr_secura_capacity(void)
{
    return MMGR_SECURE_CONFIN_SIZE;
}

mmgr_bool mmgr_secura_owns(const void *p)
{
    return MMGR_CALL(secura_owns, SecuraCtx, .p = p);
}
