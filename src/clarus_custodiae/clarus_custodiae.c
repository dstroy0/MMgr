// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "clarus_custodiae/clarus_custodiae.h"
#include "confinium/confinium.h"

/**
 * @file clarus_custodiae.c
 * @brief The plaintext guardian. One tenant over a static buffer.
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

/**
 * @brief The pool.
 * @return Pointer to it.
 */
static inline struct PlainInternal *plain_self(void)
{
    return &mmgr_clarus_internal;
}

/**
 * @brief Offset of @p p into the pool's store.
 * @param ctx Pool.
 * @param p Pointer.
 * @return The offset, or PLAIN_NO_OFFSET when the pool has no store.
 */
static inline uintptr_t plain_offset(const struct PlainInternal *ctx, const void *p)
{
    if (ctx->store == NULL)
    {
        return PLAIN_NO_OFFSET;
    }
    return (uintptr_t)p - (uintptr_t)ctx->store->mem;
}

static inline mmgr_confin *bind(struct PlainInternal *ctx)
{
    mmgr_confin *a = &ctx->pool;
    if (a->base == NULL)
    {
        ctx->store = &s_storage;
        mmgr_confin_init(a, ctx->store->mem, MMGR_PLAINTEXT_CONFIN_SIZE);
    }
    return a;
}
/**
 * @brief The tenant, if it has been bound.
 * @param ctx Pool.
 * @return The tenant, or NULL.
 */
static inline mmgr_confin *peek(struct PlainInternal *ctx)
{
    mmgr_confin *a = &ctx->pool;
    return (a->base != NULL) ? a : NULL;
}
void *mmgr_clarus_capio(size_t n, size_t align)
{
    struct PlainInternal *ctx = plain_self();

    MMGR_ASSERT((align & (align - 1)) == 0, "plaintext alignment must be a power of two");
    return mmgr_confin_interim_capio_aligned(bind(ctx), n, align);
}

mmgr_spat mmgr_clarus_span(size_t n, size_t align)
{

    return spat.from((uint8_t *)mmgr_clarus_capio(n, align), n);
}

mmgr_spat mmgr_clarus_persist_span(size_t n)
{
    struct PlainInternal *ctx = plain_self();

    return spat.from((uint8_t *)mmgr_confin_persist_capio(bind(ctx), n), n);
}

void mmgr_clarus_reset(void)
{
    struct PlainInternal *ctx = plain_self();
    mmgr_confin *a = peek(ctx);
    if (a != NULL)
    {
        mmgr_confin_interim_reset(a);
    }
}

size_t mmgr_clarus_mark(void)
{
    struct PlainInternal *ctx = plain_self();
    return mmgr_confin_interim_mark(bind(ctx));
}

void mmgr_clarus_reddo(size_t mark)
{
    struct PlainInternal *ctx = plain_self();
    mmgr_confin_interim_reddo(bind(ctx), mark);
}

size_t mmgr_clarus_used(void)
{
    struct PlainInternal *ctx = plain_self();
    const mmgr_confin *a = peek(ctx);
    return (a != NULL) ? mmgr_confin_interim_used(a) : 0;
}

size_t mmgr_clarus_high_water(void)
{
    const mmgr_confin *a = peek(plain_self());
    return (a != NULL) ? a->scratch_hw : 0;
}

size_t mmgr_clarus_capacity(void)
{
    return MMGR_PLAINTEXT_CONFIN_SIZE;
}

mmgr_bool mmgr_clarus_owns(const void *p)
{
    return plain_offset(plain_self(), p) < PLAIN_BLOCK_BYTES;
}

