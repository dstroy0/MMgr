/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief DMA channels: the completion event, the port hooks, and the praet dispatch table.
 *
 * @warning Everything below is declared only when MMGR_ENABLE_DMA is set.
 */
#ifndef MMGR_MEMORIAM_PRAETEREO_H
#define MMGR_MEMORIAM_PRAETEREO_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_INCIPE_DECLS

/**
 * @brief What the port layer reports when a transfer finishes.
 *
 * @warning data points at the port layer's buffer; it is valid only for the callback's duration [BORROWS].
 */
typedef struct
{
    const uint8_t *data; /**< Bytes the transfer moved [BORROWS]. */
    uint32_t t_ms;       /**< Completion time, whole milliseconds. */
    uint32_t t_us;       /**< Completion time, microseconds within that millisecond. */
    uint16_t len;        /**< Bytes in data. */
    uint16_t seq;        /**< Sequence number the port layer assigns. */
    uint8_t channel;     /**< Channel the transfer ran on. */
    uint8_t periph;      /**< Peripheral the channel is wired to. */
    uint8_t dir;         /**< Direction of the transfer. */
} mmgr_praet_event;

/**
 * @brief Called by the port layer when a transfer finishes.
 *
 * @param[in] ev   The completion event [BORROWS].
 * @param[in] user The pointer registered alongside this callback [BORROWS].
 */
typedef void (*mmgr_praet_cb)(const mmgr_praet_event *ev, void *user);

/**
 * @brief The channel count and buffer size a build was configured with.
 *
 * @note The implementation holds one of these, filled from MMGR_PRAET_CHANNELS and MMGR_PRAET_BUF_SIZE.
 */
typedef struct
{
    const size_t channels; /**< Channels available. */
    const size_t buf_size; /**< Largest transfer one channel accepts. */
} PraetInit;

/**
 * @brief A completion callback and the pointer handed back to it.
 */
typedef struct
{
    const mmgr_praet_cb fn; /**< Function to call [BORROWS]. */
    void *const user;       /**< Passed back to fn unexamined [BORROWS]. */
} PraetCallbackCfg;

/**
 * @brief Arguments for opening a channel, and for polling one.
 *
 * @note open reads all four members; poll passes the whole struct to the port layer.
 */
typedef struct
{
    const uint8_t channel;                     /**< Channel to act on. */
    const uint8_t periph;                      /**< Peripheral the channel is wired to. */
    const mmgr_bool loopback;                  /**< Open the channel looped back on itself. */
    const PraetCallbackCfg *const on_complete; /**< Called when a transfer finishes [BORROWS]. */
} PraetCfg;

/**
 * @brief Arguments for submitting a transfer, and for closing a channel.
 *
 * @note tx_submit reads all three; close reads channel alone.
 */
typedef struct
{
    const uint8_t channel;    /**< Channel to act on. */
    const uint8_t *const buf; /**< Bytes to send [BORROWS]. */
    const uint16_t len;       /**< Bytes in buf. */
} PraetTransferCfg;

/**
 * @brief Type of the praet dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the four members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_bool (*open)(const PraetCfg *c);              /**< Opens a channel. */
    mmgr_bool (*tx_submit)(const PraetTransferCfg *c); /**< Submits a transfer. */
    void (*close)(const PraetTransferCfg *c);          /**< Closes a channel. */
    void (*poll)(const PraetCfg *c);                   /**< Drives the port layer's poll hook. */
} MemoriamPraetereoNs;
MMGR_NS_LAYOUT(MemoriamPraetereoNs, open, tx_submit, close, poll);

/**
 * @brief Opens c->channel and registers c->on_complete against it.
 *
 * @param[in] c Channel, peripheral, loopback flag and completion callback [BORROWS].
 * @return      MMGR_TRUE when the port layer accepted the request.
 * @note The default mmgr_praet_hw_open refuses, so this returns MMGR_FALSE until a port replaces it.
 * @warning c->channel must be below the configured channel count, and c->on_complete must not be NULL.
 */
mmgr_bool mmgr_praet_open(const PraetCfg *c);

/**
 * @brief Submits c->len bytes of c->buf on c->channel.
 *
 * @param[in] c Channel, buffer and length [BORROWS].
 * @return      MMGR_TRUE when the port layer accepted the transfer.
 * @note The default mmgr_praet_hw_tx_submit refuses, so this returns MMGR_FALSE until a port replaces it.
 * @warning c->buf must stay valid until the completion callback runs [BORROWS].
 * @warning c->channel must be below the configured channel count, and c->len must not exceed the buffer size.
 */
mmgr_bool mmgr_praet_tx_submit(const PraetTransferCfg *c);

/**
 * @brief Closes c->channel.
 *
 * @param[in] c Channel to close [BORROWS].
 * @note Only c->channel is read; buf and len take no part.
 * @warning c->channel must be below the configured channel count.
 */
void mmgr_praet_close(const PraetTransferCfg *c);

/**
 * @brief Drives the port layer's poll hook.
 *
 * @param[in] c Channel to poll [BORROWS].
 * @note Passes c straight through without checking it, unlike the other three entries.
 */
void mmgr_praet_poll(const PraetCfg *c);

/**
 * @brief The four port hooks an application defines to drive real hardware.
 *
 * @note Each has a default in memoriam_praetereo.c that refuses or does nothing, so a build links without a port.
 * @note An application definition of any of these names replaces the default where MMGR_HAS_ATTRIBUTE(weak) is non-zero.
 * @warning mmgr_praet_poll calls mmgr_praet_hw_poll directly; the other three pass through a call that asserts first.
 */
mmgr_bool mmgr_praet_hw_open(const PraetCfg *c);

mmgr_bool mmgr_praet_hw_tx_submit(const PraetTransferCfg *c);

void mmgr_praet_hw_close(const PraetTransferCfg *c);

void mmgr_praet_hw_poll(const PraetCfg *c);

/**
 * @brief Dispatch table instance named praet; each member calls the matching mmgr_praet_ function.
 */
MMGR_NS MemoriamPraetereoNs praet MMGR_UNUSED = {
    .open = mmgr_praet_open,
    .tx_submit = mmgr_praet_tx_submit,
    .close = mmgr_praet_close,
    .poll = mmgr_praet_poll,
};

MMGR_FINIS_DECLS

#endif

#endif
