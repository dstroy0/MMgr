#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

typedef struct
{
    uint8_t *const out;
    const size_t cap;
    size_t cnt;
    uint32_t acc;
    mmgr_iword nbits;
    mmgr_bool overflow;
} BitorCtx;

MMGR_INLINE void bitor_put(BitorCtx *c)
{
    if (c->overflow)
    {
        return;
    }

    const size_t whole = (size_t)c->nbits / 8u;

    if (whole > (c->cap - c->cnt))
    {
        c->overflow = MMGR_TRUE;
        c->nbits = 0;
        c->acc = 0;
        return;
    }

    uint32_t acc = c->acc;

    for (size_t i = 0; i < whole; i++)
    {
        c->out[c->cnt + i] = (uint8_t)(acc & 0xFFu);
        acc >>= 8;
    }

    c->cnt += whole;
    c->nbits -= (mmgr_iword)(whole * 8u);
    c->acc = acc;
}

void mmgr_bitor_put(BitorumCfg *c)
{
    BitorCtx x = {
        .out = c->out, .cap = c->cap, .cnt = c->cnt, .acc = c->acc, .nbits = c->nbits, .overflow = c->overflow};

    bitor_put(&x);

    c->cnt = x.cnt;
    c->acc = x.acc;
    c->nbits = x.nbits;
    c->overflow = x.overflow;
}
