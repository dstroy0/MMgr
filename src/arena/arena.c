// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/arena/arena.h"
#include "mmgr/protomem/protomem.h"

#define PROTOCORE_WORKER_BINDINGS (PROTOCORE_WORKER_COUNT + 1)

static uintptr_t s_ctx[PROTOCORE_WORKER_BINDINGS];
static int s_ctx_worker[PROTOCORE_WORKER_BINDINGS];
static int s_bound;

int protocore_worker_count(void)
{
    return PROTOCORE_WORKER_COUNT;
}

#if PROTOCORE_WORKER_COUNT != 1
int protocore_worker_self(void)
{
    const uintptr_t me = protocore_platform_context_id();
    for (int i = 0; i < s_bound; i++)
    {
        if (s_ctx[i] == me)
        {
            return s_ctx_worker[i];
        }
    }
    return 0;
}
#endif

void protocore_worker_set_self(int id)
{
    const uintptr_t me = protocore_platform_context_id();
    for (int i = 0; i < s_bound; i++)
    {
        if (s_ctx[i] == me)
        {
            s_ctx_worker[i] = id;
            return;
        }
    }
    if (s_bound < PROTOCORE_WORKER_BINDINGS)
    {
        s_ctx[s_bound] = me;
        s_ctx_worker[s_bound] = id;
        s_bound++;
    }
}

typedef struct
{
    size_t size;
    size_t used;
} ABlk;

static const size_t AHDR = (sizeof(ABlk) + (PROTOCORE_ARENA_ALIGN - 1)) & ~(size_t)(PROTOCORE_ARENA_ALIGN - 1);

static inline size_t align_up(size_t n)
{
    return (n + (PROTOCORE_ARENA_ALIGN - 1)) & ~(size_t)(PROTOCORE_ARENA_ALIGN - 1);
}

void protocore_arena_init(protocore_arena *a, void *base, size_t size)
{

    uintptr_t b = (uintptr_t)base;
    uintptr_t ab = (b + (PROTOCORE_ARENA_MAX_ALIGN - 1)) & ~(uintptr_t)(PROTOCORE_ARENA_MAX_ALIGN - 1);
    size_t adj = (size_t)(ab - b);
    a->base = (uint8_t *)ab;
    a->size = (size > adj) ? ((size - adj) & ~(size_t)(PROTOCORE_ARENA_ALIGN - 1)) : 0;
    a->persist_end = 0;
    a->scratch_top = a->size;
    a->persist_used = 0;
    a->persist_hw = 0;
    a->scratch_hw = 0;
}

void *protocore_arena_persist_alloc(protocore_arena *a, size_t n)
{
    n = align_up(n ? n : PROTOCORE_ARENA_ALIGN);

    size_t off = 0;
    while (off < a->persist_end)
    {
        ABlk *b = (ABlk *)(a->base + off);
        if (!b->used && b->size >= n)
        {

            if (b->size >= n + AHDR + PROTOCORE_ARENA_ALIGN)
            {
                ABlk *nb = (ABlk *)(a->base + off + AHDR + n);
                nb->size = b->size - n - AHDR;
                nb->used = 0;
                b->size = n;
            }
            b->used = 1;
            a->persist_used += b->size;
            void *pl = a->base + off + AHDR;
            mem.set(pl, 0, b->size);
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
        mem.set(pl, 0, n);
        return pl;
    }
    return NULL;
}

void protocore_arena_persist_free(protocore_arena *a, void *p)
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

void *protocore_arena_scratch_alloc(protocore_arena *a, size_t n)
{
    return protocore_arena_scratch_alloc_aligned(a, n, PROTOCORE_ARENA_ALIGN);
}

size_t protocore_arena_free_bytes(const protocore_arena *a)
{
    size_t mid = (a->scratch_top > a->persist_end) ? a->scratch_top - a->persist_end : 0;
    return mid > AHDR ? mid - AHDR : 0;
}

size_t protocore_arena_persist_used(const protocore_arena *a)
{
    return a->persist_used;
}

void protocore_arena_set_init(protocore_arena_set *s)
{
    s->count = 0;
}

proto_bool protocore_arena_set_add(protocore_arena_set *s, void *base, size_t size)
{
    if (s->count >= PROTOCORE_ARENA_MAX_REGIONS)
    {
        return PROTO_FALSE;
    }
    protocore_arena *r = &s->region[s->count];
    protocore_arena_init(r, base, size);
    if (r->size < AHDR + PROTOCORE_ARENA_ALIGN)
    {
        return PROTO_FALSE;
    }
    s->count++;
    return PROTO_TRUE;
}

void *protocore_arena_set_persist_alloc(protocore_arena_set *s, size_t n)
{
    for (size_t i = 0; i < s->count; i++)
    {
        void *p = protocore_arena_persist_alloc(&s->region[i], n);
        if (p)
        {
            return p;
        }
    }
    return NULL;
}

void protocore_arena_set_persist_free(protocore_arena_set *s, void *p)
{
    if (!p)
    {
        return;
    }
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < s->count; i++)
    {
        protocore_arena *r = &s->region[i];
        if (b >= r->base && b < r->base + r->size)
        {
            protocore_arena_persist_free(r, p);
            return;
        }
    }
}

void *protocore_arena_set_scratch_alloc_aligned(protocore_arena_set *s, size_t n, size_t align)
{
    for (size_t i = 0; i < s->count; i++)
    {
        void *p = protocore_arena_scratch_alloc_aligned(&s->region[i], n, align);
        if (p)
        {
            return p;
        }
    }
    return NULL;
}

void *protocore_arena_set_scratch_alloc(protocore_arena_set *s, size_t n)
{
    return protocore_arena_set_scratch_alloc_aligned(s, n, PROTOCORE_ARENA_ALIGN);
}

protocore_arena_mark protocore_arena_set_scratch_mark(const protocore_arena_set *s)
{
    protocore_arena_mark m;
    m.count = s->count;
    for (size_t i = 0; i < s->count; i++)
    {
        m.top[i] = s->region[i].scratch_top;
    }
    return m;
}

void protocore_arena_set_scratch_release(protocore_arena_set *s, const protocore_arena_mark *m)
{
    size_t n = m->count < s->count ? m->count : s->count;
    for (size_t i = 0; i < n; i++)
    {
        protocore_arena_scratch_release(&s->region[i], m->top[i]);
    }
}

void protocore_arena_set_scratch_reset(protocore_arena_set *s)
{
    for (size_t i = 0; i < s->count; i++)
    {
        protocore_arena_scratch_reset(&s->region[i]);
    }
}

size_t protocore_arena_set_free_bytes(const protocore_arena_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += protocore_arena_free_bytes(&s->region[i]);
    }
    return t;
}

size_t protocore_arena_set_persist_used(const protocore_arena_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += protocore_arena_persist_used(&s->region[i]);
    }
    return t;
}

size_t protocore_arena_set_scratch_used(const protocore_arena_set *s)
{
    size_t t = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        t += protocore_arena_scratch_used(&s->region[i]);
    }
    return t;
}
