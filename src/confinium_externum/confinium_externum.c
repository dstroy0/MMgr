#include "confinium_externum/confinium_externum.h"

#if MMGR_ENABLE_EXTRAM

typedef struct
{
    size_t size;
    mmgr_bool dma_required;
    size_t free_dram;
    size_t free_psram;
    size_t psram_threshold;
    size_t dram_reserve;
} ExterCtx;

MMGR_INLINE mmgr_bool exter_dram_fits(const ExterCtx *c)
{
    return (c->size <= c->free_dram) && ((c->free_dram - c->size) >= c->dram_reserve);
}

MMGR_INLINE mmgr_bool exter_psram_fits(const ExterCtx *c)
{
    return c->size <= c->free_psram;
}

MMGR_INLINE mmgr_place exter_place(const ExterCtx *c)
{
    if (c->size == 0)
    {
        return PLACE_FAIL;
    }

    const mmgr_bool d_fits = exter_dram_fits(c);
    const mmgr_bool p_fits = exter_psram_fits(c);

    if (c->dma_required)
    {
        return d_fits ? PLACE_DRAM : PLACE_FAIL;
    }

    if (c->size >= c->psram_threshold)
    {
        if (p_fits)
        {
            return PLACE_PSRAM;
        }
        if (d_fits)
        {
            return PLACE_DRAM;
        }
        return PLACE_FAIL;
    }

    if (d_fits)
    {
        return PLACE_DRAM;
    }
    if (p_fits)
    {
        return PLACE_PSRAM;
    }
    return PLACE_FAIL;
}

MMGR_INLINE void exter_pingpong_init(PingPong *const pp)
{
    pp->fill_idx = 0;
}

MMGR_INLINE uint8_t exter_pingpong_fill(PingPong *const pp)
{
    return pp->fill_idx;
}

MMGR_INLINE uint8_t exter_pingpong_drain(PingPong *const pp)
{
    return (uint8_t)(pp->fill_idx ^ 1u);
}

MMGR_INLINE uint8_t exter_pingpong_swap(PingPong *const pp)
{
    pp->fill_idx ^= 1u;
    return pp->fill_idx;
}

mmgr_place mmgr_exter_place(const ExternumCfg *c)
{
    return MMGR_CALL(exter_place, ExterCtx, .size = c->size, .dma_required = c->dma_required, .free_dram = c->free_dram,
                     .free_psram = c->free_psram, .psram_threshold = c->psram_threshold,
                     .dram_reserve = c->dram_reserve);
}

void mmgr_pingpong_init(PingPong *const pp)
{
    exter_pingpong_init(pp);
}

uint8_t mmgr_pingpong_fill_index(PingPong *const pp)
{
    return exter_pingpong_fill(pp);
}

uint8_t mmgr_pingpong_drain_index(PingPong *const pp)
{
    return exter_pingpong_drain(pp);
}

uint8_t mmgr_pingpong_swap(PingPong *const pp)
{
    return exter_pingpong_swap(pp);
}

#endif
