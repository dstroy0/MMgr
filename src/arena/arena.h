// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ARENA_H
#define MMGR_ARENA_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

int mmgr_worker_count(void);

#if MMGR_WORKER_COUNT == 1
MMGR_INLINE int mmgr_worker_self(void)
{
    return 0;
}
#else
int mmgr_worker_self(void);
#endif

void mmgr_worker_set_self(int id);

#define MMGR_ARENA_ALIGN 8u

MMGR_INLINE size_t mmgr_arena_align_up(size_t n)
{
    return (n + (MMGR_ARENA_ALIGN - 1)) & ~(size_t)(MMGR_ARENA_ALIGN - 1);
}

#define MMGR_ARENA_MAX_ALIGN 16u

typedef struct
{
    uint8_t *base;
    size_t size;
    size_t persist_end;
    size_t scratch_top;
    size_t persist_used;
    size_t persist_hw;
    size_t scratch_hw;
} mmgr_arena;

void mmgr_arena_init(mmgr_arena *a, void *base, size_t size);

void *mmgr_arena_persist_alloc(mmgr_arena *a, size_t n);

void mmgr_arena_persist_free(mmgr_arena *a, void *p);

MMGR_INLINE void *mmgr_arena_scratch_alloc_aligned(mmgr_arena *a, size_t n, size_t align)
{
    if (align < MMGR_ARENA_ALIGN)
    {
        align = MMGR_ARENA_ALIGN;
    }
    if (align > MMGR_ARENA_MAX_ALIGN)
    {
        align = MMGR_ARENA_MAX_ALIGN;
    }
    n = mmgr_arena_align_up(n ? n : MMGR_ARENA_ALIGN);
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

void *mmgr_arena_scratch_alloc(mmgr_arena *a, size_t n);

MMGR_INLINE size_t mmgr_arena_scratch_mark(const mmgr_arena *a)
{
    return a->scratch_top;
}

MMGR_INLINE void mmgr_arena_scratch_release(mmgr_arena *a, size_t mark)
{

    if (mark >= a->scratch_top && mark <= a->size)
    {
        a->scratch_top = mark;
    }
}

MMGR_INLINE void mmgr_arena_scratch_reset(mmgr_arena *a)
{
    a->scratch_top = a->size;
}

MMGR_INLINE mmgr_bool mmgr_arena_owns(const mmgr_arena *a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

size_t mmgr_arena_free_bytes(const mmgr_arena *a);

size_t mmgr_arena_persist_used(const mmgr_arena *a);

MMGR_INLINE size_t mmgr_arena_scratch_used(const mmgr_arena *a)
{
    return a->size - a->scratch_top;
}

#ifndef MMGR_ARENA_MAX_REGIONS
#define MMGR_ARENA_MAX_REGIONS 2u
#endif

typedef struct
{
    mmgr_arena region[MMGR_ARENA_MAX_REGIONS];
    size_t count;
} mmgr_arena_set;

typedef struct
{
    size_t top[MMGR_ARENA_MAX_REGIONS];
    size_t count;
} mmgr_arena_mark;

void mmgr_arena_set_init(mmgr_arena_set *s);

mmgr_bool mmgr_arena_set_add(mmgr_arena_set *s, void *base, size_t size);

void *mmgr_arena_set_persist_alloc(mmgr_arena_set *s, size_t n);

void mmgr_arena_set_persist_free(mmgr_arena_set *s, void *p);

void *mmgr_arena_set_scratch_alloc_aligned(mmgr_arena_set *s, size_t n, size_t align);

void *mmgr_arena_set_scratch_alloc(mmgr_arena_set *s, size_t n);

mmgr_arena_mark mmgr_arena_set_scratch_mark(const mmgr_arena_set *s);

void mmgr_arena_set_scratch_release(mmgr_arena_set *s, const mmgr_arena_mark *m);

void mmgr_arena_set_scratch_reset(mmgr_arena_set *s);

size_t mmgr_arena_set_free_bytes(const mmgr_arena_set *s);

size_t mmgr_arena_set_persist_used(const mmgr_arena_set *s);

size_t mmgr_arena_set_scratch_used(const mmgr_arena_set *s);

MMGR_END_DECLS

#endif
