#ifndef MMGR_CLZ_H
#define MMGR_CLZ_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    const mmgr_u64 x; } ClzCfg;

typedef struct
{
    int (*lead)(const ClzCfg *c);
} ClzNs;
MMGR_NS_LAYOUT(ClzNs, lead);

int mmgr_clz_lead(const ClzCfg *c);

#define MMGR_CLZ_IS_VALUE(x_) ((void)_Generic((x_), mmgr_u64: 0))

#define mmgr_clz_lead(x_) (MMGR_CLZ_IS_VALUE(x_), clz.lead(&(ClzCfg){.x = (x_)}))

MMGR_NS ClzNs clz MMGR_UNUSED = {
    .lead = mmgr_clz_lead,
};

MMGR_FINIS_DECLS

#endif
