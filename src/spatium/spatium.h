#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
} mmgr_spat;

typedef struct
{
    uint8_t *const buf;
    const size_t cap;
} SpatCfg;

typedef struct
{
    mmgr_spat (*init)(const SpatCfg *c);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, init);

mmgr_spat mmgr_spat_init(const SpatCfg *c);

MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .init = mmgr_spat_init,
};

MMGR_FINIS_DECLS

#endif
