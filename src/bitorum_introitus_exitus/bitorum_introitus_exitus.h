#ifndef MMGR_BITORUM_INTROITUS_EXITUS_H
#define MMGR_BITORUM_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    uint8_t *out;
    size_t cap;
    size_t cnt;
    uint32_t acc;
    mmgr_iword nbits;
    mmgr_bool overflow;
} BitorumCfg;

typedef struct
{
    void (*put)(BitorumCfg *c);
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, put);

void mmgr_bitor_put(BitorumCfg *c);

MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .put = mmgr_bitor_put,
};

MMGR_FINIS_DECLS

#endif
