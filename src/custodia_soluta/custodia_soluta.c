#include "custodia_soluta/custodia_soluta.h"
#include "carceribus/carceribus.h"


#define PLAIN_BLOCK_BYTES ((uintptr_t)MMGR_PLAINTEXT_CONFIN_SIZE)

#define PLAIN_NO_OFFSET (~(uintptr_t)0)

struct PlainStorage
{
    _Alignas(32) uint8_t mem[MMGR_PLAINTEXT_CONFIN_SIZE];
};

struct SolutaInternal
{
    struct PlainStorage *store;
    mmgr_carcer pool;
};

static struct PlainStorage s_storage;

struct SolutaInternal mmgr_soluta_internal;

typedef struct
{
    struct SolutaInternal *pool;     size_t n;                       size_t align;                   size_t mark;                    const void *p;              } SolutaCtx;

MMGR_INLINE void soluta_self(SolutaCtx *c)
{
    c->pool = &mmgr_soluta_internal;
}

MMGR_INLINE mmgr_carcer *soluta_bind(SolutaCtx *c)
{
    soluta_self(c);

    mmgr_carcer *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_storage;
        mmgr_carcer_init(a, c->pool->store->mem, MMGR_PLAINTEXT_CONFIN_SIZE);
    }
    return a;
}

MMGR_INLINE mmgr_carcer *soluta_peek(SolutaCtx *c)
{
    soluta_self(c);

    mmgr_carcer *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

MMGR_INLINE uintptr_t soluta_offset(SolutaCtx *c)
{
    soluta_self(c);

    if (c->pool->store == NULL)
    {
        return PLAIN_NO_OFFSET;
    }
    return (uintptr_t)c->p - (uintptr_t)c->pool->store->mem;
}

MMGR_INLINE void *soluta_capio(SolutaCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "plaintext alignment must be a power of two");
    return mmgr_carcer_interim_capio_aligned(soluta_bind(c), c->n, c->align);
}

MMGR_INLINE void *soluta_persist(SolutaCtx *c)
{
    return mmgr_carcer_persist_capio(soluta_bind(c), c->n);
}

MMGR_INLINE void soluta_reset(SolutaCtx *c)
{
    mmgr_carcer *a = soluta_peek(c);

    if (a != NULL)
    {
        mmgr_carcer_interim_reset(a);
    }
}

MMGR_INLINE size_t soluta_mark(SolutaCtx *c)
{
    return mmgr_carcer_interim_mark(soluta_bind(c));
}

MMGR_INLINE void soluta_reddo(SolutaCtx *c)
{
    mmgr_carcer_interim_reddo(soluta_bind(c), c->mark);
}

MMGR_INLINE size_t soluta_used(SolutaCtx *c)
{
    mmgr_carcer *const a = soluta_peek(c);

    return (a != NULL) ? mmgr_carcer_interim_used(a) : 0;
}

MMGR_INLINE size_t soluta_high_water(SolutaCtx *c)
{
    mmgr_carcer *const a = soluta_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

MMGR_INLINE mmgr_bool soluta_owns(SolutaCtx *c)
{
    return soluta_offset(c) < PLAIN_BLOCK_BYTES;
}

void *mmgr_soluta_capio(size_t n, size_t align)
{
    return MMGR_CALL(soluta_capio, SolutaCtx, .n = n, .align = align);
}

mmgr_spat mmgr_soluta_span(size_t n, size_t align)
{
    return mmgr_spat_init((uint8_t *)mmgr_soluta_capio(n, align), n);
}

mmgr_spat mmgr_soluta_persist_span(size_t n)
{
    return mmgr_spat_init((uint8_t *)MMGR_CALL(soluta_persist, SolutaCtx, .n = n), n);
}

void mmgr_soluta_reset(void)
{
    MMGR_CALL(soluta_reset, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_mark(void)
{
    return MMGR_CALL(soluta_mark, SolutaCtx, .n = 0);
}

void mmgr_soluta_reddo(size_t mark)
{
    MMGR_CALL(soluta_reddo, SolutaCtx, .mark = mark);
}

size_t mmgr_soluta_used(void)
{
    return MMGR_CALL(soluta_used, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_high_water(void)
{
    return MMGR_CALL(soluta_high_water, SolutaCtx, .n = 0);
}

size_t mmgr_soluta_capacity(void)
{
    return MMGR_PLAINTEXT_CONFIN_SIZE;
}

mmgr_bool mmgr_soluta_owns(const void *p)
{
    return MMGR_CALL(soluta_owns, SolutaCtx, .p = p);
}
