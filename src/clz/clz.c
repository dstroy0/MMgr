#include "clz/clz.h"


typedef struct
{
    mmgr_u64 x; } ClzCtx;

MMGR_INLINE int clz_lead(const ClzCtx *c)
{
    mmgr_u64 x = c->x;
    mmgr_u64 shift;
    int n = 0;

    shift = (mmgr_u64)((x >> 32) == 0u) << 5;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 48) == 0u) << 4;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 56) == 0u) << 3;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 60) == 0u) << 2;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 62) == 0u) << 1;
    x <<= shift;
    n += (int)shift;
    n += (int)((x >> 63) == 0u);
    return n;
}


int (mmgr_clz_lead)(const ClzCfg *c)
{
    return MMGR_CALL(clz_lead, ClzCtx, .x = c->x);
}
