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
#define MMGR_DBL_EXP_BITS 11u
#define MMGR_DBL_SIGN_ONE 0x1ull
#define MMGR_DBL_EXP_ALL 0x7FFull
#define MMGR_DBL_BIAS 1023

/** @brief Bits, bytes and words in a double. A legal one is exactly this and nothing else. */
#define MMGR_DBL_BITS 64u
#define MMGR_DBL_BYTES (MMGR_DBL_BITS / 8u)
#define MMGR_DBL_WORDS ((MMGR_DBL_BITS + (MMGR_WORD_BITS - 1u)) / MMGR_WORD_BITS)

/*
 * The shape of a double, settled here so nothing below has to wonder.
 *
 * Everything in this file reads a double by its bit positions: the exponent is eleven bits at
 * fifty-two, the mantissa is the fifty-two under it, the implicit bit is there exactly when the
 * exponent field is not zero. Those positions are not a convention this library chose, they are
 * binary64, and they are only where this file says they are if the target's double is binary64.
 *
 * Plenty of embedded toolchains ship a double that is not. Where double is the same thing as
 * float, the exponent is eight bits at twenty-three and every mask below reads the wrong bits of
 * a value half the size - and reads them without complaint, because a mask of a smaller type is
 * legal C. The rendering would come back wrong and nothing would say why. So it is settled at
 * compile time, once, and a target that disagrees fails to build rather than shipping.
 *
 * The word count is the same question asked at the granularity this library moves data in: a
 * double is one word at sixty-four bits, two at thirty-two, four at sixteen. Anything arriving
 * from outside that claims to be a double and is not that many words is malformed before a single
 * bit of it is looked at, and this is the constant that says so.
 */
MMGR_STATIC_ASSERT(sizeof(double) * 8u == MMGR_DBL_BITS,
                   "double is not 64 bits on this target, so every field position in this file is wrong "
                   "- a build where double means float cannot use it");
MMGR_STATIC_ASSERT(MMGR_DBL_WORDS *MMGR_WORD_BITS == MMGR_DBL_BITS,
                   "a double is not a whole number of words on this target");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) == sizeof(double), "the bit pattern of a double does not fit mmgr_u64");

/* The fields tile the word exactly: one sign, eleven exponent, fifty-two mantissa. */
MMGR_STATIC_ASSERT(1u + MMGR_DBL_EXP_BITS + MMGR_DBL_MANT_BITS == MMGR_DBL_BITS,
                   "the three fields do not add up to the width of the value");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK | MMGR_DBL_EXP_MASK | MMGR_DBL_MANT_MASK) == 0xFFFFFFFFFFFFFFFFull,
                   "the three field masks leave a gap");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK & MMGR_DBL_EXP_MASK) == 0u && (MMGR_DBL_EXP_MASK & MMGR_DBL_MANT_MASK) == 0u &&
                       (MMGR_DBL_SIGN_MASK & MMGR_DBL_MANT_MASK) == 0u,
                   "the three field masks overlap");
MMGR_STATIC_ASSERT(MMGR_DBL_SIGN_MASK == (MMGR_DBL_SIGN_ONE << MMGR_DBL_SIGN_SHIFT),
                   "the sign mask and the sign shift disagree about where the sign is");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_MASK == (MMGR_DBL_EXP_ALL << MMGR_DBL_MANT_BITS),
                   "the exponent mask and the exponent width disagree");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_ALL == ((1u << MMGR_DBL_EXP_BITS) - 1u), "the exponent does not fill its field");
MMGR_STATIC_ASSERT(MMGR_DBL_BIAS == ((1 << (MMGR_DBL_EXP_BITS - 1u)) - 1), "the bias is not the one binary64 uses");

/*
 * And the arithmetic the two conversions rest on, so the bounds are derived here rather than
 * assumed there. The exact value of a finite double is mant * 2^scale, with the mantissa at most
 * fifty-three bits including the implicit one.
 */
#define MMGR_DBL_SCALE_MAX ((int)(MMGR_DBL_EXP_ALL - 1u) - MMGR_DBL_BIAS - (int)MMGR_DBL_MANT_BITS)
#define MMGR_DBL_SCALE_MIN (1 - MMGR_DBL_BIAS - (int)MMGR_DBL_MANT_BITS)

MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MAX == 971, "the largest scale a finite double can carry is not what it was");
MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MIN == -1074, "the smallest scale a subnormal can carry is not what it was");

#if defined(__STDC_IEC_559__)
/* The target says it is IEC 60559, which is the standard's name for what is asserted above. */
#endif

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_u64 (*sign)(double v);
    mmgr_u64 (*exp)(double v);
    mmgr_u64 (*mant)(double v);
    mmgr_u64 (*merge)(mmgr_u64 sign, mmgr_u64 exp, mmgr_u64 mant);
    double (*from_bits)(mmgr_u64 bits);
    mmgr_u64 (*to_bits)(double v);
} FractioNs;
MMGR_NS_LAYOUT(FractioNs, sign, exp, mant, merge, from_bits, to_bits);

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
/**
 * @brief Reinterpret a double as its bit pattern.
 * @param v Value.
 * @return The bits.
 *
 * The other direction of from_bits. Taking a value apart into its three fields and putting them
 * back together gets the same number out, and costs three reads and four operations to recover
 * something that was already sitting there.
 */
mmgr_u64 mmgr_fract_to_bits(double v);

/** @brief Module namespace. */
MMGR_NS FractioNs fract MMGR_UNUSED = {
    .sign = mmgr_fract_sign,
    .exp = mmgr_fract_exp,
    .mant = mmgr_fract_mant,
    .merge = mmgr_fract_merge,
    .from_bits = mmgr_fract_from_bits,
    .to_bits = mmgr_fract_to_bits,
};

MMGR_END_DECLS

#endif
