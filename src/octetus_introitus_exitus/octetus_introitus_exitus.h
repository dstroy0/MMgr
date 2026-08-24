#ifndef MMGR_OCTETUS_INTROITUS_EXITUS_H
#define MMGR_OCTETUS_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

typedef struct
{
    uint8_t *const at;
    const uint8_t *const from;
    uint64_t *const out;
    const uint64_t val;
    const size_t n;
} OctetusCfg;

typedef struct
{
    void (*put)(const OctetusCfg *c);
    void (*take)(const OctetusCfg *c);
} OctetusIntroitusExitusNs;
MMGR_NS_LAYOUT(OctetusIntroitusExitusNs, put, take);

void mmgr_octet_put(const OctetusCfg *c);
void mmgr_octet_take(const OctetusCfg *c);

MMGR_NS OctetusIntroitusExitusNs byteio MMGR_UNUSED = {
    .put = mmgr_octet_put,
    .take = mmgr_octet_take,
};

MMGR_FINIS_DECLS

#endif
