#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    uint8_t *buf;     size_t cap;       size_t pos;   } mmgr_spat;

typedef struct
{
    uint8_t *const buf;     const size_t cap;   } SpatCfg;

typedef struct
{
    mmgr_spat (*init)(const SpatCfg *c);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, init);

mmgr_spat mmgr_spat_init(const SpatCfg *c);

#define MMGR_SPAT_IS_BUF(x_) ((void)_Generic((x_), uint8_t *: 0))
#define MMGR_SPAT_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))

#define mmgr_spat_init(buf_, cap_)                                                                                     \
    (MMGR_SPAT_IS_BUF(buf_), MMGR_SPAT_IS_SIZE(cap_), spat.init(&(SpatCfg){.buf = (buf_), .cap = (cap_)}))

MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .init = mmgr_spat_init,
};

MMGR_FINIS_DECLS

#endif
