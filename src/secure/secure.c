// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/secure/secure.h"
#include "config/platform/platform.h"
#include "mmgr/arena/arena.h"
#include <assert.h>

struct SecureStorage
{
    _Alignas(32) uint8_t mem[PROTOCORE_SEC_POOL_SLOTS][PROTOCORE_SECURE_ARENA_SIZE];
};

struct SecureInternal
{
    struct SecureStorage *store;
    protocore_arena pool[PROTOCORE_SEC_POOL_SLOTS];
#if PROTOCORE_DEBUG_CHECKS
    uintptr_t owner[PROTOCORE_SEC_POOL_SLOTS];
#endif
};

static struct SecureStorage s_store;

struct SecureInternal protocore_secure_state;

static inline struct SecureInternal *secure_ctx(void)
{
    return &protocore_secure_state;
}

static inline uintptr_t secure_offset(const struct SecureInternal *ctx, const void *p)
{
    return (uintptr_t)p - (uintptr_t)ctx->store;
}

static inline int cur_worker(void)
{
    int w = protocore_worker_self();
    return (w >= 0 && w < PROTOCORE_SEC_POOL_SLOTS) ? w : PROTOCORE_GHOST_WORKER_SLOT;
}

static inline void assert_single_owner(struct SecureInternal *ctx, int w)
{
#if PROTOCORE_DEBUG_CHECKS

    const uintptr_t cur = protocore_platform_context_id();
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

static inline protocore_arena *bind(struct SecureInternal *ctx, int w)
{
    protocore_arena *a = &ctx->pool[w];
    if (a->base == NULL)
    {
        ctx->store = &s_store;
        protocore_arena_init(a, ctx->store->mem[w], PROTOCORE_SECURE_ARENA_SIZE);
    }
    return a;
}

static inline protocore_arena *peek(struct SecureInternal *ctx, int w)
{
    protocore_arena *a = &ctx->pool[w];
    return (a->base != NULL) ? a : NULL;
}

static inline void wipe_down_to(protocore_arena *a, size_t mark)
{
    const size_t top = protocore_arena_scratch_mark(a);
    if (mark > top && mark <= a->size)
    {
        protocore_secure_wipe(a->base + top, mark - top);
    }
    protocore_arena_scratch_release(a, mark);
}

void *protocore_secure_alloc(size_t n, size_t align)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    assert((align & (align - 1)) == 0 && "secure alignment must be a power of two");
    return protocore_arena_scratch_alloc_aligned(bind(ctx, w), n, align);
}

protocore_span protocore_secure_span(size_t n, size_t align)
{
    return protocore_span_from((uint8_t *)protocore_secure_alloc(n, align), n);
}

protocore_span protocore_secure_persist_span(size_t n)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);

    return protocore_span_from((uint8_t *)protocore_arena_persist_alloc(bind(ctx, w), n), n);
}

size_t protocore_secure_mark(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    return protocore_arena_scratch_mark(bind(ctx, w));
}

void protocore_secure_release(size_t mark)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    wipe_down_to(bind(ctx, w), mark);
}

void protocore_secure_reset(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    protocore_arena *a = peek(ctx, w);
    if (a != NULL)
    {
        wipe_down_to(a, a->size);
    }
}

size_t protocore_secure_used(void)
{
    const protocore_arena *a = peek(secure_ctx(), cur_worker());
    return (a != NULL) ? protocore_arena_scratch_used(a) : 0;
}

size_t protocore_secure_high_water(void)
{
    struct SecureInternal *ctx = secure_ctx();
    size_t peak = 0;
    for (int w = 0; w < PROTOCORE_SEC_POOL_SLOTS; w++)
    {
        const protocore_arena *a = peek(ctx, w);
        if (a != NULL && a->scratch_hw > peak)
        {
            peak = a->scratch_hw;
        }
    }
    return peak;
}

size_t protocore_secure_capacity(void)
{
    return PROTOCORE_SECURE_ARENA_SIZE;
}

proto_bool protocore_secure_owns(const void *p)
{
    const struct SecureInternal *ctx = secure_ctx();

    return ctx->store != NULL && secure_offset(ctx, p) < (uintptr_t)sizeof(ctx->store->mem);
}

int protocore_secure_slot_of(const void *p)
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

    return (int)(off / PROTOCORE_SECURE_ARENA_SIZE);
}
