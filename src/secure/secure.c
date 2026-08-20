// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/secure/secure.h"
#include "config/platform/platform.h"
#include "mmgr/arena/arena.h"
#include <assert.h>

struct SecureStorage
{
    _Alignas(32) uint8_t mem[MMGR_SEC_POOL_SLOTS][MMGR_SECURE_ARENA_SIZE];
};

struct SecureInternal
{
    struct SecureStorage *store;
    mmgr_arena pool[MMGR_SEC_POOL_SLOTS];
#if MMGR_DEBUG_CHECKS
    uintptr_t owner[MMGR_SEC_POOL_SLOTS];
#endif
};

static struct SecureStorage s_store;

struct SecureInternal mmgr_secure_state;

static inline struct SecureInternal *secure_ctx(void)
{
    return &mmgr_secure_state;
}

static inline uintptr_t secure_offset(const struct SecureInternal *ctx, const void *p)
{
    return (uintptr_t)p - (uintptr_t)ctx->store;
}

static inline int cur_worker(void)
{
    int w = mmgr_worker_self();
    return (w >= 0 && w < MMGR_SEC_POOL_SLOTS) ? w : MMGR_GHOST_WORKER_SLOT;
}

static inline void assert_single_owner(struct SecureInternal *ctx, int w)
{
#if MMGR_DEBUG_CHECKS

    const uintptr_t cur = mmgr_platform_context_id();
    if (ctx->owner[w] == 0)
    {
        ctx->owner[w] = cur;
    }
    else
    {
        assert(ctx->owner[w] == cur && "secure pool borrowed from a foreign task");
    }
#else
    (void)ctx;
    (void)w;
#endif
}

static inline mmgr_arena *bind(struct SecureInternal *ctx, int w)
{
    mmgr_arena *a = &ctx->pool[w];
    if (a->base == NULL)
    {
        ctx->store = &s_store;
        mmgr_arena_init(a, ctx->store->mem[w], MMGR_SECURE_ARENA_SIZE);
    }
    return a;
}

static inline mmgr_arena *peek(struct SecureInternal *ctx, int w)
{
    mmgr_arena *a = &ctx->pool[w];
    return (a->base != NULL) ? a : NULL;
}

static inline void wipe_down_to(mmgr_arena *a, size_t mark)
{
    const size_t top = mmgr_arena_scratch_mark(a);
    if (mark > top && mark <= a->size)
    {
        mmgr_secure_wipe(a->base + top, mark - top);
    }
    mmgr_arena_scratch_release(a, mark);
}

void *mmgr_secure_alloc(size_t n, size_t align)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    assert((align & (align - 1)) == 0 && "secure alignment must be a power of two");
    return mmgr_arena_scratch_alloc_aligned(bind(ctx, w), n, align);
}

mmgr_span mmgr_secure_span(size_t n, size_t align)
{
    return mmgr_span_from((uint8_t *)mmgr_secure_alloc(n, align), n);
}

mmgr_span mmgr_secure_persist_span(size_t n)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);

    return mmgr_span_from((uint8_t *)mmgr_arena_persist_alloc(bind(ctx, w), n), n);
}

size_t mmgr_secure_mark(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    return mmgr_arena_scratch_mark(bind(ctx, w));
}

void mmgr_secure_release(size_t mark)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    wipe_down_to(bind(ctx, w), mark);
}

void mmgr_secure_reset(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    mmgr_arena *a = peek(ctx, w);
    if (a != NULL)
    {
        wipe_down_to(a, a->size);
    }
}

size_t mmgr_secure_used(void)
{
    const mmgr_arena *a = peek(secure_ctx(), cur_worker());
    return (a != NULL) ? mmgr_arena_scratch_used(a) : 0;
}

size_t mmgr_secure_high_water(void)
{
    struct SecureInternal *ctx = secure_ctx();
    size_t peak = 0;
    for (int w = 0; w < MMGR_SEC_POOL_SLOTS; w++)
    {
        const mmgr_arena *a = peek(ctx, w);
        if (a != NULL && a->scratch_hw > peak)
        {
            peak = a->scratch_hw;
        }
    }
    return peak;
}

size_t mmgr_secure_capacity(void)
{
    return MMGR_SECURE_ARENA_SIZE;
}

mmgr_bool mmgr_secure_owns(const void *p)
{
    const struct SecureInternal *ctx = secure_ctx();

    return ctx->store != NULL && secure_offset(ctx, p) < (uintptr_t)sizeof(ctx->store->mem);
}

int mmgr_secure_slot_of(const void *p)
{
    const struct SecureInternal *ctx = secure_ctx();
    if (ctx->store == NULL)
    {
        return -1;
    }
    const uintptr_t off = secure_offset(ctx, p);
    if (off >= (uintptr_t)sizeof(ctx->store->mem))
    {
        return -1;
    }

    return (int)(off / MMGR_SECURE_ARENA_SIZE);
}
