// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "clarus_custodiae/clarus_custodiae.h"
#include "confinium/confinium.h"

/**
 * @file clarus_custodiae.c
 * @brief The plaintext guardian. One tenant over a static buffer.
 *
 * Every entry below takes one parameter, a pointer to ClarusCtx. A take is a byte count, an
 * alignment and the pool it comes from, so they are one context.
 */

#define PLAIN_BLOCK_BYTES ((uintptr_t)MMGR_PLAINTEXT_CONFIN_SIZE)

#define PLAIN_NO_OFFSET (~(uintptr_t)0)

struct PlainStorage
{
    _Alignas(32) uint8_t mem[MMGR_PLAINTEXT_CONFIN_SIZE];
};

struct PlainInternal
{
    struct PlainStorage *store;
    mmgr_confin pool;
};

static struct PlainStorage s_storage;

struct PlainInternal mmgr_clarus_internal;

/** @brief One take, return or question of the pool. */
typedef struct
{
    struct PlainInternal *pool; /**< The pool. */
    size_t n;                   /**< Bytes wanted. */
    size_t align;               /**< Alignment wanted. */
    size_t mark;                /**< The mark being rewound to. */
    const void *p;              /**< The pointer being asked about. */
} ClarusCtx;

/**
 * @brief Point the context at the pool.
 * @param c In/out. The take.
 */
MMGR_INLINE void clarus_self(ClarusCtx *c)
{
    c->pool = &mmgr_clarus_internal;
}

/**
 * @brief The tenant, bound to its storage on first use.
 * @param c In/out. The take.
 * @return The tenant.
 */
MMGR_INLINE mmgr_confin *clarus_bind(ClarusCtx *c)
{
    clarus_self(c);

    mmgr_confin *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_storage;
        mmgr_confin_init(a, c->pool->store->mem, MMGR_PLAINTEXT_CONFIN_SIZE);
    }
    return a;
}

/**
 * @brief The tenant, if it has been bound.
 * @param c In/out. The take.
 * @return The tenant, or NULL.
 */
MMGR_INLINE mmgr_confin *clarus_peek(ClarusCtx *c)
{
    clarus_self(c);

    mmgr_confin *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

/**
 * @brief Offset of @c p into the pool's store.
 * @param c In/out. The take.
 * @return The offset, or PLAIN_NO_OFFSET when the pool has no store.
 */
MMGR_INLINE uintptr_t clarus_offset(ClarusCtx *c)
{
    clarus_self(c);

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
MMGR_INLINE void *clarus_capio(ClarusCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "plaintext alignment must be a power of two");
    return mmgr_confin_interim_capio_aligned(clarus_bind(c), c->n, c->align);
}

/**
 * @brief Take @c n bytes that a mark release will not reclaim.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the tenant is full.
 */
MMGR_INLINE void *clarus_persist(ClarusCtx *c)
{
    return mmgr_confin_persist_capio(clarus_bind(c), c->n);
}

/**
 * @brief Release everything the tenant holds.
 * @param c In/out. The take.
 */
MMGR_INLINE void clarus_reset(ClarusCtx *c)
{
    mmgr_confin *a = clarus_peek(c);

    if (a != NULL)
    {
        mmgr_confin_interim_reset(a);
    }
}

/**
 * @brief Where the down-growing end is now.
 * @param c In/out. The take.
 * @return The mark.
 */
MMGR_INLINE size_t clarus_mark(ClarusCtx *c)
{
    return mmgr_confin_interim_mark(clarus_bind(c));
}

/**
 * @brief Wind back to where the mark was taken.
 * @param c In/out. The take.
 */
MMGR_INLINE void clarus_reddo(ClarusCtx *c)
{
    mmgr_confin_interim_reddo(clarus_bind(c), c->mark);
}

/**
 * @brief How much the down-growing end holds.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t clarus_used(ClarusCtx *c)
{
    mmgr_confin *const a = clarus_peek(c);

    return (a != NULL) ? mmgr_confin_interim_used(a) : 0;
}

/**
 * @brief The most it ever held.
 * @param c In/out. The take.
 * @return Byte count.
 */
MMGR_INLINE size_t clarus_high_water(ClarusCtx *c)
{
    mmgr_confin *const a = clarus_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

/**
 * @brief Is this pointer inside the pool at all.
 * @param c In/out. The take.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool clarus_owns(ClarusCtx *c)
{
    return clarus_offset(c) < PLAIN_BLOCK_BYTES;
}

void *mmgr_clarus_capio(size_t n, size_t align)
{
    return MMGR_CALL(clarus_capio, ClarusCtx, .n = n, .align = align);
}

mmgr_spat mmgr_clarus_span(size_t n, size_t align)
{
    return spat.from((uint8_t *)mmgr_clarus_capio(n, align), n);
}

mmgr_spat mmgr_clarus_persist_span(size_t n)
{
    return spat.from((uint8_t *)MMGR_CALL(clarus_persist, ClarusCtx, .n = n), n);
}

void mmgr_clarus_reset(void)
{
    MMGR_CALL(clarus_reset, ClarusCtx, .n = 0);
}

size_t mmgr_clarus_mark(void)
{
    return MMGR_CALL(clarus_mark, ClarusCtx, .n = 0);
}

void mmgr_clarus_reddo(size_t mark)
{
    MMGR_CALL(clarus_reddo, ClarusCtx, .mark = mark);
}

size_t mmgr_clarus_used(void)
{
    return MMGR_CALL(clarus_used, ClarusCtx, .n = 0);
}

size_t mmgr_clarus_high_water(void)
{
    return MMGR_CALL(clarus_high_water, ClarusCtx, .n = 0);
}

size_t mmgr_clarus_capacity(void)
{
    return MMGR_PLAINTEXT_CONFIN_SIZE;
}

mmgr_bool mmgr_clarus_owns(const void *p)
{
    return MMGR_CALL(clarus_owns, ClarusCtx, .p = p);
}
