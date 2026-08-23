#include "custodia_secura/custodia_secura.h"
#include "carceribus/carceribus.h"


#define SEC_BLOCK_BYTES ((uintptr_t)MMGR_SECURE_CONFIN_SIZE)

#define SEC_NO_OFFSET (~(uintptr_t)0)

struct SecureStorage
{
    _Alignas(32) uint8_t mem[MMGR_SECURE_CONFIN_SIZE];
};

struct SecuraInternal
{
    struct SecureStorage *store;
    mmgr_carcer pool;
};

static struct SecureStorage s_store;

struct SecuraInternal mmgr_secura_state;

typedef struct
{
    struct SecuraInternal *pool;     size_t n;                        size_t align;                    size_t mark;                     const void *p;               } SecuraCtx;

MMGR_INLINE void secura_self(SecuraCtx *c)
{
    c->pool = &mmgr_secura_state;
}

MMGR_INLINE mmgr_carcer *secura_bind(SecuraCtx *c)
{
    secura_self(c);

    mmgr_carcer *a = &c->pool->pool;
    if (a->base == NULL)
    {
        c->pool->store = &s_store;
        mmgr_carcer_init(a, c->pool->store->mem, MMGR_SECURE_CONFIN_SIZE);
    }
    return a;
}

MMGR_INLINE mmgr_carcer *secura_peek(SecuraCtx *c)
{
    secura_self(c);

    mmgr_carcer *a = &c->pool->pool;
    return (a->base != NULL) ? a : NULL;
}

MMGR_INLINE uintptr_t secura_offset(SecuraCtx *c)
{
    secura_self(c);

    if (c->pool->store == NULL)
    {
        return SEC_NO_OFFSET;
    }
    return (uintptr_t)c->p - (uintptr_t)c->pool->store->mem;
}

MMGR_INLINE void secura_wipe_down_to(SecuraCtx *c)
{
    mmgr_carcer *a = secura_bind(c);
    const size_t top = mmgr_carcer_interim_mark(a);

    if ((c->mark > top) && (c->mark <= a->size))
    {
        mmgr_secura_wipe(a->base + top, c->mark - top);
    }
    mmgr_carcer_interim_reddo(a, c->mark);
}

MMGR_INLINE void *secura_capio(SecuraCtx *c)
{
    MMGR_ASSERT((c->align & (c->align - 1)) == 0, "secure alignment must be a power of two");
    return mmgr_carcer_interim_capio_aligned(secura_bind(c), c->n, c->align);
}

MMGR_INLINE void *secura_persist(SecuraCtx *c)
{
    return mmgr_carcer_persist_capio(secura_bind(c), c->n);
}

MMGR_INLINE size_t secura_mark(SecuraCtx *c)
{
    return mmgr_carcer_interim_mark(secura_bind(c));
}

MMGR_INLINE void secura_reset(SecuraCtx *c)
{
    mmgr_carcer *a = secura_peek(c);

    if (a != NULL)
    {
        c->mark = a->size;
        secura_wipe_down_to(c);
    }
}

MMGR_INLINE size_t secura_used(SecuraCtx *c)
{
    mmgr_carcer *const a = secura_peek(c);

    return (a != NULL) ? mmgr_carcer_interim_used(a) : 0;
}

MMGR_INLINE size_t secura_high_water(SecuraCtx *c)
{
    mmgr_carcer *const a = secura_peek(c);

    return (a != NULL) ? a->scratch_hw : 0;
}

MMGR_INLINE mmgr_bool secura_owns(SecuraCtx *c)
{
    return secura_offset(c) < SEC_BLOCK_BYTES;
}

void *mmgr_secura_capio(size_t n, size_t align)
{
    return MMGR_CALL(secura_capio, SecuraCtx, .n = n, .align = align);
}

mmgr_spat mmgr_secura_span(size_t n, size_t align)
{
    return mmgr_spat_init((uint8_t *)mmgr_secura_capio(n, align), n);
}

mmgr_spat mmgr_secura_persist_span(size_t n)
{
    return mmgr_spat_init((uint8_t *)MMGR_CALL(secura_persist, SecuraCtx, .n = n), n);
}

size_t mmgr_secura_mark(void)
{
    return MMGR_CALL(secura_mark, SecuraCtx, .n = 0);
}

void mmgr_secura_reddo(size_t mark)
{
    MMGR_CALL(secura_wipe_down_to, SecuraCtx, .mark = mark);
}

void mmgr_secura_reset(void)
{
    MMGR_CALL(secura_reset, SecuraCtx, .n = 0);
}

size_t mmgr_secura_used(void)
{
    return MMGR_CALL(secura_used, SecuraCtx, .n = 0);
}

size_t mmgr_secura_high_water(void)
{
    return MMGR_CALL(secura_high_water, SecuraCtx, .n = 0);
}

size_t mmgr_secura_capacity(void)
{
    return MMGR_SECURE_CONFIN_SIZE;
}

mmgr_bool mmgr_secura_owns(const void *p)
{
    return MMGR_CALL(secura_owns, SecuraCtx, .p = p);
}
