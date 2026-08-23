#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "fractio/fractio.h"
#include "config/mmgr_config.h"
#include "pow5/pow5.h"

MMGR_INCIPE_DECLS


#define MMGR_MUTO_MANT_MAX ((mmgr_u64)((~(mmgr_u64)0 - 9u) / 10u))

#define MMGR_MUTO_EXP_LIMIT 400

typedef struct
{
    mmgr_u64 *const mant;     const char digit;         const int e2;             const int ex;             const int rest;           const unsigned above;     const mmgr_bool neg;  } TransformoCfg;

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

#define MMGR_MUTO_IS_MANT(x_) ((void)_Generic((x_), mmgr_u64: 0))
#define MMGR_MUTO_IS_DIGIT(x_) ((void)_Generic((x_), char: 0))
#define MMGR_MUTO_IS_EXP(x_) ((void)_Generic((x_), int: 0))
#define MMGR_MUTO_IS_ABOVE(x_) ((void)_Generic((x_), unsigned: 0))
#define MMGR_MUTO_IS_NEG(x_) ((void)_Generic((x_), mmgr_bool: 0, int: 0))


#define mmgr_muto_take(mant_, digit_)                                                                                  \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_DIGIT(digit_),                                                             \
     muto.take(&(TransformoCfg){.mant = &(mant_), .digit = (digit_)}))

#define mmgr_muto_scale(mant_, ex_, rest_, neg_)                                                                       \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_EXP(ex_), MMGR_MUTO_IS_EXP(rest_), MMGR_MUTO_IS_NEG(neg_),                 \
     muto.scale(&(TransformoCfg){.mant = &(mant_), .ex = (ex_), .rest = (rest_), .neg = (neg_)}))

#define mmgr_muto_scale_to_u64(mant_, e2_, ex_, above_)                                                                \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_EXP(e2_), MMGR_MUTO_IS_EXP(ex_), MMGR_MUTO_IS_ABOVE(above_),               \
     muto.scale_to_u64(&(TransformoCfg){.mant = &(mant_), .e2 = (e2_), .ex = (ex_), .above = (above_)}))

MMGR_NS TransformoNs muto MMGR_UNUSED = {
    .take = mmgr_muto_take,
    .scale = mmgr_muto_scale,
    .scale_to_u64 = mmgr_muto_scale_to_u64,
};

MMGR_FINIS_DECLS

#endif
