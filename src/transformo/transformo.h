#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "config/mmgr_config.h"
#include "fractio/fractio.h"
#include "pow5/pow5.h"

MMGR_INCIPE_DECLS

#define MMGR_MUTO_MANT_MAX ((mmgr_u64)((~(mmgr_u64)0 - 9u) / 10u))

#define MMGR_MUTO_EXP_LIMIT 400

typedef struct
{
    mmgr_u64 *const mant;
    const char digit;
    const mmgr_iword e2;
    const mmgr_iword ex;
    const mmgr_iword rest;
    const mmgr_word above;
    const mmgr_bool neg;
} TransformoCfg;

typedef struct
{
    mmgr_bool (*take)(const TransformoCfg *c);
    double (*scale)(const TransformoCfg *c);
    mmgr_u64 (*scale_to_u64)(const TransformoCfg *c);
} TransformoNs;
MMGR_NS_LAYOUT(TransformoNs, take, scale, scale_to_u64);

mmgr_bool mmgr_muto_take(const TransformoCfg *c);
double mmgr_muto_scale(const TransformoCfg *c);
mmgr_u64 mmgr_muto_scale_to_u64(const TransformoCfg *c);

MMGR_NS TransformoNs muto MMGR_UNUSED = {
    .take = mmgr_muto_take,
    .scale = mmgr_muto_scale,
    .scale_to_u64 = mmgr_muto_scale_to_u64,
};

MMGR_FINIS_DECLS

#endif
