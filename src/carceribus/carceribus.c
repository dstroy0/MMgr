/**
 * @brief Double-ended bump allocator: persistent bytes grow up from base, interim bytes grow down.
 */
#include "carceribus/carceribus.h"

/**
 * @brief Returns base plus the old persist_end, then advances persist_end by c->size.
 *
 * @note Documented at the declaration in carceribus.h.
 */
void *mmgr_carcer_persist_capio(const CarcerCfg *c)
{
    CarcerCtx *const w = c->pool;
    void *const pl = w->base + w->persist_end;
    const size_t next_persist = w->persist_end + c->size;

    w->persist_end = next_persist;
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    // Branchless max: the comparison becomes a mask that selects the larger of the two
    const size_t hw_mask = 0u - (next_persist > w->hw);
    w->hw = (w->hw & ~hw_mask) | (next_persist & hw_mask);
#endif
    return pl;
}

/**
 * @brief Moves persist_end back by c->size.
 *
 * @note Documented at the declaration in carceribus.h.
 */
void mmgr_carcer_persist_reddo(const CarcerCfg *c)
{
    c->pool->persist_end -= c->size;
}

/**
 * @brief Lowers interim_top by c->size and returns base plus the new interim_top.
 *
 * @note Documented at the declaration in carceribus.h.
 */
void *mmgr_carcer_interim_capio(const CarcerCfg *c)
{
    CarcerCtx *const w = c->pool;
    const size_t next_top = w->interim_top - c->size;

    w->interim_top = next_top;
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    // used is the bytes taken from the top; the mask then selects the larger of the two
    const size_t used = w->size - next_top;
    const size_t hw_mask = 0u - (used > w->hw);
    w->hw = (w->hw & ~hw_mask) | (used & hw_mask);
#endif
    return w->base + next_top;
}

/**
 * @brief Returns the current interim_top.
 *
 * @note Documented at the declaration in carceribus.h.
 */
size_t mmgr_carcer_interim_mark(const CarcerCfg *c)
{
    return c->pool->interim_top;
}

/**
 * @brief Sets interim_top to the value mmgr_carcer_interim_mark reports.
 *
 * @note Documented at the declaration in carceribus.h.
 */
void mmgr_carcer_interim_reddo(const CarcerCfg *c)
{
    c->pool->interim_top = mmgr_carcer_interim_mark(c);
}

/**
 * @brief Sets interim_top to the pool size, releasing every interim allocation.
 *
 * @note Documented at the declaration in carceribus.h.
 */
void mmgr_carcer_interim_reset(const CarcerCfg *c)
{
    c->pool->interim_top = c->pool->size;
}

/**
 * @brief Returns whether c->at lies inside the pool's bytes.
 *
 * @note Documented at the declaration in carceribus.h.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCfg *c)
{
    // Explicit casts to uintptr_t let one unsigned compare cover both ends: below base wraps high
    return ((uintptr_t)c->at - (uintptr_t)c->pool->base) < c->pool->size;
}

/**
 * @brief Returns the bytes between persist_end and interim_top.
 *
 * @note Documented at the declaration in carceribus.h.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCfg *c)
{
    return c->pool->interim_top - c->pool->persist_end;
}

/**
 * @brief Returns persist_end, the bytes taken from the bottom.
 *
 * @note Documented at the declaration in carceribus.h.
 */
size_t mmgr_carcer_persist_used(const CarcerCfg *c)
{
    return c->pool->persist_end;
}
