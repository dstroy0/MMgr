#ifndef MMGR_BITORUM_INTROITUS_EXITUS_H
#define MMGR_BITORUM_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    uint8_t *out;            size_t cap;              size_t cnt;              uint32_t acc;            int nbits;               mmgr_bool overflow;  } BitorumCfg;

typedef struct
{
    void (*put)(BitorumCfg *c);
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, put);

void mmgr_bitor_put(BitorumCfg *c);

#define MMGR_BITOR_IS_WRITER(x_) ((void)_Generic((x_), BitorumCfg: 0))
#define MMGR_BITOR_IS_BITS(x_) ((void)_Generic((x_), uint32_t: 0, int: 0))
#define MMGR_BITOR_IS_COUNT(x_) ((void)_Generic((x_), int: 0))

#define mmgr_bitor_put(c_, bits_, n_)                                                                                  \
    (MMGR_BITOR_IS_WRITER(c_), MMGR_BITOR_IS_BITS(bits_), MMGR_BITOR_IS_COUNT(n_),                                     \
     (c_).acc |= (uint32_t)(((n_) >= 32) ? (bits_) : ((bits_) & ((1u << (n_)) - 1u))) << (c_).nbits,                  \
     (c_).nbits += (n_), bitio.put(&(c_)))

MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .put = mmgr_bitor_put,
};

MMGR_FINIS_DECLS

#endif
