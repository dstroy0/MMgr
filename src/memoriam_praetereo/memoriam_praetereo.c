#include "memoriam_praetereo/memoriam_praetereo.h"

#if MMGR_ENABLE_DMA

static const PraetInit praet_init = {
    .channels = MMGR_PRAET_CHANNELS,
    .buf_size = MMGR_PRAET_BUF_SIZE,
};

typedef struct
{
    uint8_t channel;
    uint8_t periph;
    mmgr_bool loopback;
    const PraetCallbackCfg *on_complete;
} PraetOpenCtx;

typedef struct
{
    uint8_t channel;
    const uint8_t *buf;
    uint16_t len;
} PraetTransferCtx;

MMGR_WEAK mmgr_bool mmgr_praet_hw_open(const PraetCfg *c)
{
    (void)c;
    return MMGR_FALSE;
}

MMGR_WEAK mmgr_bool mmgr_praet_hw_tx_submit(const PraetTransferCfg *c)
{
    (void)c;
    return MMGR_FALSE;
}

MMGR_WEAK void mmgr_praet_hw_close(const PraetTransferCfg *c)
{
    (void)c;
}

MMGR_WEAK void mmgr_praet_hw_poll(const PraetCfg *c)
{
    (void)c;
}

MMGR_INLINE mmgr_bool praet_open(const PraetOpenCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");
    MMGR_ASSERT(c->on_complete != NULL, "an open channel reports completion");

    return MMGR_CALL(mmgr_praet_hw_open, PraetCfg, .channel = c->channel, .periph = c->periph,
                     .loopback = c->loopback, .on_complete = c->on_complete);
}

MMGR_INLINE mmgr_bool praet_tx_submit(const PraetTransferCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");
    MMGR_ASSERT(c->len <= praet_init.buf_size, "a transfer is bounded by the channel buffer");

    return MMGR_CALL(mmgr_praet_hw_tx_submit, PraetTransferCfg, .channel = c->channel, .buf = c->buf, .len = c->len);
}

MMGR_INLINE void praet_close(const PraetTransferCtx *c)
{
    MMGR_ASSERT(c->channel < praet_init.channels, "no such channel");

    MMGR_CALL(mmgr_praet_hw_close, PraetTransferCfg, .channel = c->channel);
}

mmgr_bool mmgr_praet_open(const PraetCfg *c)
{
    return MMGR_CALL(praet_open, PraetOpenCtx, .channel = c->channel, .periph = c->periph, .loopback = c->loopback,
                     .on_complete = c->on_complete);
}

mmgr_bool mmgr_praet_tx_submit(const PraetTransferCfg *c)
{
    return MMGR_CALL(praet_tx_submit, PraetTransferCtx, .channel = c->channel, .buf = c->buf, .len = c->len);
}

void mmgr_praet_close(const PraetTransferCfg *c)
{
    MMGR_CALL(praet_close, PraetTransferCtx, .channel = c->channel);
}

void mmgr_praet_poll(const PraetCfg *c)
{
    mmgr_praet_hw_poll(c);
}

#endif
