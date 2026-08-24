#ifndef MMGR_PROXIMUS_OPEROR_H
#define MMGR_PROXIMUS_OPEROR_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

#define MMGR_RAW MMGR_ALIGN(1) MMGR_ALIAS

#if MMGR_WORD_BITS >= 64
#define MMGR_RAW_WORD 8
#elif MMGR_WORD_BITS >= 32
#define MMGR_RAW_WORD 4
#else
#define MMGR_RAW_WORD 2
#endif

#if MMGR_RAW_WORD >= 8
typedef uint64_t mmgr_migro_word;
#elif MMGR_RAW_WORD >= 4
typedef uint32_t mmgr_migro_word;
#else
typedef uint16_t mmgr_migro_word;
#endif

typedef struct
{
    void *const dst;
    const void *const at;
    const uint64_t val;
    const size_t size;
} ProximusCfg;

typedef struct
{
    uint16_t (*load16)(const ProximusCfg *c);
    uint32_t (*load32)(const ProximusCfg *c);
    uint64_t (*load64)(const ProximusCfg *c);
    void (*put16)(const ProximusCfg *c);
    void (*put32)(const ProximusCfg *c);
    void (*put64)(const ProximusCfg *c);
    mmgr_migro_word (*load)(const ProximusCfg *c);
    void (*put)(const ProximusCfg *c);
    mmgr_migro_word (*al_load)(const ProximusCfg *c);
    void (*al_put)(const ProximusCfg *c);
    uint64_t (*al_load64)(const ProximusCfg *c);
    void (*al_put64)(const ProximusCfg *c);
    void (*read)(const ProximusCfg *c);
} ProximusOperorNs;
MMGR_NS_LAYOUT(ProximusOperorNs, load16, load32, load64, put16, put32, put64, load, put, al_load, al_put, al_load64,
               al_put64, read);

uint16_t mmgr_proxim_load16(const ProximusCfg *c);
uint32_t mmgr_proxim_load32(const ProximusCfg *c);
uint64_t mmgr_proxim_load64(const ProximusCfg *c);
void mmgr_proxim_put16(const ProximusCfg *c);
void mmgr_proxim_put32(const ProximusCfg *c);
void mmgr_proxim_put64(const ProximusCfg *c);
mmgr_migro_word mmgr_proxim_load(const ProximusCfg *c);
void mmgr_proxim_put(const ProximusCfg *c);
mmgr_migro_word mmgr_aequus_load(const ProximusCfg *c);
void mmgr_aequus_put(const ProximusCfg *c);
uint64_t mmgr_aequus_load64(const ProximusCfg *c);
void mmgr_aequus_put64(const ProximusCfg *c);
void mmgr_proxim_read(const ProximusCfg *c);

MMGR_NS ProximusOperorNs proxim MMGR_UNUSED = {
    .load16 = mmgr_proxim_load16,
    .load32 = mmgr_proxim_load32,
    .load64 = mmgr_proxim_load64,
    .put16 = mmgr_proxim_put16,
    .put32 = mmgr_proxim_put32,
    .put64 = mmgr_proxim_put64,
    .load = mmgr_proxim_load,
    .put = mmgr_proxim_put,
    .al_load = mmgr_aequus_load,
    .al_put = mmgr_aequus_put,
    .al_load64 = mmgr_aequus_load64,
    .al_put64 = mmgr_aequus_put64,
    .read = mmgr_proxim_read,
};

MMGR_FINIS_DECLS

#endif
