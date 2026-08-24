#include "carceribus/carceribus.h"

void *mmgr_carcer_persist_capio(const CarcerCfg *c)
{
    CarcerCtx *const w = c->pool;
    void *const pl = w->base + w->persist_end;
    const size_t next_persist = w->persist_end + c->size;

    w->persist_end = next_persist;
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    const size_t hw_mask = 0u - (next_persist > w->hw);
    w->hw = (w->hw & ~hw_mask) | (next_persist & hw_mask);
#endif
    return pl;
}

void mmgr_carcer_persist_reddo(const CarcerCfg *c)
{
    c->pool->persist_end -= c->size;
}

void *mmgr_carcer_interim_capio(const CarcerCfg *c)
{
    CarcerCtx *const w = c->pool;
    const size_t next_top = w->interim_top - c->size;

    w->interim_top = next_top;
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    const size_t used = w->size - next_top;
    const size_t hw_mask = 0u - (used > w->hw);
    w->hw = (w->hw & ~hw_mask) | (used & hw_mask);
#endif
    return w->base + next_top;
}

size_t mmgr_carcer_interim_mark(const CarcerCfg *c)
{
    return c->pool->interim_top;
}

void mmgr_carcer_interim_reddo(const CarcerCfg *c)
{
    c->pool->interim_top = mmgr_carcer_interim_mark(c);
}

void mmgr_carcer_interim_reset(const CarcerCfg *c)
{
    c->pool->interim_top = c->pool->size;
}

mmgr_bool mmgr_carcer_owns(const CarcerCfg *c)
{
    return ((uintptr_t)c->at - (uintptr_t)c->pool->base) < c->pool->size;
}

size_t mmgr_carcer_octas_praesto(const CarcerCfg *c)
{
    return c->pool->interim_top - c->pool->persist_end;
}

size_t mmgr_carcer_persist_used(const CarcerCfg *c)
{
    return c->pool->persist_end;
}
