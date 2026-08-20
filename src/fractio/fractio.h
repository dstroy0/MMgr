// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_FRACTIO_H
#define MMGR_FRACTIO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file fractio.h
 * @brief Take an IEEE 754 double apart and put it back together, without <math.h>.
 */

/** @brief IEEE 754 binary64 field masks, shifts and bias. */
#define MMGR_DBL_SIGN_MASK 0x8000000000000000ull
#define MMGR_DBL_EXP_MASK 0x7FF0000000000000ull
#define MMGR_DBL_MANT_MASK 0x000FFFFFFFFFFFFFull
#define MMGR_DBL_SIGN_SHIFT 63u
#define MMGR_DBL_MANT_BITS 52u
#define MMGR_DBL_SIGN_ONE 0x1ull
#define MMGR_DBL_EXP_ALL 0x7FFull
#define MMGR_DBL_BIAS 1023

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_u64 (*sign)(double v);
    mmgr_u64 (*exp)(double v);
    mmgr_u64 (*mant)(double v);
    mmgr_u64 (*merge)(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant);
    double (*from_bits)(mmgr_u64 bits);
} FractioNs;
MMGR_NS_LAYOUT(FractioNs, sign, exp, mant, merge, from_bits);

/**
 * @brief Sign bit.
 * @param v Value.
 * @return 0 or 1.
 */
mmgr_u64 mmgr_fract_sign(double v);
/**
 * @brief Raw exponent field, unbiased.
 * @param v Value.
 * @return Exponent bits.
 */
mmgr_u64 mmgr_fract_exp(double v);
/**
 * @brief Mantissa field, without the implicit leading bit.
 * @param v Value.
 * @return Mantissa bits.
 */
mmgr_u64 mmgr_fract_mant(double v);
/**
 * @brief Assemble the three fields into a bit pattern.
 * @param sign Sign bit.
 * @param exp Raw exponent.
 * @param mant Mantissa.
 * @return The bit pattern.
 */
mmgr_u64 mmgr_fract_merge(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant);
/**
 * @brief Reinterpret a bit pattern as a double.
 * @param bits Bit pattern.
 * @return The value.
 */
double mmgr_fract_from_bits(mmgr_u64 bits);

/** @brief Module namespace. */
MMGR_NS FractioNs fract MMGR_UNUSED = {
    .sign = mmgr_fract_sign,
    .exp = mmgr_fract_exp,
    .mant = mmgr_fract_mant,
    .merge = mmgr_fract_merge,
    .from_bits = mmgr_fract_from_bits,
};

MMGR_END_DECLS

#endif
