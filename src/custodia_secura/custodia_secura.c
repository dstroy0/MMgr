#include "custodia_secura/custodia_secura.h"

typedef struct
{
    CarcerCtx *pool;
    void *at;
    size_t n;
} SecuraCtx;

MMGR_INLINE void secura_wipe(SecuraCtx *c)
{
    uintptr_t *w = (uintptr_t *)c->at;
    size_t words = c->n / sizeof(uintptr_t);
    const uintptr_t z = 0u;

    while (words >= 16u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w[4] = z;
        w[5] = z;
        w[6] = z;
        w[7] = z;
        w[8] = z;
        w[9] = z;
        w[10] = z;
        w[11] = z;
        w[12] = z;
        w[13] = z;
        w[14] = z;
        w[15] = z;
        w += 16;
        words -= 16u;
    }
    if (words & 8u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w[4] = z;
        w[5] = z;
        w[6] = z;
        w[7] = z;
        w += 8;
    }
    if (words & 4u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w += 4;
    }
    if (words & 2u)
    {
        w[0] = z;
        w[1] = z;
        w += 2;
    }
    if (words & 1u)
    {
        w[0] = z;
    }
}

MMGR_INLINE void *secura_init(SecuraCtx *c)
{
    return MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = c->pool, .size = c->n);
}

MMGR_INLINE void secura_reddo(SecuraCtx *c)
{
    secura_wipe(c);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = c->pool, .size = c->n);
}

MMGR_INLINE size_t secura_used(SecuraCtx *c)
{
    return MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = c->pool);
}

void *mmgr_secura_init(const SecuraCfg *c)
{
    return MMGR_CALL(secura_init, SecuraCtx, .pool = c->pool, .n = c->n);
}

void mmgr_secura_reddo(const SecuraCfg *c)
{
    MMGR_CALL(secura_reddo, SecuraCtx, .pool = c->pool, .at = c->at, .n = c->n);
}

size_t mmgr_secura_used(const SecuraCfg *c)
{
    return MMGR_CALL(secura_used, SecuraCtx, .pool = c->pool);
}

void mmgr_secura_wipe(const SecuraCfg *c)
{
    MMGR_CALL(secura_wipe, SecuraCtx, .pool = c->pool, .at = c->at, .n = c->n);
}
