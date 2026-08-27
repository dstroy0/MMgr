/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief DMA channel handling, over a weak port layer an application replaces.
 *
 * @warning The whole file is compiled only when MMGR_ENABLE_DMA is set.
 */
#include "memoriam_praetereo/memoriam_praetereo.h"

#if MMGR_ENABLE_DMA

/**
 * @brief The channel count and buffer size this build was configured with.
 *
 * @note Named only by the assertions in the three checking calls; the default MMGR_ASSERT leaves them unevaluated.
 */
static const PraetInit praet_init = {
    .channels = MMGR_PRAET_CHANNELS,
    .buf_size = MMGR_PRAET_BUF_SIZE,
};

/**
 * @brief Arguments for opening a channel.
 *
 * @note Mirrors PraetCfg without its top-level const qualifiers; on_complete still points at a const PraetCallbackCfg.
 */
typedef struct
{
    uint8_t channel;                     /**< Channel to open. */
    uint8_t periph;                      /**< Peripheral the channel is wired to. */
    mmgr_bool loopback;                  /**< Open the channel looped back on itself. */
    const PraetCallbackCfg *on_complete; /**< Called when a transfer finishes [BORROWS]. */
} PraetOpenCtx;

/**
 * @brief Arguments for submitting a transfer, and for closing a channel.
 *
 * @note Mirrors PraetTransferCfg without its top-level const qualifiers; buf still points at const uint8_t.
 * @note close reads channel alone.
 */
typedef struct
{
    uint8_t channel;    /**< Channel to act on. */
    const uint8_t *buf; /**< Bytes to send [BORROWS]. */
    uint16_t len;       /**< Bytes in buf. */
} PraetTransferCtx;

/**
 * @brief Weak default for opening a channel, which refuses every request.
 *
 * @param[in] c Channel, peripheral and completion callback [BORROWS].
 * @return      MMGR_FALSE always.
 * @note MMGR_WEAK marks this weak where MMGR_HAS_ATTRIBUTE(weak) is non-zero; an application definition replaces it.
 * @note The (void)c discards the argument, since this body reads nothing.
 */
MMGR_WEAK mmgr_bool mmgr_praet_hw_open(const PraetCfg *c)
{
    (void)c;
    return MMGR_FALSE;
}

/**
 * @brief Weak default for submitting a transfer, which refuses every request.
 *
 * @param[in] c Channel, buffer and length [BORROWS].
 * @return      MMGR_FALSE always.
 * @note MMGR_WEAK marks this weak where MMGR_HAS_ATTRIBUTE(weak) is non-zero; an application definition replaces it.
 */
MMGR_WEAK mmgr_bool mmgr_praet_hw_tx_submit(const PraetTransferCfg *c)
{
    (void)c;
    return MMGR_FALSE;
}

/**
 * @brief Weak default for closing a channel, which does nothing.
 *
 * @param[in] c Channel to close [BORROWS].
 * @note MMGR_WEAK marks this weak where MMGR_HAS_ATTRIBUTE(weak) is non-zero; an application definition replaces it.
 */
MMGR_WEAK void mmgr_praet_hw_close(const PraetTransferCfg *c)
{
    (void)c;
}

/**
 * @brief Weak default for the poll hook, which does nothing.
 *
 * @param[in] c Channel to poll [BORROWS].
 * @note MMGR_WEAK marks this weak where MMGR_HAS_ATTRIBUTE(weak) is non-zero; an application definition replaces it.
 */
MMGR_WEAK void mmgr_praet_hw_poll(const PraetCfg *c)
{
    (void)c;
}

/**
 * @brief Checks the channel and the callback, then hands the request to the port layer.
 *
 * @param[in] c Channel, peripheral, loopback flag and completion callback [BORROWS].
 * @return      Whatever mmgr_praet_hw_open returns.
 * @warning c->channel must be below praet_init.channels, and c->on_complete must not be NULL.
 */
MMGR_INLINE mmgr_bool praet_open(const PraetOpenCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");
    MMGR_ASSERT(c->on_complete != NULL, "an open channel reports completion");

    return MMGR_CALL(mmgr_praet_hw_open, PraetCfg, .channel = c->channel, .periph = c->periph,
                     .loopback = c->loopback, .on_complete = c->on_complete);
}

/**
 * @brief Checks the channel and the length, then hands the transfer to the port layer.
 *
 * @param[in] c Channel, buffer and length [BORROWS].
 * @return      Whatever mmgr_praet_hw_tx_submit returns.
 * @warning c->channel must be below praet_init.channels, and c->len must not exceed praet_init.buf_size.
 */
MMGR_INLINE mmgr_bool praet_tx_submit(const PraetTransferCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");
    MMGR_ASSERT(c->len <= praet_init.buf_size, "a transfer is bounded by the channel buffer");

    return MMGR_CALL(mmgr_praet_hw_tx_submit, PraetTransferCfg, .channel = c->channel, .buf = c->buf, .len = c->len);
}

/**
 * @brief Checks the channel, then hands the close to the port layer.
 *
 * @param[in] c Channel to close [BORROWS].
 * @note Passes only the channel on; buf and len take no part.
 * @warning c->channel must be below praet_init.channels.
 */
MMGR_INLINE void praet_close(const PraetTransferCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");

    MMGR_CALL(mmgr_praet_hw_close, PraetTransferCfg, .channel = c->channel);
}

/**
 * @brief Binds this module's fixed arguments to GENERIC_ENTRY, with the two types per entry.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] ctx  Context type this entry's backend takes.
 * @param[in] cfg  Argument type the caller passes.
 * @param[in] name Name after the mmgr_praet_ and praet_ prefixes, which the two share.
 * @note Both types are parameters here. Opening a channel and moving bytes on one take different
 *       arguments, so the module carries two of each rather than one.
 */
#define PRAET_ENTRY(ret, ctx, cfg, name, ...) GENERIC_ENTRY(mmgr_praet_, praet_, ctx, cfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same to GENERIC_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] ctx  Context type this entry's backend takes.
 * @param[in] cfg  Argument type the caller passes.
 * @param[in] name Name after the mmgr_praet_ and praet_ prefixes.
 */
#define PRAET_ENTRY_V(ctx, cfg, name, ...) GENERIC_ENTRY_V(mmgr_praet_, praet_, ctx, cfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in memoriam_praetereo.h.
 * @note close forwards c->channel alone; the rest of its argument type is not read.
 */
PRAET_ENTRY(mmgr_bool, PraetOpenCtx, PraetCfg, open, .channel = c->channel, .periph = c->periph,
            .loopback = c->loopback, .on_complete = c->on_complete)
PRAET_ENTRY(mmgr_bool, PraetTransferCtx, PraetTransferCfg, tx_submit, .channel = c->channel, .buf = c->buf,
            .len = c->len)
PRAET_ENTRY_V(PraetTransferCtx, PraetTransferCfg, close, .channel = c->channel)

/**
 * @brief Calls the port layer's poll hook.
 *
 * @note Hand-rolled rather than an entry line, as mmgr_infin_init is. It hands c to the weak hook
 *       unchanged, with no checking call in between, so there is no argument pack to build and no
 *       praet_ backend for GENERIC_ENTRY to name.
 * @note Documented at the declaration in memoriam_praetereo.h.
 */
void mmgr_praet_poll(const PraetCfg *c)
{
    mmgr_praet_hw_poll(c);
}

#endif
