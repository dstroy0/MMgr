// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_DMA_H
#define MMGR_DMA_H

#include "mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_BEGIN_DECLS

/**
 * @file dma.h
 * @brief DMA transfers, with the hardware behind four weak hooks.
 *
 * Built only when MMGR_ENABLE_DMA is set. A board supplies the hooks; a host build links without
 * them and the weak stubs do nothing.
 */

/** @brief Which peripheral a channel is bound to. */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_DMA_UART = 0,
    MMGR_DMA_I2C = 1,
    MMGR_DMA_SPI = 2,
} mmgr_dma_periph;

/** @brief Transfer direction. */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_DMA_RX = 0,
    MMGR_DMA_TX = 1,
} mmgr_dma_dir;

/**
 * @brief One completed transfer.
 *
 * @c data points into the driver's buffer and is only valid inside the callback.
 */
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

/** @brief Completion callback. */
typedef void (*mmgr_dma_cb)(const mmgr_dma_event *ev, void *ctx);

/** @brief What to open a channel as. */
typedef struct
{
    uint8_t channel;
    mmgr_dma_periph periph;
    mmgr_bool loopback;
    mmgr_dma_cb on_complete;
    void *ctx;
} mmgr_dma_config;

/**
 * @brief Open a channel.
 * @param cfg Configuration.
 * @return MMGR_FALSE if the channel is taken or the hardware refused.
 */
mmgr_bool mmgr_dma_open(const mmgr_dma_config *cfg);

/**
 * @brief Queue a transmit.
 * @param ch Channel.
 * @param buf Data. Must outlive the transfer.
 * @param len Byte count.
 * @return MMGR_FALSE if the channel is closed or busy.
 */
mmgr_bool mmgr_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len);

/**
 * @brief Close a channel.
 * @param ch Channel.
 */
void mmgr_dma_close(uint8_t ch);

/**
 * @brief Service completed transfers and run their callbacks.
 *
 * Nothing is delivered until this is called. There is no interrupt context here.
 */
void mmgr_dma_poll(void);

MMGR_END_DECLS

#endif

#endif
