// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dma/dma.h"

/**
 * @file dma.c
 * @brief DMA driver. The four hardware hooks are weak so a board can override them and a host build
 *        links without one.
 *
 * Every entry below takes one parameter, a pointer to DmaCtx. What a caller asks of a channel is
 * one request, so its parts are one context.
 *
 * The hooks keep their own shapes, because they are the board's to implement and not this file's to
 * decide.
 */

#if MMGR_ENABLE_DMA

/** @brief One request of a channel. */
typedef struct
{
    uint8_t ch;                 /**< The channel. */
    const uint8_t *buf;         /**< The bytes, when submitting. */
    uint16_t len;               /**< How many. */
} DmaCtx;

MMGR_WEAK mmgr_bool mmgr_dma_hw_open(const mmgr_dma_config *cfg)
{
    (void)cfg;
    return MMGR_FALSE;
}
MMGR_WEAK mmgr_bool mmgr_dma_hw_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    (void)ch;
    (void)buf;
    (void)len;
    return MMGR_FALSE;
}
MMGR_WEAK void mmgr_dma_hw_close(uint8_t ch)
{
    (void)ch;
}
MMGR_WEAK void mmgr_dma_hw_poll(void)
{
}

/**
 * @brief Open a channel.
 * @param cfg The configuration.
 * @return MMGR_FALSE if the configuration cannot be honoured.
 */
MMGR_INLINE mmgr_bool dma_open(const mmgr_dma_config *cfg)
{
    if ((cfg == NULL) || (cfg->on_complete == NULL) || (cfg->channel >= MMGR_DMA_CHANNELS))
    {
        return MMGR_FALSE;
    }
    return mmgr_dma_hw_open(cfg);
}

/**
 * @brief Hand a run of bytes to a channel.
 * @param c The request.
 * @return MMGR_FALSE if the channel or the run is not one this accepts.
 */
MMGR_INLINE mmgr_bool dma_tx_submit(const DmaCtx *c)
{
    if ((c->ch >= MMGR_DMA_CHANNELS) || (c->buf == NULL) || (c->len == 0u) || (c->len > MMGR_DMA_BUF_SIZE))
    {
        return MMGR_FALSE;
    }
    return mmgr_dma_hw_tx_submit(c->ch, c->buf, c->len);
}

/**
 * @brief Close a channel.
 * @param ch The channel.
 */
MMGR_INLINE void dma_close(uint8_t ch)
{
    if (ch < MMGR_DMA_CHANNELS)
    {
        mmgr_dma_hw_close(ch);
    }
}

mmgr_bool mmgr_dma_open(const mmgr_dma_config *cfg)
{
    return dma_open(cfg);
}

mmgr_bool mmgr_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    return MMGR_CALL(dma_tx_submit, DmaCtx, .ch = ch, .buf = buf, .len = len);
}

void mmgr_dma_close(uint8_t ch)
{
    dma_close(ch);
}

void mmgr_dma_poll(void)
{
    mmgr_dma_hw_poll();
}

#endif
