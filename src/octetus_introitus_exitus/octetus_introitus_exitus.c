#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

typedef struct
{
    uint8_t *at;
    uint64_t val;
    size_t n;
} OctetPutCtx;

typedef struct
{
    const uint8_t *from;
    uint64_t *out;
    size_t n;
} OctetTakeCtx;

MMGR_INLINE void octet_put(const OctetPutCtx *c)
{
    const uint64_t v = c->val << (8u * (8u - c->n));

    MMGR_CALL(proxim.al_put64, ProximusCfg, .dst = c->at,
              .val = MMGR_CALL(magna_extremitas.rev, EndianCfg, .v = v, .n = MMGR_ENDIAN_64));
}

MMGR_INLINE void octet_take(const OctetTakeCtx *c)
{
    const uint64_t v = MMGR_CALL(proxim.al_load64, ProximusCfg, .at = c->from);

    *c->out = MMGR_CALL(magna_extremitas.rev, EndianCfg, .v = v, .n = (mmgr_endian_width)c->n);
}

void mmgr_octet_put(const OctetusCfg *c)
{
    MMGR_CALL(octet_put, OctetPutCtx, .at = c->at, .val = c->val, .n = c->n);
}

void mmgr_octet_take(const OctetusCfg *c)
{
    MMGR_CALL(octet_take, OctetTakeCtx, .from = c->from, .out = c->out, .n = c->n);
}
