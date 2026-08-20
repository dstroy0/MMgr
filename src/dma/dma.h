// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_DMA_H
#define PROTOCORE_DMA_H

#include "protocore_config.h"

#if PROTOCORE_ENABLE_DMA

PROTOCORE_BEGIN_DECLS

typedef enum PROTO_ENUM_PACKED
{
    PROTOCORE_DMA_UART = 0,
    PROTOCORE_DMA_I2C = 1,
    PROTOCORE_DMA_SPI = 2,
} protocore_dma_periph;

typedef enum PROTO_ENUM_PACKED
{
    PROTOCORE_DMA_RX = 0,
    PROTOCORE_DMA_TX = 1,
} protocore_dma_dir;

typedef struct
{
    const uint8_t *data;
    uint32_t t_ms;
    uint32_t t_us;

    uint16_t len;
    uint16_t seq;
    uint8_t channel;
    protocore_dma_periph periph;
    protocore_dma_dir dir;
    uint8_t _pad;
} protocore_dma_event;

typedef void (*protocore_dma_cb)(const protocore_dma_event *ev, void *ctx);

typedef struct
{
    uint8_t channel;
    protocore_dma_periph periph;
    proto_bool loopback;
    protocore_dma_cb on_complete;
    void *ctx;
} protocore_dma_config;

proto_bool protocore_dma_open(const protocore_dma_config *cfg);

proto_bool protocore_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len);

void protocore_dma_close(uint8_t ch);

void protocore_dma_poll(void);

PROTOCORE_END_DECLS

#endif

#endif
