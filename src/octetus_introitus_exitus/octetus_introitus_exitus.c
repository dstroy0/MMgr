#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "proximus_operor/proximus_operor.h"


MMGR_INLINE uint64_t octet_rev(uint64_t v)
{
    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    return (v << 32) | (v >> 32);
}

MMGR_INLINE void octet_put(const OctetusCfg *c)
{
    proxim.al_put_u64(c->at, octet_rev(c->val << (8u * (8u - c->n))));
}

MMGR_INLINE void octet_take(const OctetusCfg *c)
{
    *c->out = octet_rev(proxim.al_load(c->from, 8u)) >> (8u * (8u - c->n));
}


void (mmgr_octet_put)(const OctetusCfg *c)
{
    octet_put(c);
}

void (mmgr_octet_take)(const OctetusCfg *c)
{
    octet_take(c);
}
