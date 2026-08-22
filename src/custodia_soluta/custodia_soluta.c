// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "custodia_soluta/custodia_soluta.h"
#include "carceribus/carceribus.h"

/**
 * @file custodia_soluta.c
 * @brief The plaintext guardian. One tenant over a static buffer.
 *
 * Every entry below takes one parameter, a pointer to SolutaCtx. A take is a byte count, an
 * alignment and the pool it comes from, so they are one context.
 */

#define PLAIN_BLOCK_BYTES ((uintptr_t)MMGR_PLAINTEXT_CONFIN_SIZE)

#define PLAIN_NO_OFFSET (~(uintptr_t)0)

struct PlainStorage
{
    _Alignas(32) uint8_t mem[MMGR_PLAINTEXT_CONFIN_SIZE];
};

struct SolutaInternal
{
    struct PlainStorage *store;
    mmgr_carcer pool;
};

static struct PlainStorage s_storage;

struct SolutaInternal mmgr_soluta_internal;

/** @brief One take, return or question of the pool. */
typedef struct
{
    struct SolutaInternal *pool; /**< The pool. */
    size_t n;                   /**< Bytes wanted. */
    size_t align;               /**< Alignment wanted. */
    size_t mark;                /**< The mark being rewound to. */
    const void *p;              /**< The pointer being asked about. */
} SolutaCtx;

/**
 * @brief Point the context at the pool.
 * @param c In/out. The take.
 */
MMGR_INLINE void soluta_self(SolutaCtx *c)
{
    c->pool = &mmgr_soluta_internal;
}

/**
 * @brief The tenant, bound to its storage on first use.
 * @param c In/out. The take.
 * @return The tenant.
 */
MMGR_INLINE mmgr_carcer *soluta_bind(SolutaCtx *c)
{
    soluta_self(c);

    mmgr_carcer *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_storage;
        mmgr_carcer_init(a, c->pool->store->mem, MMGR_PLAINTEXT_CONFIN_SIZE);
    }
    return a;
}

/**
 * @brief The tenant, if it has been bound.
 * @param c In/out. The take.
 * @return The tenant, or NULL.
 */
MMGR_INLINE mmgr_carcer *soluta_peek(SolutaCtx *c)
{
    soluta_self(c);

    mmgr_carcer *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

/**
 * @brief Offset of @c p into the pool's store.
 * @param c In/out. The take.
 * @return The offset, or PLAIN_NO_OFFSET when the pool has no store.
 */
MMGR_INLINE uintptr_t soluta_offset(SolutaCtx *c)
{
    soluta_self(c);

    if (c->pool->store == NULL)
    {
        return PLAIN_NO_OFFSET;
    }
    return (uintptr_t)c->p - (uintptr_t)c->pool->store->mem;
}

/**
 * @brief Take @c n bytes that a mark release will reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *soluta_capio(SolutaCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "plaintext alignment must be a power of two");
    return mmgr_carcer_interim_capio_aligned(soluta_bind(c), c->n, c->align);
}

/**
 * @brief Take @c n bytes that a mark release will not reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *soluta_persist(SolutaCtx *c)
{
    return mmgr_carcer_persist_capio(soluta_bind(c), c->n);
}

/**
 * @brief Release everything the tenant holds.
 * @param c In/out. The take.
 */
MMGR_INLINE void soluta_reset(SolutaCtx *c)
{
    mmgr_carcer *a = soluta_peek(c);

    if (a != NULL)
    {
        mmgr_carcer_interim_reset(a);
    }
}

/**
 * @brief Where the down-growing end is now.
 * @param c In/out. The take.
 * @return The mark.
 */
MMGR_INLINE size_t soluta_mark(SolutaCtx *c)
{
    return mmgr_carcer_interim_mark(soluta_bind(c));
}

/**
 * @brief Wind back to where the mark was taken.
 * @param c In/out. The take.
 */
MMGR_INLINE void soluta_reddo(SolutaCtx *c)
{
    mmgr_carcer_interim_reddo(soluta_bind(c), c->mark);
}

/**
 * @brief How much the down-growing end holds.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t soluta_used(SolutaCtx *c)
{
    mmgr_carcer *const a = soluta_peek(c);

    return (a != NULL) ? mmgr_carcer_interim_used(a) : 0;
}

/**
 * @brief The most it ever held.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t soluta_high_water(SolutaCtx *c)
{
    mmgr_carcer *const a = soluta_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

/**
 * @brief Is this pointer inside the pool at all.
 * @param c In/out. The take.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool soluta_owns(SolutaCtx *c)
{
    return soluta_offset(c) < PLAIN_BLOCK_BYTES;
}

void *mmgr_soluta_capio(size_t n, size_t align)
{
    return MMGR_CALL(soluta_capio, SolutaCtx, .n = n, .align = align);
}

mmgr_spat mmgr_soluta_span(size_t n, size_t align)
{
    return spat.init(&(SpatCfg){(uint8_t *)mmgr_soluta_capio(n, align), n});
}

mmgr_spat mmgr_soluta_persist_span(size_t n)
{
    return spat.init(&(SpatCfg){(uint8_t *)MMGR_CALL(soluta_persist, SolutaCtx, .n = n), n});
}

void mmgr_soluta_reset(void)
{
    MMGR_CALL(soluta_reset, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_mark(void)
{
    return MMGR_CALL(soluta_mark, SolutaCtx, .n = 0);
}

void mmgr_soluta_reddo(size_t mark)
{
    MMGR_CALL(soluta_reddo, SolutaCtx, .mark = mark);
}

size_t mmgr_soluta_used(void)
{
    return MMGR_CALL(soluta_used, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_high_water(void)
{
    return MMGR_CALL(soluta_high_water, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_capacity(void)
{
    return MMGR_PLAINTEXT_CONFIN_SIZE;
}

mmgr_bool mmgr_soluta_owns(const void *p)
{
    return MMGR_CALL(soluta_owns, SolutaCtx, .p = p);
}
