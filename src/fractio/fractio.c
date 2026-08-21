// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "fractio/fractio.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @file fractio.c
 * @brief IEEE 754 binary64 field access, without <math.h>.
 *
 * No context here. A context groups a parameter list, and every entry in this module takes one
 * value and returns one value - there is nothing to group. Wrapping the double in a struct so it
 * can be read back out one line later is a store and a load to reach a register, and the unnamed
 * fields of a designated initializer are zeroed first, so it is a clear of the struct as well.
 */

/**
 * @brief The value's bits.
 * @param v The value.
 * @return The bit pattern.
 *
 * Through a byte read rather than a cast, because a cast between a double and an integer of the
 * same width is not a reinterpretation in C and a union is a second way to say the same thing.
 */
MMGR_INLINE mmgr_u64 fract_bits(double v)
{
    return mmgr_proxim_u64(&v);
}

mmgr_u64 mmgr_fract_sign(double v)
{
    return (fract_bits(v) & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

mmgr_u64 mmgr_fract_exp(double v)
{
    return (fract_bits(v) & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

mmgr_u64 mmgr_fract_mant(double v)
{
    return fract_bits(v) & MMGR_DBL_MANT_MASK;
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

mmgr_u64 mmgr_fract_to_bits(double v)
{
    mmgr_u64 bits = 0;

    mmgr_proxim_read(&bits, &v, sizeof(bits));
    return bits;
}
