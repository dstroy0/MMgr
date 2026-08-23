#include "proximus_operor/proximus_operor.h"


typedef struct
{
    unsigned char *d;           const unsigned char *u;     size_t sz;                  size_t i;               } ProximCtx;

MMGR_INLINE void proxim_head(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);

    while ((c->i < c->sz) && (((uintptr_t)(c->d + c->i) & mask) != 0u))
    {
        c->d[c->i] = c->u[c->i];
        c->i++;
    }
}

MMGR_INLINE void proxim_aligned(ProximCtx *c)
{
    while ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        mmgr_migro_put(c->d + c->i, mmgr_migro_load(c->u + c->i));
        c->i += MMGR_RAW_WORD;
    }
}

MMGR_INLINE void proxim_straddled(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);
    const size_t off = (size_t)((uintptr_t)(c->u + c->i) & mask);
    const unsigned char *sa = (c->u + c->i) - off;
    const unsigned lo = (unsigned)(off * 8u);
    const unsigned hi = (unsigned)(MMGR_MV_BITS - (off * 8u));
    mmgr_migro_word prev = mmgr_migro_load(sa);

    while ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        sa += MMGR_RAW_WORD;
        const mmgr_migro_word cur = mmgr_migro_load(sa);
#if MMGR_HW_BIG_ENDIAN
        mmgr_migro_put(c->d + c->i, (mmgr_migro_word)((prev << lo) | (cur >> hi)));
#else
        mmgr_migro_put(c->d + c->i, (mmgr_migro_word)((prev >> lo) | (cur << hi)));
#endif
        prev = cur;
        c->i += MMGR_RAW_WORD;
    }
}

MMGR_INLINE void proxim_tail(ProximCtx *c)
{
    while (c->i < c->sz)
    {
        c->d[c->i] = c->u[c->i];
        c->i++;
    }
}

MMGR_INLINE void proxim_read(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);

    proxim_head(c);

    if (((uintptr_t)(c->u + c->i) & mask) == 0u)
    {
        proxim_aligned(c);
    }
    else if ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        proxim_straddled(c);
    }

    proxim_tail(c);
}


void mmgr_proxim_read(void *dst, const void *p, size_t sz)
{
    MMGR_CALL(proxim_read, ProximCtx, .d = (unsigned char *)dst, .u = (const unsigned char *)p, .sz = sz);
}
