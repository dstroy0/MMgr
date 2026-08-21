// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "occultum_custodiae/occultum_custodiae.h"
#include "confinium/confinium.h"

/**
 * @file occultum_custodiae.c
 * @brief The secure guardian. Same shape as clarus, but released bytes are wiped.
 */

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

/**
 * @brief The pool.
 * @return Pointer to it.
 */
static inline struct SecureInternal *secure_ctx(void)
{
    return &mmgr_occult_state;
}

/**
 * @brief Offset of @p p into the pool's store.
 * @param ctx Pool.
 * @param p Pointer.
 * @return The offset, or the no-offset sentinel when the pool has no store.
 */
static inline uintptr_t secure_offset(const struct SecureInternal *ctx, const void *p)
{
    return (uintptr_t)p - (uintptr_t)ctx->store;
}

static inline mmgr_confin *bind(struct SecureInternal *ctx)
{
    mmgr_confin *a = &ctx->pool;
    if (a->base == NULL)
    {
        ctx->store = &s_store;
        mmgr_confin_init(a, ctx->store->mem, MMGR_SECURE_CONFIN_SIZE);
    }
    return a;
}
/**
 * @brief The tenant, if it has been bound.
 * @param ctx Pool.
 * @return The tenant, or NULL.
 */
static inline mmgr_confin *peek(struct SecureInternal *ctx)
{
    mmgr_confin *a = &ctx->pool;
    return (a->base != NULL) ? a : NULL;
}
/**
 * @brief Release interim back to @p mark, wiping what is given up.
 * @param a Tenant.
 * @param mark Where to release to.
 *
 * This is what makes the secure pool secure. Releasing a mark hands the bytes back for reuse, so
 * they are zeroed before the fill point moves.
 */
static inline void wipe_down_to(mmgr_confin *a, size_t mark)
{
    const size_t top = mmgr_confin_interim_mark(a);
    if (mark > top && mark <= a->size)
    {
        mmgr_occult_wipe(a->base + top, mark - top);
    }
    mmgr_confin_interim_reddo(a, mark);
}

void *mmgr_occult_capio(size_t n, size_t align)
{
    struct SecureInternal *ctx = secure_ctx();
    MMGR_ASSERT((align & (align - 1)) == 0, "secure alignment must be a power of two");
    return mmgr_confin_interim_capio_aligned(bind(ctx), n, align);
}

mmgr_spat mmgr_occult_span(size_t n, size_t align)
{
    return spat.from((uint8_t *)mmgr_occult_capio(n, align), n);
}

mmgr_spat mmgr_occult_persist_span(size_t n)
{
    struct SecureInternal *ctx = secure_ctx();

    return spat.from((uint8_t *)mmgr_confin_persist_capio(bind(ctx), n), n);
}

size_t mmgr_occult_mark(void)
{
    struct SecureInternal *ctx = secure_ctx();
    return mmgr_confin_interim_mark(bind(ctx));
}

void mmgr_occult_reddo(size_t mark)
{
    struct SecureInternal *ctx = secure_ctx();
    wipe_down_to(bind(ctx), mark);
}

void mmgr_occult_reset(void)
{
    struct SecureInternal *ctx = secure_ctx();
    mmgr_confin *a = peek(ctx);
    if (a != NULL)
    {
        wipe_down_to(a, a->size);
    }
}

size_t mmgr_occult_used(void)
{
    const mmgr_confin *a = peek(secure_ctx());
    return (a != NULL) ? mmgr_confin_interim_used(a) : 0;
}

size_t mmgr_occult_high_water(void)
{
    const mmgr_confin *a = peek(secure_ctx());
    return (a != NULL) ? a->scratch_hw : 0;
}

size_t mmgr_occult_capacity(void)
{
    return MMGR_SECURE_CONFIN_SIZE;
}

mmgr_bool mmgr_occult_owns(const void *p)
{
    const struct SecureInternal *ctx = secure_ctx();

    return ctx->store != NULL && secure_offset(ctx, p) < (uintptr_t)sizeof(ctx->store->mem);
}

