#include "memoriam_praetereo/memoriam_praetereo.h"


#if MMGR_ENABLE_DMA

typedef struct
{
    uint8_t ch;                     const uint8_t *buf;             uint16_t len;               } MemoriamPraetereoCtx;

MMGR_WEAK mmgr_bool mmgr_praet_hw_open(const MemoriamPraetereoCfg *cfg)
{
    (void)cfg;
    return MMGR_FALSE;
}
MMGR_WEAK mmgr_bool mmgr_praet_hw_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    (void)ch;
    (void)buf;
    (void)len;
    return MMGR_FALSE;
}
MMGR_WEAK void mmgr_praet_hw_close(uint8_t ch)
{
    (void)ch;
}
MMGR_WEAK void mmgr_praet_hw_poll(void)
{
}

MMGR_INLINE mmgr_bool praet_open(const MemoriamPraetereoCfg *cfg)
{
    if ((cfg == NULL) || (cfg->on_complete == NULL) || (cfg->channel >= MMGR_PRAET_CHANNELS))
    {
        return MMGR_FALSE;
    }
    return mmgr_praet_hw_open(cfg);
}

MMGR_INLINE mmgr_bool praet_tx_submit(const MemoriamPraetereoCtx *c)
{
    if ((c->ch >= MMGR_PRAET_CHANNELS) || (c->buf == NULL) || (c->len == 0u) || (c->len > MMGR_PRAET_BUF_SIZE))
    {
        return MMGR_FALSE;
    }
    return mmgr_praet_hw_tx_submit(c->ch, c->buf, c->len);
}

MMGR_INLINE void praet_close(uint8_t ch)
{
    if (ch < MMGR_PRAET_CHANNELS)
    {
        mmgr_praet_hw_close(ch);
    }
}


mmgr_bool mmgr_praet_open(const MemoriamPraetereoCfg *cfg)
{
    return praet_open(cfg);
}

mmgr_bool mmgr_praet_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    return MMGR_CALL(praet_tx_submit, MemoriamPraetereoCtx, .ch = ch, .buf = buf, .len = len);
}

void mmgr_praet_close(uint8_t ch)
{
    praet_close(ch);
}

void mmgr_praet_poll(void)
{
    mmgr_praet_hw_poll();
}

#endif
