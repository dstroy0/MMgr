#ifndef MMGR_MEMORIAM_PRAETEREO_H
#define MMGR_MEMORIAM_PRAETEREO_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_DMA

MMGR_INCIPE_DECLS


typedef enum MMGR_ENUM_PACKED
{
    MMGR_PRAET_UART = 0,
    MMGR_PRAET_I2C = 1,
    MMGR_PRAET_SPI = 2,
} mmgr_praet_periph;

typedef enum MMGR_ENUM_PACKED
{
    MMGR_PRAET_RX = 0,
    MMGR_PRAET_TX = 1,
} mmgr_praet_dir;

typedef struct
{
    const uint8_t *data;
    uint32_t t_ms;
    uint32_t t_us;

    uint16_t len;
    uint16_t seq;
    uint8_t channel;
    mmgr_praet_periph periph;
    mmgr_praet_dir dir;
    uint8_t _pad;
} mmgr_praet_event;

typedef void (*mmgr_praet_cb)(const mmgr_praet_event *ev, void *ctx);

typedef struct
{
    const uint8_t channel;
    const mmgr_praet_periph periph;
    const mmgr_bool loopback;
    const mmgr_praet_cb on_complete;
    void *const ctx;
} MemoriamPraetereoCfg;

typedef struct
{
    mmgr_bool (*open)(const MemoriamPraetereoCfg *cfg);
    mmgr_bool (*tx_submit)(uint8_t ch, const uint8_t *buf, uint16_t len);
    void (*close)(uint8_t ch);
    void (*poll)(void);
} MemoriamPraetereoNs;
MMGR_NS_LAYOUT(MemoriamPraetereoNs, open, tx_submit, close, poll);

mmgr_bool mmgr_praet_open(const MemoriamPraetereoCfg *cfg);
mmgr_bool mmgr_praet_tx_submit(uint8_t ch, const uint8_t *buf, uint16_t len);
void mmgr_praet_close(uint8_t ch);
void mmgr_praet_poll(void);

MMGR_NS MemoriamPraetereoNs praet MMGR_UNUSED = {
    .open = mmgr_praet_open,
    .tx_submit = mmgr_praet_tx_submit,
    .close = mmgr_praet_close,
    .poll = mmgr_praet_poll,
};

MMGR_FINIS_DECLS

#endif

#endif
