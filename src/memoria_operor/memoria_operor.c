#include "memoria_operor/memoria_operor.h"

#include "proximus_operor/proximus_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"


typedef struct
{
    unsigned char *restrict dst;        const unsigned char *restrict src;  size_t n;                       } MemorCpyCtx;

typedef struct
{
    unsigned char *dst;         const unsigned char *src;   size_t n;               } MemorMoveCtx;

typedef struct
{
    const unsigned char *src;   const unsigned char *other; size_t n;                   uint8_t v;          } MemorScanCtx;

typedef struct
{
    unsigned char *dst;         size_t n;                   uint8_t v;          } MemorSetCtx;

MMGR_INLINE void memor_cpy(MemorCpyCtx *c)
{
    size_t t = c->n & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->n - t;

    if (w != 0u)
    {
        do
        {
            proxim.mv_put(c->dst, proxim.mv_load(c->src));
            c->dst += MMGR_RAW_WORD;
            c->src += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
    }
    if (t != 0u)
    {
        do
        {
            *c->dst++ = *c->src++;
        } while (--t);
    }
}

MMGR_INLINE void memor_move_up(MemorMoveCtx *c)
{
    size_t t = c->n & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->n - t;

    c->dst += c->n;
    c->src += c->n;

    if (t != 0u)
    {
        do
        {
            *--c->dst = *--c->src;
        } while (--t);
    }
    if (w != 0u)
    {
        do
        {
            c->dst -= MMGR_RAW_WORD;
            c->src -= MMGR_RAW_WORD;
            proxim.mv_put(c->dst, proxim.mv_load(c->src));
            w -= MMGR_RAW_WORD;
        } while (w);
    }
}

MMGR_INLINE int memor_cmp(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->n; at += MMGR_SWAR_BYTES)
    {
        const mmgr_scrut_word d = (mmgr_scrut_word)proxim.load(c->src + at, MMGR_SWAR_BYTES) ^
                                  (mmgr_scrut_word)proxim.load(c->other + at, MMGR_SWAR_BYTES);
        const mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_lanes_below(c->n - at));
        if (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            return (int)c->src[k] - (int)c->other[k];
        }
    }
    return 0;
}

MMGR_INLINE const void *memor_chr(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->n; at += MMGR_SWAR_BYTES)
    {
        const mmgr_scrut_word m =
            (mmgr_scrut_word)(scrut.eq((mmgr_scrut_word)proxim.load(c->src + at, MMGR_SWAR_BYTES), c->v, MMGR_FALSE) &
                              mmgr_scrut_lanes_below(c->n - at));
        if (m != 0)
        {
            return c->src + at + scrut.zero_lane(m);
        }
    }
    return NULL;
}

MMGR_INLINE void memor_set(MemorSetCtx *c)
{
    const mmgr_migro_word fill = (mmgr_migro_word)(MMGR_SWAR_ONES * (mmgr_migro_word)c->v);
    size_t t = c->n & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->n - t;

    if (w != 0u)
    {
        do
        {
            proxim.mv_put(c->dst, fill);
            c->dst += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
    }
    if (t != 0u)
    {
        do
        {
            *c->dst++ = c->v;
        } while (--t);
    }
}

void (mmgr_memor_cpy)(const MemoriaCfg *c)
{
    MMGR_CALL(memor_cpy, MemorCpyCtx, .dst = (unsigned char *)c->dst, .src = (const unsigned char *)c->src, .n = c->n);
}

void (mmgr_memor_move_up)(const MemoriaCfg *c)
{
    MMGR_CALL(memor_move_up, MemorMoveCtx, .dst = (unsigned char *)c->dst, .src = (const unsigned char *)c->src,
              .n = c->n);
}

int (mmgr_memor_cmp)(const MemoriaCfg *c)
{
    return MMGR_CALL(memor_cmp, MemorScanCtx, .src = (const unsigned char *)c->src,
                     .other = (const unsigned char *)c->other, .n = c->n);
}

const void *(mmgr_memor_chr)(const MemoriaCfg *c)
{
    return MMGR_CALL(memor_chr, MemorScanCtx, .src = (const unsigned char *)c->src, .n = c->n, .v = c->v);
}

void (mmgr_memor_set)(const MemoriaCfg *c)
{
    MMGR_CALL(memor_set, MemorSetCtx, .dst = (unsigned char *)c->dst, .n = c->n, .v = c->v);
}
