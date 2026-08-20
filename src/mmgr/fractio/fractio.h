// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_FRACTIO_H
#define MMGR_FRACTIO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_DBL_SIGN_MASK 0x8000000000000000ull
#define MMGR_DBL_EXP_MASK 0x7FF0000000000000ull
#define MMGR_DBL_MANT_MASK 0x000FFFFFFFFFFFFFull
#define MMGR_DBL_SIGN_SHIFT 63u
#define MMGR_DBL_MANT_BITS 52u
#define MMGR_DBL_SIGN_ONE 0x1ull
#define MMGR_DBL_EXP_ALL 0x7FFull
#define MMGR_DBL_BIAS 1023

typedef struct
{
    mmgr_u64 (*sign)(double v);
    mmgr_u64 (*exp)(double v);
    mmgr_u64 (*mant)(double v);
    mmgr_u64 (*merge)(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant);
    double (*from_bits)(mmgr_u64 bits);
} FractioNs;

mmgr_u64 mmgr_fract_sign(double v);
mmgr_u64 mmgr_fract_exp(double v);
mmgr_u64 mmgr_fract_mant(double v);
mmgr_u64 mmgr_fract_merge(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant);
double mmgr_fract_from_bits(mmgr_u64 bits);

static const FractioNs fract __attribute__((unused)) = {mmgr_fract_sign, mmgr_fract_exp, mmgr_fract_mant,
                                                        mmgr_fract_merge, mmgr_fract_from_bits};

MMGR_END_DECLS

#endif
