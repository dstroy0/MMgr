#ifndef MMGR_IMPENSA_ANCORAE_ACUS_H
#define MMGR_IMPENSA_ANCORAE_ACUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef struct
{
    const uint8_t b; } AncoraeCfg;

typedef struct
{
    uint8_t (*impensa)(const AncoraeCfg *c);
} ImpensaAncoraeAcusNs;
MMGR_NS_LAYOUT(ImpensaAncoraeAcusNs, impensa);

uint8_t mmgr_ancorae_impensa(const AncoraeCfg *c);

#define MMGR_ANCORAE_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

#define mmgr_ancorae_impensa(b_) (MMGR_ANCORAE_IS_BYTE(b_), ancorae.impensa(&(AncoraeCfg){.b = (b_)}))

MMGR_NS ImpensaAncoraeAcusNs ancorae MMGR_UNUSED = {
    .impensa = mmgr_ancorae_impensa,
};

MMGR_FINIS_DECLS

#endif
