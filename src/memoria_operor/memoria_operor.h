#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    void *const dst;            const void *const src;      const void *const other;    const size_t n;             const uint8_t v;          } MemoriaCfg;

typedef struct
{
    void (*cpy)(const MemoriaCfg *c);
    void (*move_down)(const MemoriaCfg *c);
    void (*move_up)(const MemoriaCfg *c);
    int (*cmp)(const MemoriaCfg *c);
    const void *(*chr)(const MemoriaCfg *c);
    void (*set)(const MemoriaCfg *c);
} MemoriaOperorNs;
MMGR_NS_LAYOUT(MemoriaOperorNs, cpy, move_down, move_up, cmp, chr, set);

void mmgr_memor_cpy(const MemoriaCfg *c);
void mmgr_memor_move_up(const MemoriaCfg *c);
int mmgr_memor_cmp(const MemoriaCfg *c);
const void *mmgr_memor_chr(const MemoriaCfg *c);
void mmgr_memor_set(const MemoriaCfg *c);

#define MMGR_MEMOR_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))
#define MMGR_MEMOR_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

#define mmgr_memor_cpy(dst_, src_, n_)                                                                                 \
    (MMGR_MEMOR_IS_SIZE(n_), memor.cpy(&(MemoriaCfg){.dst = (dst_), .src = (src_), .n = (n_)}))

#define mmgr_memor_move_down(dst_, src_, n_)                                                                           \
    (MMGR_MEMOR_IS_SIZE(n_), memor.move_down(&(MemoriaCfg){.dst = (dst_), .src = (src_), .n = (n_)}))

#define mmgr_memor_move_up(dst_, src_, n_)                                                                             \
    (MMGR_MEMOR_IS_SIZE(n_), memor.move_up(&(MemoriaCfg){.dst = (dst_), .src = (src_), .n = (n_)}))

#define mmgr_memor_cmp(a_, b_, n_)                                                                                     \
    (MMGR_MEMOR_IS_SIZE(n_), memor.cmp(&(MemoriaCfg){.src = (a_), .other = (b_), .n = (n_)}))

#define mmgr_memor_chr(src_, n_, v_)                                                                                   \
    (MMGR_MEMOR_IS_SIZE(n_), MMGR_MEMOR_IS_BYTE(v_),                                                                   \
     memor.chr(&(MemoriaCfg){.src = (src_), .n = (n_), .v = (v_)}))

#define mmgr_memor_set(dst_, v_, n_)                                                                                   \
    (MMGR_MEMOR_IS_BYTE(v_), MMGR_MEMOR_IS_SIZE(n_),                                                                   \
     memor.set(&(MemoriaCfg){.dst = (dst_), .v = (v_), .n = (n_)}))

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
