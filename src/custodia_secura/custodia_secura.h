#ifndef MMGR_CUSTODIA_SECURA_H
#define MMGR_CUSTODIA_SECURA_H

#include "carceribus/carceribus.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    CarcerCtx *const pool;
    void *const at;
    const size_t bytes;
} SecuraCfg;

typedef struct
{
    void *(*init)(const SecuraCfg *c);
    void (*release)(const SecuraCfg *c);
    size_t (*used)(const SecuraCfg *c);
    void (*wipe)(const SecuraCfg *c);
} CustodiaSecuraNs;
MMGR_NS_LAYOUT(CustodiaSecuraNs, init, release, used, wipe);

void *mmgr_secura_init(const SecuraCfg *c);
void mmgr_secura_reddo(const SecuraCfg *c);
size_t mmgr_secura_used(const SecuraCfg *c);
void mmgr_secura_wipe(const SecuraCfg *c);

MMGR_NS CustodiaSecuraNs secura MMGR_UNUSED = {
    .init = mmgr_secura_init,
    .release = mmgr_secura_reddo,
    .used = mmgr_secura_used,
    .wipe = mmgr_secura_wipe,
};

MMGR_FINIS_DECLS

#endif
