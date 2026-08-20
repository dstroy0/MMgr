// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/plaintext/plaintext.h"
#include "mmgr/arena/arena.h"

#define PLAIN_BLOCK_BYTES ((uintptr_t)MMGR_REG_POOL_SLOTS * (uintptr_t)MMGR_PLAINTEXT_ARENA_SIZE)

#define PLAIN_NO_OFFSET (~(uintptr_t)0)

struct PlainStorage
{

    _Alignas(32) uint8_t mem[MMGR_REG_POOL_SLOTS][MMGR_PLAINTEXT_ARENA_SIZE];
};

struct PlainInternal
{
    struct PlainStorage *store;
    mmgr_arena pool[MMGR_REG_POOL_SLOTS];
#if MMGR_DEBUG_CHECKS

    uintptr_t owner[MMGR_REG_POOL_SLOTS];
#endif
};

static struct PlainStorage s_storage;

struct PlainInternal mmgr_plaintext_internal;

static inline struct PlainInternal *plain_self(void)
{
    return &mmgr_plaintext_internal;
}

static inline uintptr_t plain_offset(const struct PlainInternal *ctx, const void *p)
{
    if (ctx->store == NULL)
    {
        return PLAIN_NO_OFFSET;
    }
    return (uintptr_t)p - (uintptr_t)ctx->store->mem;
}

static inline int cur_worker(void)
{
    int w = mmgr_worker_self();
    return (w >= 0 && w < MMGR_REG_POOL_SLOTS) ? w : MMGR_GHOST_WORKER_SLOT;
}

static inline void assert_single_owner(struct PlainInternal *ctx, int w)
{
#if MMGR_DEBUG_CHECKS
    const uintptr_t cur = mmgr_platform_context_id();
    if (ctx->owner[w] == 0)
    {
        ctx->owner[w] = cur;
    }
    else
    {
        MMGR_ASSERT(ctx->owner[w] == cur, "plaintext pool borrowed from a foreign task");
    }
#else
    (void)ctx;
    (void)w;
#endif
}

static inline mmgr_arena *bind(struct PlainInternal *ctx, int w)
{
    mmgr_arena *a = &ctx->pool[w];
    if (a->base == NULL)
    {
        ctx->store = &s_storage;
        mmgr_arena_init(a, ctx->store->mem[w], MMGR_PLAINTEXT_ARENA_SIZE);
    }
    return a;
}

static inline mmgr_arena *peek(struct PlainInternal *ctx, int w)
{
    mmgr_arena *a = &ctx->pool[w];
    return (a->base != NULL) ? a : NULL;
}

void *mmgr_plaintext_alloc(size_t n, size_t align)
{
    struct PlainInternal *ctx = plain_self();
    int w = cur_worker();
    assert_single_owner(ctx, w);

    MMGR_ASSERT((align & (align - 1)) == 0, "plaintext alignment must be a power of two");
    return mmgr_arena_scratch_alloc_aligned(bind(ctx, w), n, align);
}

mmgr_span mmgr_plaintext_span(size_t n, size_t align)
{

    return mmgr_span_from((uint8_t *)mmgr_plaintext_alloc(n, align), n);
}

mmgr_span mmgr_plaintext_persist_span(size_t n)
{
    struct PlainInternal *ctx = plain_self();
    int w = cur_worker();
    assert_single_owner(ctx, w);

    return mmgr_span_from((uint8_t *)mmgr_arena_persist_alloc(bind(ctx, w), n), n);
}

void mmgr_plaintext_reset(void)
{
    struct PlainInternal *ctx = plain_self();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    mmgr_arena *a = peek(ctx, w);
    if (a != NULL)
    {
        mmgr_arena_scratch_reset(a);
    }
}

size_t mmgr_plaintext_mark(void)
{
    struct PlainInternal *ctx = plain_self();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    return mmgr_arena_scratch_mark(bind(ctx, w));
}

void mmgr_plaintext_release(size_t mark)
{
    struct PlainInternal *ctx = plain_self();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    mmgr_arena_scratch_release(bind(ctx, w), mark);
}

size_t mmgr_plaintext_used(void)
{
    struct PlainInternal *ctx = plain_self();
    const mmgr_arena *a = peek(ctx, cur_worker());
    return (a != NULL) ? mmgr_arena_scratch_used(a) : 0;
}

size_t mmgr_plaintext_high_water(void)
{

    struct PlainInternal *ctx = plain_self();
    size_t peak = 0;
    for (int w = 0; w < MMGR_REG_POOL_SLOTS; w++)
    {
        const mmgr_arena *a = peek(ctx, w);
        if (a != NULL && a->scratch_hw > peak)
        {
            peak = a->scratch_hw;
        }
    }
    return peak;
}

size_t mmgr_plaintext_capacity(void)
{
    return MMGR_PLAINTEXT_ARENA_SIZE;
}

mmgr_bool mmgr_plaintext_owns(const void *p)
{
    return plain_offset(plain_self(), p) < PLAIN_BLOCK_BYTES;
}

int mmgr_plaintext_slot_of(const void *p)
{
    const uintptr_t off = plain_offset(plain_self(), p);
    if (off >= PLAIN_BLOCK_BYTES)
    {
        return -1;
    }

    return (int)(off / MMGR_PLAINTEXT_ARENA_SIZE);
}
