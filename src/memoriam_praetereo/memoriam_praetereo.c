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
 * @brief Opens a channel through the port layer.
 *
 * @note Documented at the declaration in memoriam_praetereo.h.
 */
mmgr_bool mmgr_praet_open(const PraetCfg *c)
{
    return MMGR_CALL(praet_open, PraetOpenCtx, .channel = c->channel, .periph = c->periph, .loopback = c->loopback,
                     .on_complete = c->on_complete);
}

/**
 * @brief Submits a transfer through the port layer.
 *
 * @note Documented at the declaration in memoriam_praetereo.h.
 */
mmgr_bool mmgr_praet_tx_submit(const PraetTransferCfg *c)
{
    return MMGR_CALL(praet_tx_submit, PraetTransferCtx, .channel = c->channel, .buf = c->buf, .len = c->len);
}

/**
 * @brief Closes a channel through the port layer.
 *
 * @note Only c->channel is forwarded.
 * @note Documented at the declaration in memoriam_praetereo.h.
 */
void mmgr_praet_close(const PraetTransferCfg *c)
{
    MMGR_CALL(praet_close, PraetTransferCtx, .channel = c->channel);
}

/**
 * @brief Calls the port layer's poll hook.
 *
 * @note Passes c straight through, with no checking call in between, unlike the other three entries.
 * @note Documented at the declaration in memoriam_praetereo.h.
 */
void mmgr_praet_poll(const PraetCfg *c)
{
    mmgr_praet_hw_poll(c);
}

#endif
