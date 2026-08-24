#ifndef MMGR_CLZ_H
#define MMGR_CLZ_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    const mmgr_u64 val;
} ClzCfg;

typedef struct
{
    mmgr_iword (*lead)(const ClzCfg *c);
} ClzNs;
MMGR_NS_LAYOUT(ClzNs, lead);

mmgr_iword mmgr_clz_lead(const ClzCfg *c);

MMGR_NS ClzNs clz MMGR_UNUSED = {
    .lead = mmgr_clz_lead,
};

MMGR_FINIS_DECLS

#endif
