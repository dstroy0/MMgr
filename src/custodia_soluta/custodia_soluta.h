#ifndef MMGR_CUSTODIA_SOLUTA_H
#define MMGR_CUSTODIA_SOLUTA_H

#include "carceribus/carceribus.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    CarcerCtx *const pool;
    void *const at;
    const size_t n;
} SolutaCfg;

typedef struct
{
    void *(*init)(const SolutaCfg *c);
    void (*release)(const SolutaCfg *c);
    size_t (*used)(const SolutaCfg *c);
} CustodiaSolutaNs;
MMGR_NS_LAYOUT(CustodiaSolutaNs, init, release, used);

void *mmgr_soluta_init(const SolutaCfg *c);
void mmgr_soluta_reddo(const SolutaCfg *c);
size_t mmgr_soluta_used(const SolutaCfg *c);

MMGR_NS CustodiaSolutaNs soluta MMGR_UNUSED = {
    .init = mmgr_soluta_init,
    .release = mmgr_soluta_reddo,
    .used = mmgr_soluta_used,
};

MMGR_FINIS_DECLS

#endif
