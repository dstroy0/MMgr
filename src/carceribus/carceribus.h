#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    uint8_t *const at;
    const size_t size;
} CarcerInit;

typedef struct
{
    uint8_t *const base;
    const size_t size;
    size_t persist_end;
    size_t interim_top;
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    size_t hw;
#endif
} CarcerCtx;

typedef struct
{
    CarcerCtx *const pool;
    const size_t size;
    const void *const at;
} CarcerCfg;

typedef struct
{
    void *(*persist_capio)(const CarcerCfg *c);
    void (*persist_reddo)(const CarcerCfg *c);
    void *(*interim_capio)(const CarcerCfg *c);
    size_t (*interim_mark)(const CarcerCfg *c);
    void (*interim_reddo)(const CarcerCfg *c);
    void (*interim_reset)(const CarcerCfg *c);
    mmgr_bool (*owns)(const CarcerCfg *c);
    size_t (*octas_praesto)(const CarcerCfg *c);
    size_t (*persist_used)(const CarcerCfg *c);
} CarceribusNs;
MMGR_NS_LAYOUT(CarceribusNs, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto, persist_used);

void *mmgr_carcer_persist_capio(const CarcerCfg *c);
void mmgr_carcer_persist_reddo(const CarcerCfg *c);
void *mmgr_carcer_interim_capio(const CarcerCfg *c);
size_t mmgr_carcer_interim_mark(const CarcerCfg *c);
void mmgr_carcer_interim_reddo(const CarcerCfg *c);
void mmgr_carcer_interim_reset(const CarcerCfg *c);
mmgr_bool mmgr_carcer_owns(const CarcerCfg *c);
size_t mmgr_carcer_octas_praesto(const CarcerCfg *c);
size_t mmgr_carcer_persist_used(const CarcerCfg *c);

MMGR_NS CarceribusNs carcer MMGR_UNUSED = {
    .persist_capio = mmgr_carcer_persist_capio,
    .persist_reddo = mmgr_carcer_persist_reddo,
    .interim_capio = mmgr_carcer_interim_capio,
    .interim_mark = mmgr_carcer_interim_mark,
    .interim_reddo = mmgr_carcer_interim_reddo,
    .interim_reset = mmgr_carcer_interim_reset,
    .owns = mmgr_carcer_owns,
    .octas_praesto = mmgr_carcer_octas_praesto,
    .persist_used = mmgr_carcer_persist_used,
};

MMGR_FINIS_DECLS

#endif
