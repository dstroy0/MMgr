// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/occultum_custodiae/occultum_custodiae.h"
#include "mmgr/confinium/confinium.h"

struct SecureStorage
{
    _Alignas(32) uint8_t mem[MMGR_SEC_POOL_SLOTS][MMGR_SECURE_ARENA_SIZE];
};

struct SecureInternal
{
    struct SecureStorage *store;
    mmgr_confin pool[MMGR_SEC_POOL_SLOTS];
#if MMGR_DEBUG_CHECKS
    uintptr_t owner[MMGR_SEC_POOL_SLOTS];
#endif
};

static struct SecureStorage s_store;

struct SecureInternal mmgr_occult_state;

static inline struct SecureInternal *secure_ctx(void)
{
    return &mmgr_occult_state;
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
        MMGR_ASSERT(ctx->owner[w] == cur, "secure pool borrowed from a foreign task");
    }
#else
    (void)ctx;
    (void)w;
#endif
}

static inline mmgr_confin *bind(struct SecureInternal *ctx, int w)
{
    mmgr_confin *a = &ctx->pool[w];
    if (a->base == NULL)
    {
        ctx->store = &s_store;
        mmgr_confin_init(a, ctx->store->mem[w], MMGR_SECURE_ARENA_SIZE);
    }
    return a;
}

static inline mmgr_confin *peek(struct SecureInternal *ctx, int w)
{
    mmgr_confin *a = &ctx->pool[w];
    return (a->base != NULL) ? a : NULL;
}

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
    int w = cur_worker();
    assert_single_owner(ctx, w);
    MMGR_ASSERT((align & (align - 1)) == 0, "secure alignment must be a power of two");
    return mmgr_confin_interim_capio_aligned(bind(ctx, w), n, align);
}

mmgr_spat mmgr_occult_span(size_t n, size_t align)
{
    return mmgr_spat_from((uint8_t *)mmgr_occult_capio(n, align), n);
}

mmgr_spat mmgr_occult_persist_span(size_t n)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);

    return mmgr_spat_from((uint8_t *)mmgr_confin_persist_capio(bind(ctx, w), n), n);
}

size_t mmgr_occult_mark(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    return mmgr_confin_interim_mark(bind(ctx, w));
}

void mmgr_occult_reddo(size_t mark)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    wipe_down_to(bind(ctx, w), mark);
}

void mmgr_occult_reset(void)
{
    struct SecureInternal *ctx = secure_ctx();
    int w = cur_worker();
    assert_single_owner(ctx, w);
    mmgr_confin *a = peek(ctx, w);
    if (a != NULL)
    {
        wipe_down_to(a, a->size);
    }
}

size_t mmgr_occult_used(void)
{
    const mmgr_confin *a = peek(secure_ctx(), cur_worker());
    return (a != NULL) ? mmgr_confin_interim_used(a) : 0;
}

size_t mmgr_occult_high_water(void)
{
    struct SecureInternal *ctx = secure_ctx();
    size_t peak = 0;
    for (int w = 0; w < MMGR_SEC_POOL_SLOTS; w++)
    {
        const mmgr_confin *a = peek(ctx, w);
        if (a != NULL && a->scratch_hw > peak)
        {
            peak = a->scratch_hw;
        }
    }
    return peak;
}

size_t mmgr_occult_capacity(void)
{
    return MMGR_SECURE_ARENA_SIZE;
}

mmgr_bool mmgr_occult_owns(const void *p)
{
    const struct SecureInternal *ctx = secure_ctx();

    return ctx->store != NULL && secure_offset(ctx, p) < (uintptr_t)sizeof(ctx->store->mem);
}

int mmgr_occult_slot_of(const void *p)
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
