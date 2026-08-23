#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


#define MMGR_CARCER_ALIGN 8u

#define MMGR_CARCER_MAX_ALIGN 16u

typedef struct
{
    uint8_t *base;          size_t size;            size_t persist_end;     size_t scratch_top;     size_t persist_used;    size_t persist_hw;      size_t scratch_hw;  } mmgr_carcer;

MMGR_INLINE size_t mmgr_carcer_align_up(size_t n)
{
    return (n + (MMGR_CARCER_ALIGN - 1)) & ~(size_t)(MMGR_CARCER_ALIGN - 1);
}

MMGR_INLINE void *mmgr_carcer_interim_capio_aligned(mmgr_carcer *const a, size_t n, size_t align)
{
    if (align < MMGR_CARCER_ALIGN)
    {
        align = MMGR_CARCER_ALIGN;
    }
    if (align > MMGR_CARCER_MAX_ALIGN)
    {
        align = MMGR_CARCER_MAX_ALIGN;
    }
    n = mmgr_carcer_align_up(n ? n : MMGR_CARCER_ALIGN);
    if (a->scratch_top < n)
    {
        return NULL;
    }

    size_t nt = (a->scratch_top - n) & ~(size_t)(align - 1);

        if (nt < a->persist_end || nt > a->scratch_top)     {
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

MMGR_INLINE size_t mmgr_carcer_interim_mark(mmgr_carcer *const a)
{
    return a->scratch_top;
}

MMGR_INLINE void mmgr_carcer_interim_reddo(mmgr_carcer *const a, size_t mark)
{

    if (mark >= a->scratch_top && mark <= a->size)
    {
        a->scratch_top = mark;
    }
}

MMGR_INLINE void mmgr_carcer_interim_reset(mmgr_carcer *const a)
{
    a->scratch_top = a->size;
}

MMGR_INLINE mmgr_bool mmgr_carcer_owns(mmgr_carcer *const a, const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    return a->base != NULL && q >= a->base && q < a->base + a->size;
}

MMGR_INLINE size_t mmgr_carcer_interim_used(mmgr_carcer *const a)
{
    return a->size - a->scratch_top;
}

void mmgr_carcer_init(mmgr_carcer *const a, void *base, size_t size);
void *mmgr_carcer_persist_capio(mmgr_carcer *const a, size_t n);
void mmgr_carcer_persist_reddo(mmgr_carcer *const a, void *p);
void *mmgr_carcer_interim_capio(mmgr_carcer *const a, size_t n);
size_t mmgr_carcer_octas_praesto(mmgr_carcer *const a);
size_t mmgr_carcer_persist_used(mmgr_carcer *const a);

typedef struct
{
    size_t (*align_up)(size_t n);
    void (*init)(mmgr_carcer *const a, void *base, size_t size);
    void *(*persist_capio)(mmgr_carcer *const a, size_t n);
    void (*persist_reddo)(mmgr_carcer *const a, void *p);
    void *(*interim_capio_aligned)(mmgr_carcer *const a, size_t n, size_t align);
    void *(*interim_capio)(mmgr_carcer *const a, size_t n);
    size_t (*interim_mark)(mmgr_carcer *const a);
    void (*interim_reddo)(mmgr_carcer *const a, size_t mark);
    void (*interim_reset)(mmgr_carcer *const a);
    mmgr_bool (*owns)(mmgr_carcer *const a, const void *p);
    size_t (*octas_praesto)(mmgr_carcer *const a);
    size_t (*persist_used)(mmgr_carcer *const a);
    size_t (*interim_used)(mmgr_carcer *const a);
} CarceribusNs;
MMGR_NS_LAYOUT(CarceribusNs, align_up, init, persist_capio, persist_reddo, interim_capio_aligned, interim_capio,
               interim_mark, interim_reddo, interim_reset, owns, octas_praesto, persist_used, interim_used);

MMGR_NS CarceribusNs carcer MMGR_UNUSED = {
    .align_up = mmgr_carcer_align_up,
    .init = mmgr_carcer_init,
    .persist_capio = mmgr_carcer_persist_capio,
    .persist_reddo = mmgr_carcer_persist_reddo,
    .interim_capio_aligned = mmgr_carcer_interim_capio_aligned,
    .interim_capio = mmgr_carcer_interim_capio,
    .interim_mark = mmgr_carcer_interim_mark,
    .interim_reddo = mmgr_carcer_interim_reddo,
    .interim_reset = mmgr_carcer_interim_reset,
    .owns = mmgr_carcer_owns,
    .octas_praesto = mmgr_carcer_octas_praesto,
    .persist_used = mmgr_carcer_persist_used,
    .interim_used = mmgr_carcer_interim_used,
};

#ifndef MMGR_CARCER_MAX_REGIONS
#define MMGR_CARCER_MAX_REGIONS 2u
#endif

typedef struct
{
    mmgr_carcer region[MMGR_CARCER_MAX_REGIONS];     size_t count;                                } mmgr_carcer_set;

typedef struct
{
    size_t top[MMGR_CARCER_MAX_REGIONS];     size_t count;                        } mmgr_carcer_mark;

void mmgr_carcer_set_init(mmgr_carcer_set *const s);
mmgr_bool mmgr_carcer_set_add(mmgr_carcer_set *const s, void *base, size_t size);
void *mmgr_carcer_set_persist_capio(mmgr_carcer_set *const s, size_t n);
void mmgr_carcer_set_persist_reddo(mmgr_carcer_set *const s, void *p);
void *mmgr_carcer_set_interim_capio_aligned(mmgr_carcer_set *const s, size_t n, size_t align);
void *mmgr_carcer_set_interim_capio(mmgr_carcer_set *const s, size_t n);
mmgr_carcer_mark mmgr_carcer_set_interim_mark(mmgr_carcer_set *const s);
void mmgr_carcer_set_interim_reddo(mmgr_carcer_set *const s, const mmgr_carcer_mark *m);
void mmgr_carcer_set_interim_reset(mmgr_carcer_set *const s);
size_t mmgr_carcer_set_octas_praesto(mmgr_carcer_set *const s);
size_t mmgr_carcer_set_persist_used(mmgr_carcer_set *const s);
size_t mmgr_carcer_set_interim_used(mmgr_carcer_set *const s);

typedef struct
{
    void (*init)(mmgr_carcer_set *const s);
    mmgr_bool (*add)(mmgr_carcer_set *const s, void *base, size_t size);
    void *(*persist_capio)(mmgr_carcer_set *const s, size_t n);
    void (*persist_reddo)(mmgr_carcer_set *const s, void *p);
    void *(*interim_capio_aligned)(mmgr_carcer_set *const s, size_t n, size_t align);
    void *(*interim_capio)(mmgr_carcer_set *const s, size_t n);
    mmgr_carcer_mark (*interim_mark)(mmgr_carcer_set *const s);
    void (*interim_reddo)(mmgr_carcer_set *const s, const mmgr_carcer_mark *m);
    void (*interim_reset)(mmgr_carcer_set *const s);
    size_t (*octas_praesto)(mmgr_carcer_set *const s);
    size_t (*persist_used)(mmgr_carcer_set *const s);
    size_t (*interim_used)(mmgr_carcer_set *const s);
} ConfiniumSetNs;
MMGR_NS_LAYOUT(ConfiniumSetNs, init, add, persist_capio, persist_reddo, interim_capio_aligned, interim_capio,
               interim_mark, interim_reddo, interim_reset, octas_praesto, persist_used, interim_used);

MMGR_NS ConfiniumSetNs carcer_set MMGR_UNUSED = {
    .init = mmgr_carcer_set_init,
    .add = mmgr_carcer_set_add,
    .persist_capio = mmgr_carcer_set_persist_capio,
    .persist_reddo = mmgr_carcer_set_persist_reddo,
    .interim_capio_aligned = mmgr_carcer_set_interim_capio_aligned,
    .interim_capio = mmgr_carcer_set_interim_capio,
    .interim_mark = mmgr_carcer_set_interim_mark,
    .interim_reddo = mmgr_carcer_set_interim_reddo,
    .interim_reset = mmgr_carcer_set_interim_reset,
    .octas_praesto = mmgr_carcer_set_octas_praesto,
    .persist_used = mmgr_carcer_set_persist_used,
    .interim_used = mmgr_carcer_set_interim_used,
};

MMGR_FINIS_DECLS

#endif
