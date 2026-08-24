#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    void *const dst;
    const void *const src;
    const void *const other;
    const size_t n;
    const uint8_t v;
} MemoriaCfg;

typedef struct
{
    void (*cpy)(const MemoriaCfg *c);
    void (*move_down)(const MemoriaCfg *c);
    void (*move_up)(const MemoriaCfg *c);
    mmgr_iword (*cmp)(const MemoriaCfg *c);
    const void *(*chr)(const MemoriaCfg *c);
    void (*set)(const MemoriaCfg *c);
} MemoriaOperorNs;
MMGR_NS_LAYOUT(MemoriaOperorNs, cpy, move_down, move_up, cmp, chr, set);

void mmgr_memor_cpy(const MemoriaCfg *c);
void mmgr_memor_move_up(const MemoriaCfg *c);
mmgr_iword mmgr_memor_cmp(const MemoriaCfg *c);
const void *mmgr_memor_chr(const MemoriaCfg *c);
void mmgr_memor_set(const MemoriaCfg *c);

MMGR_NS MemoriaOperorNs memor MMGR_UNUSED = {
    .cpy = mmgr_memor_cpy,
    .move_down = mmgr_memor_cpy,
    .move_up = mmgr_memor_move_up,
    .cmp = mmgr_memor_cmp,
    .chr = mmgr_memor_chr,
    .set = mmgr_memor_set,
};

MMGR_FINIS_DECLS

#endif
