#include "proximus_operor/proximus_operor.h"

typedef uint16_t mmgr_proxim_u16_t MMGR_RAW;
typedef uint32_t mmgr_proxim_u32_t MMGR_RAW;
typedef uint64_t mmgr_proxim_u64_t MMGR_RAW;

typedef uint64_t mmgr_aequus_u64_t MMGR_ALIAS;

typedef mmgr_migro_word mmgr_proxim_word_t MMGR_RAW;
typedef mmgr_migro_word mmgr_aequus_word_t MMGR_ALIAS;

typedef struct
{
    const uint8_t *at;
} ProximLoadCtx;

typedef struct
{
    uint8_t *dst;
    uint64_t val;
} ProximPutCtx;

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    size_t bytes;
} ProximReadCtx;

MMGR_INLINE uint16_t proxim_load16(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u16_t *)c->at;
}

MMGR_INLINE uint32_t proxim_load32(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u32_t *)c->at;
}

MMGR_INLINE uint64_t proxim_load64(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u64_t *)c->at;
}

MMGR_INLINE mmgr_migro_word proxim_load(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_word_t *)c->at;
}

MMGR_INLINE mmgr_migro_word aequus_load(const ProximLoadCtx *c)
{
    return *(const mmgr_aequus_word_t *)c->at;
}

MMGR_INLINE uint64_t aequus_load64(const ProximLoadCtx *c)
{
    return *(const mmgr_aequus_u64_t *)c->at;
}

MMGR_INLINE void proxim_put16(const ProximPutCtx *c)
{
    *(mmgr_proxim_u16_t *)c->dst = (uint16_t)c->val;
}

MMGR_INLINE void proxim_put32(const ProximPutCtx *c)
{
    *(mmgr_proxim_u32_t *)c->dst = (uint32_t)c->val;
}

MMGR_INLINE void proxim_put64(const ProximPutCtx *c)
{
    *(mmgr_proxim_u64_t *)c->dst = c->val;
}

MMGR_INLINE void proxim_put(const ProximPutCtx *c)
{
    *(mmgr_proxim_word_t *)c->dst = (mmgr_migro_word)c->val;
}

MMGR_INLINE void aequus_put(const ProximPutCtx *c)
{
    *(mmgr_aequus_word_t *)c->dst = (mmgr_migro_word)c->val;
}

MMGR_INLINE void aequus_put64(const ProximPutCtx *c)
{
    *(mmgr_aequus_u64_t *)c->dst = c->val;
}

MMGR_INLINE void proxim_head(ProximReadCtx *c)
{
    const size_t skew = (size_t)((0u - (uintptr_t)c->dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    size_t t = (skew < c->bytes) ? skew : c->bytes;

    if (t == 0u)
    {
        return;
    }
    c->bytes -= t;

    do
    {
        *c->dst++ = *c->src++;
    } while (--t);
}

MMGR_INLINE void proxim_words(ProximReadCtx *c)
{
    size_t w = c->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);
    if (w == 0u)
    {
        return;
    }
    c->bytes -= w;
    do
    {
        *(mmgr_aequus_word_t *)c->dst = *(const mmgr_proxim_word_t *)c->src;
        c->dst += MMGR_RAW_WORD;
        c->src += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);
}

MMGR_INLINE void proxim_tail(ProximReadCtx *c)
{
    size_t t = c->bytes;

    if (t == 0u)
    {
        return;
    }

    do
    {
        *c->dst++ = *c->src++;
    } while (--t);
}

MMGR_INLINE void proxim_read(ProximReadCtx *c)
{
    proxim_head(c);
    proxim_words(c);
    proxim_tail(c);
}

uint16_t mmgr_proxim_load16(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load16, ProximLoadCtx, .at = c->at);
}

uint32_t mmgr_proxim_load32(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load32, ProximLoadCtx, .at = c->at);
}

uint64_t mmgr_proxim_load64(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load64, ProximLoadCtx, .at = c->at);
}

void mmgr_proxim_put16(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put16, ProximPutCtx, .dst = c->dst, .val = c->val);
}

void mmgr_proxim_put32(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put32, ProximPutCtx, .dst = c->dst, .val = c->val);
}

void mmgr_proxim_put64(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put64, ProximPutCtx, .dst = c->dst, .val = c->val);
}

mmgr_migro_word mmgr_proxim_load(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load, ProximLoadCtx, .at = c->at);
}

void mmgr_proxim_put(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put, ProximPutCtx, .dst = c->dst, .val = c->val);
}

mmgr_migro_word mmgr_aequus_load(const ProximusCfg *c)
{
    return MMGR_CALL(aequus_load, ProximLoadCtx, .at = c->at);
}

void mmgr_aequus_put(const ProximusCfg *c)
{
    MMGR_CALL(aequus_put, ProximPutCtx, .dst = c->dst, .val = c->val);
}

uint64_t mmgr_aequus_load64(const ProximusCfg *c)
{
    return MMGR_CALL(aequus_load64, ProximLoadCtx, .at = c->at);
}

void mmgr_aequus_put64(const ProximusCfg *c)
{
    MMGR_CALL(aequus_put64, ProximPutCtx, .dst = c->dst, .val = c->val);
}

void mmgr_proxim_read(const ProximusCfg *c)
{
    MMGR_CALL(proxim_read, ProximReadCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->at, .bytes = c->size);
}
