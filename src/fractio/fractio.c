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
    return proxim.u64(&v);
}

/**
 * @brief Sign bit.
 * @param v The value.
 * @return 0 or 1.
 */
MMGR_INLINE mmgr_u64 fract_sign(double v)
{
    return (fract_bits(v) & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

/**
 * @brief Raw exponent field, still biased.
 * @param v The value.
 * @return Exponent bits.
 */
MMGR_INLINE mmgr_u64 fract_exp(double v)
{
    return (fract_bits(v) & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

/**
 * @brief Mantissa field, without the implicit leading bit.
 * @param v The value.
 * @return Mantissa bits.
 */
MMGR_INLINE mmgr_u64 fract_mant(double v)
{
    return fract_bits(v) & MMGR_DBL_MANT_MASK;
}

/**
 * @brief Assemble the three fields into a bit pattern.
 * @param sign Sign bit.
 * @param exp Raw exponent.
 * @param mant Mantissa.
 * @return The bit pattern.
 *
 * Each field is masked to its own width before it is shifted, so a caller that hands over a value
 * too wide for the field cannot spill it into the field above.
 */
MMGR_INLINE mmgr_u64 fract_merge(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant)
{
    return ((sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) | ((exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) |
           (mant & MMGR_DBL_MANT_MASK);
}

/**
 * @brief Reinterpret a bit pattern as a double.
 * @param bits The bit pattern.
 * @return The value.
 */
MMGR_INLINE double fract_from_bits(mmgr_u64 bits)
{
    double v = 0.0;

    proxim.read(&v, &bits, sizeof(v));
    return v;
}

/**
 * @brief Reinterpret a double as its bit pattern.
 * @param v The value.
 * @return The bits.
 *
 * The other direction of from_bits. Taking a value apart into its three fields and putting them
 * back together gets the same number out, and costs three reads and four operations to recover
 * something that was already sitting there.
 */
MMGR_INLINE mmgr_u64 fract_to_bits(double v)
{
    mmgr_u64 bits = 0;

    proxim.read(&bits, &v, sizeof(bits));
    return bits;
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each hands its arguments to the body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

mmgr_u64 mmgr_fract_sign(double v)
{
    return fract_sign(v);
}

mmgr_u64 mmgr_fract_exp(double v)
{
    return fract_exp(v);
}

mmgr_u64 mmgr_fract_mant(double v)
{
    return fract_mant(v);
}

mmgr_u64 mmgr_fract_merge(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant)
{
    return fract_merge(sign, exp, mant);
}

double mmgr_fract_from_bits(mmgr_u64 bits)
{
    return fract_from_bits(bits);
}

mmgr_u64 mmgr_fract_to_bits(double v)
{
    return fract_to_bits(v);
}
