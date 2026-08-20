// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_H
#define MMGR_CONFINIUM_H

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
void mmgr_worker_set_self(int id);
#endif

#define MMGR_ARENA_ALIGN 8u

MMGR_INLINE size_t mmgr_confin_align_up(size_t n)
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
} mmgr_confin;

void mmgr_confin_init(mmgr_confin *a, void *base, size_t size);

void *mmgr_confin_persist_capio(mmgr_confin *a, size_t n);

void mmgr_confin_persist_reddo(mmgr_confin *a, void *p);

MMGR_INLINE void *mmgr_confin_interim_capio_aligned(mmgr_confin *a, size_t n, size_t align)
{
    if (align < MMGR_ARENA_ALIGN)
    {
        align = MMGR_ARENA_ALIGN;
    }
    if (align > MMGR_ARENA_MAX_ALIGN)
    {
        align = MMGR_ARENA_MAX_ALIGN;
    }
    n = mmgr_confin_align_up(n ? n : MMGR_ARENA_ALIGN);
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

void *mmgr_confin_interim_capio(mmgr_confin *a, size_t n);

MMGR_INLINE size_t mmgr_confin_interim_mark(const mmgr_confin *a)
{
    return a->scratch_top;
}

MMGR_INLINE void mmgr_confin_interim_reddo(mmgr_confin *a, size_t mark)
{

    if (mark >= a->scratch_top && mark <= a->size)
    {
        a->scratch_top = mark;
    }
}

MMGR_INLINE void mmgr_confin_interim_reset(mmgr_confin *a)
{
    a->scratch_top = a->size;
}

MMGR_INLINE mmgr_bool mmgr_confin_owns(const mmgr_confin *a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

size_t mmgr_confin_octas_praesto(const mmgr_confin *a);

size_t mmgr_confin_persist_used(const mmgr_confin *a);

MMGR_INLINE size_t mmgr_confin_interim_used(const mmgr_confin *a)
{
    return a->size - a->scratch_top;
}

#ifndef MMGR_ARENA_MAX_REGIONS
#define MMGR_ARENA_MAX_REGIONS 2u
#endif

typedef struct
{
    mmgr_confin region[MMGR_ARENA_MAX_REGIONS];
    size_t count;
} mmgr_confin_set;

typedef struct
{
    size_t top[MMGR_ARENA_MAX_REGIONS];
    size_t count;
} mmgr_confin_mark;

void mmgr_confin_set_init(mmgr_confin_set *s);

mmgr_bool mmgr_confin_set_add(mmgr_confin_set *s, void *base, size_t size);

void *mmgr_confin_set_persist_capio(mmgr_confin_set *s, size_t n);

void mmgr_confin_set_persist_reddo(mmgr_confin_set *s, void *p);

void *mmgr_confin_set_interim_capio_aligned(mmgr_confin_set *s, size_t n, size_t align);

void *mmgr_confin_set_interim_capio(mmgr_confin_set *s, size_t n);

mmgr_confin_mark mmgr_confin_set_interim_mark(const mmgr_confin_set *s);

void mmgr_confin_set_interim_reddo(mmgr_confin_set *s, const mmgr_confin_mark *m);

void mmgr_confin_set_interim_reset(mmgr_confin_set *s);

size_t mmgr_confin_set_octas_praesto(const mmgr_confin_set *s);

size_t mmgr_confin_set_persist_used(const mmgr_confin_set *s);

size_t mmgr_confin_set_interim_used(const mmgr_confin_set *s);

MMGR_END_DECLS

#endif
