// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_DMA_H
#define MMGR_DMA_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_INCIPE_DECLS

/**
 * @file dma.h
 * @brief DMA transfers, with the hardware behind four weak hooks.
 *
 * Built only when MMGR_ENABLE_DMA is set. A board supplies the hooks; a host build links without
 * them and the weak stubs do nothing.
 *
 * The table is the whole surface. There are no free functions to call.
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

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_bool (*open)(const mmgr_dma_config *cfg);
    mmgr_bool (*tx_submit)(uint8_t ch, const uint8_t *buf, uint16_t len);
    void (*close)(uint8_t ch);
    void (*poll)(void);
} DmaNs;
MMGR_NS_LAYOUT(DmaNs, open, tx_submit, close, poll);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *
 *  Nothing is delivered until poll is called. There is no interrupt context here.
 *  @{ */
mmgr_bool mmgr_dma_open(const mmgr_dma_config *cfg);
mmgr_bool mmgr_dma_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len);
void mmgr_dma_close(uint8_t ch);
void mmgr_dma_poll(void);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS DmaNs dma MMGR_UNUSED = {
    .open = mmgr_dma_open,
    .tx_submit = mmgr_dma_tx_submit,
    .close = mmgr_dma_close,
    .poll = mmgr_dma_poll,
};

MMGR_FINIS_DECLS

#endif

#endif
