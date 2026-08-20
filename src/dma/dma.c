// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dma/dma.h"

#if MMGR_ENABLE_DMA

__attribute__((weak)) mmgr_bool mmgr_dma_hw_open(const mmgr_dma_config *cfg)
{
    (void)cfg;
    return MMGR_FALSE;
}
__attribute__((weak)) mmgr_bool mmgr_dma_hw_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    (void)ch;
    (void)buf;
    (void)len;
    return MMGR_FALSE;
}
__attribute__((weak)) void mmgr_dma_hw_close(uint8_t ch)
{
    (void)ch;
}
__attribute__((weak)) void mmgr_dma_hw_poll(void)
{
}

mmgr_bool mmgr_dma_open(const mmgr_dma_config *cfg)
{
    if (!cfg || !cfg->on_complete || cfg->channel >= MMGR_DMA_CHANNELS)
    {
        return MMGR_FALSE;
    }
    return mmgr_dma_hw_open(cfg);
}

mmgr_bool mmgr_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    if (ch >= MMGR_DMA_CHANNELS || !buf || len == 0 || len > MMGR_DMA_BUF_SIZE)
    {
        return MMGR_FALSE;
    }
    return mmgr_dma_hw_tx_submit(ch, buf, len);
}

void mmgr_dma_close(uint8_t ch)
{
    if (ch < MMGR_DMA_CHANNELS)
    {
        mmgr_dma_hw_close(ch);
    }
}

void mmgr_dma_poll(void)
{
    mmgr_dma_hw_poll();
}

#endif
