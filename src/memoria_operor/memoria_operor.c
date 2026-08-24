#include "memoria_operor/memoria_operor.h"

#include "proximus_operor/proximus_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

typedef struct
{
    uint8_t *restrict dst;
    const uint8_t *restrict src;
    size_t bytes;
} MemorCpyCtx;

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    size_t bytes;
} MemorMoveCtx;

typedef struct
{
    const uint8_t *src;
    const uint8_t *other;
    size_t bytes;
    uint8_t val;
} MemorScanCtx;

typedef struct
{
    uint8_t *dst;
    size_t bytes;
    uint8_t val;
} MemorSetCtx;

MMGR_INLINE void memor_cpy(MemorCpyCtx *c)
{
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    if (w != 0u)
    {
        do
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst,
                      .val = MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->src));
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
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    c->dst += c->bytes;
    c->src += c->bytes;

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
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst,
                      .val = MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->src));
            w -= MMGR_RAW_WORD;
        } while (w);
    }
}

MMGR_INLINE mmgr_iword memor_cmp(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->bytes; at += MMGR_SWAR_BYTES)
    {
        const mmgr_word d = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at) ^
                                  MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word m =
            (mmgr_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                              MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = c->bytes - at));
        if (m != 0)
        {
            const size_t k = at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
            return (mmgr_iword)c->src[k] - (mmgr_iword)c->other[k];
        }
    }
    return 0;
}

MMGR_INLINE const void *memor_chr(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->bytes; at += MMGR_SWAR_BYTES)
    {
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word m =
            (mmgr_word)(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = c->val, .ci = MMGR_FALSE) &
                              MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = c->bytes - at));
        if (m != 0)
        {
            return c->src + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return NULL;
}

MMGR_INLINE void memor_set(MemorSetCtx *c)
{
    const mmgr_migro_word fill = (mmgr_migro_word)(MMGR_SWAR_ONES * (mmgr_migro_word)c->val);
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    if (w != 0u)
    {
        do
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst, .val = fill);
            c->dst += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
    }
    if (t != 0u)
    {
        do
        {
            *c->dst++ = c->val;
        } while (--t);
    }
}

void mmgr_memor_cpy(const MemoriaCfg *c)
{
    MMGR_CALL(memor_cpy, MemorCpyCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->src, .bytes = c->bytes);
}

void mmgr_memor_move_up(const MemoriaCfg *c)
{
    MMGR_CALL(memor_move_up, MemorMoveCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->src, .bytes = c->bytes);
}

mmgr_iword mmgr_memor_cmp(const MemoriaCfg *c)
{
    return MMGR_CALL(memor_cmp, MemorScanCtx, .src = (const uint8_t *)c->src, .other = (const uint8_t *)c->other,
                     .bytes = c->bytes);
}

const void *mmgr_memor_chr(const MemoriaCfg *c)
{
    return MMGR_CALL(memor_chr, MemorScanCtx, .src = (const uint8_t *)c->src, .bytes = c->bytes, .val = c->val);
}

void mmgr_memor_set(const MemoriaCfg *c)
{
    MMGR_CALL(memor_set, MemorSetCtx, .dst = (uint8_t *)c->dst, .bytes = c->bytes, .val = c->val);
}
