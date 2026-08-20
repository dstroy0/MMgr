// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "confinium/confinium.h"
#include "memoria_operor/memoria_operor.h"

int mmgr_worker_count(void)
{
    return MMGR_WORKER_COUNT;
}

// The context-to-worker binding table, and the two entries that maintain it.
//
// All of it is gated on there being more than one worker. With a single worker mmgr_worker_self()
// is an inline in the header that returns 0, so there is nothing a binding could answer - and
// leaving the setter ungated made a single-worker build call mmgr_platform_context_id() anyway.
// That is an integrator-supplied symbol, so the default configuration of a library that is meant to
// need nothing but stddef/stdint/stdatomic did not link without one.
//
// It was worse than a missing symbol: the declaration is guarded too, so the call compiled under an
// implicit declaration, gcc assumed it returned int, and the result was assigned to a 64-bit
// uintptr_t. Caught by -Wimplicit-function-declaration once the CMake build turned the warnings on.
#if MMGR_WORKER_COUNT != 1

#define MMGR_WORKER_BINDINGS (MMGR_WORKER_COUNT + 1)

static uintptr_t s_ctx[MMGR_WORKER_BINDINGS];
static int s_ctx_worker[MMGR_WORKER_BINDINGS];
static int s_bound;

int mmgr_worker_self(void)
{
    const uintptr_t me = mmgr_platform_context_id();
    for (int i = 0; i < s_bound; i++)
    {
        if (s_ctx[i] == me)
        {
            return s_ctx_worker[i];
        }
    }
    return 0;
}

void mmgr_worker_set_self(int id)
{
    const uintptr_t me = mmgr_platform_context_id();
    for (int i = 0; i < s_bound; i++)
    {
        if (s_ctx[i] == me)
        {
            s_ctx_worker[i] = id;
            return;
        }
    }
    if (s_bound < MMGR_WORKER_BINDINGS)
    {
        s_ctx[s_bound] = me;
        s_ctx_worker[s_bound] = id;
        s_bound++;
    }
}

#endif // MMGR_WORKER_COUNT != 1

typedef struct
{
    size_t size;
    size_t used;
} ABlk;

static const size_t AHDR = (sizeof(ABlk) + (MMGR_ARENA_ALIGN - 1)) & ~(size_t)(MMGR_ARENA_ALIGN - 1);

static inline size_t align_up(size_t n)
{
    return (n + (MMGR_ARENA_ALIGN - 1)) & ~(size_t)(MMGR_ARENA_ALIGN - 1);
}

void mmgr_confin_init(mmgr_confin *a, void *base, size_t size)
{

    uintptr_t b = (uintptr_t)base;
    uintptr_t ab = (b + (MMGR_ARENA_MAX_ALIGN - 1)) & ~(uintptr_t)(MMGR_ARENA_MAX_ALIGN - 1);
    size_t adj = (size_t)(ab - b);
    a->base = (uint8_t *)ab;
    a->size = (size > adj) ? ((size - adj) & ~(size_t)(MMGR_ARENA_ALIGN - 1)) : 0;
    a->persist_end = 0;
    a->scratch_top = a->size;
    a->persist_used = 0;
    a->persist_hw = 0;
    a->scratch_hw = 0;
}

void *mmgr_confin_persist_capio(mmgr_confin *a, size_t n)
{
    n = align_up(n ? n : MMGR_ARENA_ALIGN);

    size_t off = 0;
    while (off < a->persist_end)
    {
        ABlk *b = (ABlk *)(a->base + off);
        if (!b->used && b->size >= n)
        {

            if (b->size >= n + AHDR + MMGR_ARENA_ALIGN)
            {
                ABlk *nb = (ABlk *)(a->base + off + AHDR + n);
                nb->size = b->size - n - AHDR;
                nb->used = 0;
                b->size = n;
            }
            b->used = 1;
            a->persist_used += b->size;
            void *pl = a->base + off + AHDR;
            memor.set(pl, 0, b->size);
            return pl;
        }
        off += AHDR + b->size;
    }

    size_t need = AHDR + n;
    if (a->persist_end + need <= a->scratch_top && a->persist_end + need >= need)
    {
        ABlk *b = (ABlk *)(a->base + a->persist_end);
        b->size = n;
        b->used = 1;
        void *pl = a->base + a->persist_end + AHDR;
        a->persist_end += need;
        if (a->persist_end > a->persist_hw)
        {
            a->persist_hw = a->persist_end;
        }
        a->persist_used += n;
        memor.set(pl, 0, n);
        return pl;
    }
    return NULL;
}

void mmgr_confin_persist_reddo(mmgr_confin *a, void *p)
{
    if (!p)
    {
        return;
    }
    ABlk *b = (ABlk *)((uint8_t *)p - AHDR);
    if (b->used)
    {
        b->used = 0;

        if (a->persist_used >= b->size)
        {
            a->persist_used -= b->size;
        }
    }

    size_t off = 0;
    while (off < a->persist_end)
    {
        ABlk *cur = (ABlk *)(a->base + off);
        size_t next_off = off + AHDR + cur->size;
        if (!cur->used && next_off < a->persist_end)
        {
            ABlk *nxt = (ABlk *)(a->base + next_off);
            if (!nxt->used)
            {
                cur->size += AHDR + nxt->size;
                continue;
            }
        }
        off = next_off;
    }

    off = 0;
    size_t last = 0;
    while (off < a->persist_end)
    {
        last = off;
        ABlk *cur = (ABlk *)(a->base + off);
        off += AHDR + cur->size;
    }
    if (a->persist_end > 0 && !((ABlk *)(a->base + last))->used)
    {
        a->persist_end = last;
    }
}

void *mmgr_confin_interim_capio(mmgr_confin *a, size_t n)
{
    return mmgr_confin_interim_capio_aligned(a, n, MMGR_ARENA_ALIGN);
}

size_t mmgr_confin_octas_praesto(const mmgr_confin *a)
{
    size_t mid = (a->scratch_top > a->persist_end) ? a->scratch_top - a->persist_end : 0;
    return mid > AHDR ? mid - AHDR : 0;
}

size_t mmgr_confin_persist_used(const mmgr_confin *a)
{
    return a->persist_used;
}

void mmgr_confin_set_init(mmgr_confin_set *s)
{
    s->count = 0;
}

mmgr_bool mmgr_confin_set_add(mmgr_confin_set *s, void *base, size_t size)
{
    if (s->count >= MMGR_ARENA_MAX_REGIONS)
    {
        return MMGR_FALSE;
    }
    mmgr_confin *r = &s->region[s->count];
    mmgr_confin_init(r, base, size);
    if (r->size < AHDR + MMGR_ARENA_ALIGN)
    {
        return MMGR_FALSE;
    }
    s->count++;
    return MMGR_TRUE;
}

void *mmgr_confin_set_persist_capio(mmgr_confin_set *s, size_t n)
{
    for (size_t i = 0; i < s->count; i++)
    {
        void *p = mmgr_confin_persist_capio(&s->region[i], n);
        if (p)
        {
            return p;
        }
    }
    return NULL;
}

void mmgr_confin_set_persist_reddo(mmgr_confin_set *s, void *p)
{
    if (!p)
    {
        return;
    }
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < s->count; i++)
    {
        mmgr_confin *r = &s->region[i];
        if (b >= r->base && b < r->base + r->size)
        {
            mmgr_confin_persist_reddo(r, p);
            return;
        }
    }
}

void *mmgr_confin_set_interim_capio_aligned(mmgr_confin_set *s, size_t n, size_t align)
{
    for (size_t i = 0; i < s->count; i++)
    {
        void *p = mmgr_confin_interim_capio_aligned(&s->region[i], n, align);
        if (p)
        {
            return p;
        }
    }
    return NULL;
}

void *mmgr_confin_set_interim_capio(mmgr_confin_set *s, size_t n)
{
    return mmgr_confin_set_interim_capio_aligned(s, n, MMGR_ARENA_ALIGN);
}

mmgr_confin_mark mmgr_confin_set_interim_mark(const mmgr_confin_set *s)
{
    mmgr_confin_mark m;
    m.count = s->count;
    for (size_t i = 0; i < s->count; i++)
    {
        m.top[i] = s->region[i].scratch_top;
    }
    return m;
}

void mmgr_confin_set_interim_reddo(mmgr_confin_set *s, const mmgr_confin_mark *m)
{
    size_t n = m->count < s->count ? m->count : s->count;
    for (size_t i = 0; i < n; i++)
    {
        mmgr_confin_interim_reddo(&s->region[i], m->top[i]);
    }
}

void mmgr_confin_set_interim_reset(mmgr_confin_set *s)
{
    for (size_t i = 0; i < s->count; i++)
    {
        mmgr_confin_interim_reset(&s->region[i]);
    }
}

size_t mmgr_confin_set_octas_praesto(const mmgr_confin_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += mmgr_confin_octas_praesto(&s->region[i]);
    }
    return t;
}

size_t mmgr_confin_set_persist_used(const mmgr_confin_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += mmgr_confin_persist_used(&s->region[i]);
    }
    return t;
}

size_t mmgr_confin_set_interim_used(const mmgr_confin_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += mmgr_confin_interim_used(&s->region[i]);
    }
    return t;
}
