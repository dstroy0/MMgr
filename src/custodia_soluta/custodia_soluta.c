#include "custodia_soluta/custodia_soluta.h"

typedef struct
{
    CarcerCtx *pool;
    void *at;
    size_t bytes;
} SolutaCtx;

MMGR_INLINE void *soluta_init(SolutaCtx *c)
{
    return MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

MMGR_INLINE void soluta_reddo(SolutaCtx *c)
{
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

MMGR_INLINE size_t soluta_used(SolutaCtx *c)
{
    return MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = c->pool);
}

void *mmgr_soluta_init(const SolutaCfg *c)
{
    return MMGR_CALL(soluta_init, SolutaCtx, .pool = c->pool, .bytes = c->bytes);
}

void mmgr_soluta_reddo(const SolutaCfg *c)
{
    MMGR_CALL(soluta_reddo, SolutaCtx, .pool = c->pool, .bytes = c->bytes);
}

size_t mmgr_soluta_used(const SolutaCfg *c)
{
    return MMGR_CALL(soluta_used, SolutaCtx, .pool = c->pool);
}
