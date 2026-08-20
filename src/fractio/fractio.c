// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "fractio/fractio.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @file fractio.c
 * @brief IEEE 754 binary64 field access, without <math.h>.
 */

mmgr_u64 mmgr_fract_sign(double v)
{
    return (mmgr_proxim_u64(&v) & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

mmgr_u64 mmgr_fract_exp(double v)
{
    return (mmgr_proxim_u64(&v) & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

mmgr_u64 mmgr_fract_mant(double v)
{
    return mmgr_proxim_u64(&v) & MMGR_DBL_MANT_MASK;
}

mmgr_u64 mmgr_fract_merge(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant)
{
    return ((sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) | ((exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) |
           (mant & MMGR_DBL_MANT_MASK);
}

double mmgr_fract_from_bits(mmgr_u64 bits)
{
    double v = 0.0;
    mmgr_proxim_read(&v, &bits, sizeof(v));
    return v;
}
