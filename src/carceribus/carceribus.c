#include "carceribus/carceribus.h"
#include "memoria_operor/memoria_operor.h"


typedef struct
{
    size_t size;
    size_t used;
} ABlk;

static const size_t AHDR = (sizeof(ABlk) + (MMGR_CARCER_ALIGN - 1)) & ~(size_t)(MMGR_CARCER_ALIGN - 1);

typedef struct
{
    mmgr_carcer *const a;             mmgr_carcer_set *const s;         const mmgr_carcer_mark *mark;     void *base;                       void *p;                          size_t n;                         size_t align;                 } CarcerCtx;

MMGR_INLINE size_t carcer_align_up(const CarcerCtx *c)
{
    return (c->n + (MMGR_CARCER_ALIGN - 1)) & ~(size_t)(MMGR_CARCER_ALIGN - 1);
}

MMGR_INLINE void carcer_init(CarcerCtx *c)
{
    const uintptr_t b = (uintptr_t)c->base;
    const uintptr_t ab = (b + (MMGR_CARCER_MAX_ALIGN - 1)) & ~(uintptr_t)(MMGR_CARCER_MAX_ALIGN - 1);
    const size_t adj = (size_t)(ab - b);

    c->a->base = (uint8_t *)ab;
    c->a->size = (c->n > adj) ? ((c->n - adj) & ~(size_t)(MMGR_CARCER_ALIGN - 1)) : 0;
    c->a->persist_end = 0;
    c->a->scratch_top = c->a->size;
    c->a->persist_used = 0;
    c->a->persist_hw = 0;
    c->a->scratch_hw = 0;
}

MMGR_INLINE void *carcer_persist_capio(CarcerCtx *c)
{
    mmgr_carcer *const a = c->a;

    c->n = c->n ? c->n : MMGR_CARCER_ALIGN;
    const size_t n = carcer_align_up(c);

    size_t off = 0;
    while (off < a->persist_end)
    {
        ABlk *b = (ABlk *)(a->base + off);
        if (!b->used && b->size >= n)
        {
            if (b->size >= n + AHDR + MMGR_CARCER_ALIGN)
            {
                ABlk *nb = (ABlk *)(a->base + off + AHDR + n);
                nb->size = b->size - n - AHDR;
                nb->used = 0;
                b->size = n;
            }
            b->used = 1;
            a->persist_used += b->size;
            void *pl = a->base + off + AHDR;
            mmgr_memor_set(pl, (uint8_t)0, b->size);
            return pl;
        }
        off += AHDR + b->size;
    }

    const size_t need = AHDR + n;
        if (((a->persist_end + need) <= a->scratch_top) && ((a->persist_end + need) >= need))     {
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
        mmgr_memor_set(pl, (uint8_t)0, n);
        return pl;
    }
    return NULL;
}

MMGR_INLINE void carcer_coalesce(CarcerCtx *c)
{
    mmgr_carcer *const a = c->a;
    size_t off = 0;

    while (off < a->persist_end)
    {
        ABlk *cur = (ABlk *)(a->base + off);
        const size_t next_off = off + AHDR + cur->size;
        if (!cur->used && (next_off < a->persist_end))
        {
            const ABlk *nxt = (const ABlk *)(a->base + next_off);
            if (!nxt->used)
            {
                cur->size += AHDR + nxt->size;
                continue;
            }
        }
        off = next_off;
    }
}

MMGR_INLINE void carcer_trim(CarcerCtx *c)
{
    mmgr_carcer *const a = c->a;
    size_t off = 0;
    size_t last = 0;

    while (off < a->persist_end)
    {
        last = off;
        const ABlk *cur = (const ABlk *)(a->base + off);
        off += AHDR + cur->size;
    }
    if ((a->persist_end > 0) && !((ABlk *)(a->base + last))->used)
    {
        a->persist_end = last;
    }
}

MMGR_INLINE void carcer_persist_reddo(CarcerCtx *c)
{
    if (c->p == NULL)
    {
        return;
    }

    ABlk *b = (ABlk *)((uint8_t *)c->p - AHDR);
    if (b->used)
    {
        b->used = 0;
                if (c->a->persist_used >= b->size)         {
            c->a->persist_used -= b->size;
        }
    }

    carcer_coalesce(c);
    carcer_trim(c);
}

MMGR_INLINE size_t carcer_octas_praesto(const CarcerCtx *c)
{
    mmgr_carcer *const a = c->a;
    const size_t mid = (a->scratch_top > a->persist_end) ? (a->scratch_top - a->persist_end) : 0;

    return (mid > AHDR) ? (mid - AHDR) : 0;
}

MMGR_INLINE size_t carcer_persist_used(const CarcerCtx *c)
{
    return c->a->persist_used;
}

MMGR_INLINE void carcer_set_init(mmgr_carcer_set *const s)
{
    s->count = 0;
}

MMGR_INLINE mmgr_bool carcer_set_add(CarcerCtx *c)
{
    if (c->s->count >= MMGR_CARCER_MAX_REGIONS)
    {
        return MMGR_FALSE;
    }

    mmgr_carcer *const r = &c->s->region[c->s->count];
    mmgr_carcer_init(r, c->base, c->n);
    if (r->size < (AHDR + MMGR_CARCER_ALIGN))
    {
        return MMGR_FALSE;
    }
    c->s->count++;
    return MMGR_TRUE;
}

MMGR_INLINE void *carcer_set_persist_capio(CarcerCtx *c)
{
    for (size_t i = 0; i < c->s->count; i++)
    {
        void *p = mmgr_carcer_persist_capio(&c->s->region[i], c->n);
        if (p != NULL)
        {
            return p;
        }
    }
    return NULL;
}

MMGR_INLINE void carcer_set_persist_reddo(CarcerCtx *c)
{
    if (c->p == NULL)
    {
        return;
    }

    const uint8_t *b = (const uint8_t *)c->p;
    for (size_t i = 0; i < c->s->count; i++)
    {
        mmgr_carcer *const r = &c->s->region[i];
        if ((b >= r->base) && (b < (r->base + r->size)))
        {
            mmgr_carcer_persist_reddo(r, c->p);
            return;
        }
    }
}

MMGR_INLINE void *carcer_set_interim_capio_aligned(CarcerCtx *c)
{
    for (size_t i = 0; i < c->s->count; i++)
    {
        void *p = mmgr_carcer_interim_capio_aligned(&c->s->region[i], c->n, c->align);
        if (p != NULL)
        {
            return p;
        }
    }
    return NULL;
}

MMGR_INLINE mmgr_carcer_mark carcer_set_interim_mark(const CarcerCtx *c)
{
    mmgr_carcer_mark m;

    m.count = c->s->count;
    for (size_t i = 0; i < c->s->count; i++)
    {
        m.top[i] = c->s->region[i].scratch_top;
    }
    return m;
}

MMGR_INLINE void carcer_set_interim_reddo(CarcerCtx *c)
{
    const size_t n = (c->mark->count < c->s->count) ? c->mark->count : c->s->count;

    for (size_t i = 0; i < n; i++)
    {
        mmgr_carcer_interim_reddo(&c->s->region[i], c->mark->top[i]);
    }
}

MMGR_INLINE void carcer_set_interim_reset(mmgr_carcer_set *const s)
{
    for (size_t i = 0; i < s->count; i++)
    {
        mmgr_carcer_interim_reset(&s->region[i]);
    }
}

MMGR_INLINE size_t carcer_set_octas_praesto(const CarcerCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_carcer_octas_praesto(&c->s->region[i]);
    }
    return t;
}

MMGR_INLINE size_t carcer_set_persist_used(const CarcerCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_carcer_persist_used(&c->s->region[i]);
    }
    return t;
}

MMGR_INLINE size_t carcer_set_interim_used(const CarcerCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_carcer_interim_used(&c->s->region[i]);
    }
    return t;
}


void mmgr_carcer_init(mmgr_carcer *const a, void *base, size_t size)
{
    MMGR_CALL(carcer_init, CarcerCtx, .a = a, .base = base, .n = size);
}

void *mmgr_carcer_persist_capio(mmgr_carcer *const a, size_t n)
{
    return MMGR_CALL(carcer_persist_capio, CarcerCtx, .a = a, .n = n);
}

void mmgr_carcer_persist_reddo(mmgr_carcer *const a, void *p)
{
    MMGR_CALL(carcer_persist_reddo, CarcerCtx, .a = a, .p = p);
}

void *mmgr_carcer_interim_capio(mmgr_carcer *const a, size_t n)
{
    return mmgr_carcer_interim_capio_aligned(a, n, MMGR_CARCER_ALIGN);
}

size_t mmgr_carcer_octas_praesto(mmgr_carcer *const a)
{
    return MMGR_CALL(carcer_octas_praesto, CarcerCtx, .a = a);
}

size_t mmgr_carcer_persist_used(mmgr_carcer *const a)
{
    return MMGR_CALL(carcer_persist_used, CarcerCtx, .a = a);
}

void mmgr_carcer_set_init(mmgr_carcer_set *const s)
{
    carcer_set_init(s);
}

mmgr_bool mmgr_carcer_set_add(mmgr_carcer_set *const s, void *base, size_t size)
{
    return MMGR_CALL(carcer_set_add, CarcerCtx, .s = s, .base = base, .n = size);
}

void *mmgr_carcer_set_persist_capio(mmgr_carcer_set *const s, size_t n)
{
    return MMGR_CALL(carcer_set_persist_capio, CarcerCtx, .s = s, .n = n);
}

void mmgr_carcer_set_persist_reddo(mmgr_carcer_set *const s, void *p)
{
    MMGR_CALL(carcer_set_persist_reddo, CarcerCtx, .s = s, .p = p);
}

void *mmgr_carcer_set_interim_capio_aligned(mmgr_carcer_set *const s, size_t n, size_t align)
{
    return MMGR_CALL(carcer_set_interim_capio_aligned, CarcerCtx, .s = s, .n = n, .align = align);
}

void *mmgr_carcer_set_interim_capio(mmgr_carcer_set *const s, size_t n)
{
    return mmgr_carcer_set_interim_capio_aligned(s, n, MMGR_CARCER_ALIGN);
}

mmgr_carcer_mark mmgr_carcer_set_interim_mark(mmgr_carcer_set *const s)
{
    return MMGR_CALL(carcer_set_interim_mark, CarcerCtx, .s = s);
}

void mmgr_carcer_set_interim_reddo(mmgr_carcer_set *const s, const mmgr_carcer_mark *m)
{
    MMGR_CALL(carcer_set_interim_reddo, CarcerCtx, .s = s, .mark = m);
}

void mmgr_carcer_set_interim_reset(mmgr_carcer_set *const s)
{
    carcer_set_interim_reset(s);
}

size_t mmgr_carcer_set_octas_praesto(mmgr_carcer_set *const s)
{
    return MMGR_CALL(carcer_set_octas_praesto, CarcerCtx, .s = s);
}

size_t mmgr_carcer_set_persist_used(mmgr_carcer_set *const s)
{
    return MMGR_CALL(carcer_set_persist_used, CarcerCtx, .s = s);
}

size_t mmgr_carcer_set_interim_used(mmgr_carcer_set *const s)
{
    return MMGR_CALL(carcer_set_interim_used, CarcerCtx, .s = s);
}
