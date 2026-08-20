// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_ARENA_H
#define PROTOCORE_ARENA_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

int protocore_worker_count(void);

#if PROTOCORE_WORKER_COUNT == 1
PROTOCORE_INLINE int protocore_worker_self(void)
{
    return 0;
}
#else
int protocore_worker_self(void);
#endif

void protocore_worker_set_self(int id);

#define PROTOCORE_ARENA_ALIGN 8u

PROTOCORE_INLINE size_t protocore_arena_align_up(size_t n)
{
    return (n + (PROTOCORE_ARENA_ALIGN - 1)) & ~(size_t)(PROTOCORE_ARENA_ALIGN - 1);
}

#define PROTOCORE_ARENA_MAX_ALIGN 16u

typedef struct
{
    uint8_t *base;
    size_t size;
    size_t persist_end;
    size_t scratch_top;
    size_t persist_used;
    size_t persist_hw;
    size_t scratch_hw;
} protocore_arena;

void protocore_arena_init(protocore_arena *a, void *base, size_t size);

void *protocore_arena_persist_alloc(protocore_arena *a, size_t n);

void protocore_arena_persist_free(protocore_arena *a, void *p);

PROTOCORE_INLINE void *protocore_arena_scratch_alloc_aligned(protocore_arena *a, size_t n, size_t align)
{
    if (align < PROTOCORE_ARENA_ALIGN)
    {
        align = PROTOCORE_ARENA_ALIGN;
    }
    if (align > PROTOCORE_ARENA_MAX_ALIGN)
    {
        align = PROTOCORE_ARENA_MAX_ALIGN;
    }
    n = protocore_arena_align_up(n ? n : PROTOCORE_ARENA_ALIGN);
    if (a->scratch_top < n)
    {
        return NULL;
    }

    size_t nt = (a->scratch_top - n) & ~(size_t)(align - 1);

    if (nt < a->persist_end || nt > a->scratch_top)
    {
        return NULL;
    }
    a->scratch_top = nt;
    size_t used = a->size - a->scratch_top;
    if (used > a->scratch_hw)
    {
        a->scratch_hw = used;
    }
    return a->base + a->scratch_top;
}

void *protocore_arena_scratch_alloc(protocore_arena *a, size_t n);

PROTOCORE_INLINE size_t protocore_arena_scratch_mark(const protocore_arena *a)
{
    return a->scratch_top;
}

PROTOCORE_INLINE void protocore_arena_scratch_release(protocore_arena *a, size_t mark)
{

    if (mark >= a->scratch_top && mark <= a->size)
    {
        a->scratch_top = mark;
    }
}

PROTOCORE_INLINE void protocore_arena_scratch_reset(protocore_arena *a)
{
    a->scratch_top = a->size;
}

PROTOCORE_INLINE proto_bool protocore_arena_owns(const protocore_arena *a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

size_t protocore_arena_free_bytes(const protocore_arena *a);

size_t protocore_arena_persist_used(const protocore_arena *a);

PROTOCORE_INLINE size_t protocore_arena_scratch_used(const protocore_arena *a)
{
    return a->size - a->scratch_top;
}

#ifndef PROTOCORE_ARENA_MAX_REGIONS
#define PROTOCORE_ARENA_MAX_REGIONS 2u
#endif

typedef struct
{
    protocore_arena region[PROTOCORE_ARENA_MAX_REGIONS];
    size_t count;
} protocore_arena_set;

typedef struct
{
    size_t top[PROTOCORE_ARENA_MAX_REGIONS];
    size_t count;
} protocore_arena_mark;

void protocore_arena_set_init(protocore_arena_set *s);

proto_bool protocore_arena_set_add(protocore_arena_set *s, void *base, size_t size);

void *protocore_arena_set_persist_alloc(protocore_arena_set *s, size_t n);

void protocore_arena_set_persist_free(protocore_arena_set *s, void *p);

void *protocore_arena_set_scratch_alloc_aligned(protocore_arena_set *s, size_t n, size_t align);

void *protocore_arena_set_scratch_alloc(protocore_arena_set *s, size_t n);

protocore_arena_mark protocore_arena_set_scratch_mark(const protocore_arena_set *s);

void protocore_arena_set_scratch_release(protocore_arena_set *s, const protocore_arena_mark *m);

void protocore_arena_set_scratch_reset(protocore_arena_set *s);

size_t protocore_arena_set_free_bytes(const protocore_arena_set *s);

size_t protocore_arena_set_persist_used(const protocore_arena_set *s);

size_t protocore_arena_set_scratch_used(const protocore_arena_set *s);

PROTOCORE_END_DECLS

#endif
