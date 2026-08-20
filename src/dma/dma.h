// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_DMA_H
#define MMGR_DMA_H

#include "mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_BEGIN_DECLS

typedef enum MMGR_ENUM_PACKED
{
    MMGR_DMA_UART = 0,
    MMGR_DMA_I2C = 1,
    MMGR_DMA_SPI = 2,
} mmgr_dma_periph;

typedef enum MMGR_ENUM_PACKED
{
    MMGR_DMA_RX = 0,
    MMGR_DMA_TX = 1,
} mmgr_dma_dir;

typedef struct
{
    const uint8_t *data;
    uint32_t t_ms;
    uint32_t t_us;

    uint16_t len;
    uint16_t seq;
    uint8_t channel;
    mmgr_dma_periph periph;
    mmgr_dma_dir dir;
    uint8_t _pad;
} mmgr_dma_event;

typedef void (*mmgr_dma_cb)(const mmgr_dma_event *ev, void *ctx);

typedef struct
{
    uint8_t channel;
    mmgr_dma_periph periph;
    mmgr_bool loopback;
    mmgr_dma_cb on_complete;
    void *ctx;
} mmgr_dma_config;

mmgr_bool mmgr_dma_open(const mmgr_dma_config *cfg);

mmgr_bool mmgr_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len);

void mmgr_dma_close(uint8_t ch);

void mmgr_dma_poll(void);

MMGR_END_DECLS

#endif

#endif
