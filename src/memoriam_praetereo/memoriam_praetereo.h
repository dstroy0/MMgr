#ifndef MMGR_MEMORIAM_PRAETEREO_H
#define MMGR_MEMORIAM_PRAETEREO_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_INCIPE_DECLS

typedef struct
{
    const uint8_t *data;
    uint32_t t_ms;
    uint32_t t_us;
    uint16_t len;
    uint16_t seq;
    uint8_t channel;
    uint8_t periph;
    uint8_t dir;
} mmgr_praet_event;

typedef void (*mmgr_praet_cb)(const mmgr_praet_event *ev, void *user);

typedef struct
{
    const size_t channels;
    const size_t buf_size;
} PraetInit;

typedef struct
{
    const mmgr_praet_cb fn;
    void *const user;
} PraetCallbackCfg;

typedef struct
{
    const uint8_t channel;
    const uint8_t periph;
    const mmgr_bool loopback;
    const PraetCallbackCfg *const on_complete;
} PraetCfg;

typedef struct
{
    const uint8_t channel;
    const uint8_t *const buf;
    const uint16_t len;
} PraetTransferCfg;

typedef struct
{
    mmgr_bool (*open)(const PraetCfg *c);
    mmgr_bool (*tx_submit)(const PraetTransferCfg *c);
    void (*close)(const PraetTransferCfg *c);
    void (*poll)(const PraetCfg *c);
} MemoriamPraetereoNs;
MMGR_NS_LAYOUT(MemoriamPraetereoNs, open, tx_submit, close, poll);

mmgr_bool mmgr_praet_open(const PraetCfg *c);
mmgr_bool mmgr_praet_tx_submit(const PraetTransferCfg *c);
void mmgr_praet_close(const PraetTransferCfg *c);
void mmgr_praet_poll(const PraetCfg *c);

mmgr_bool mmgr_praet_hw_open(const PraetCfg *c);
mmgr_bool mmgr_praet_hw_tx_submit(const PraetTransferCfg *c);
void mmgr_praet_hw_close(const PraetTransferCfg *c);
void mmgr_praet_hw_poll(const PraetCfg *c);

MMGR_NS MemoriamPraetereoNs praet MMGR_UNUSED = {
    .open = mmgr_praet_open,
    .tx_submit = mmgr_praet_tx_submit,
    .close = mmgr_praet_close,
    .poll = mmgr_praet_poll,
};

MMGR_FINIS_DECLS

#endif

#endif
