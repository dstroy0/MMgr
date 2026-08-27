/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Field access on the bit pattern of a binary64 double.
 */
#include "fractio/fractio.h"

/**
 * @brief Arguments for every fract backend, grouped by the calls that read them.
 *
 * @note Mirrors FractioCfg without its const qualifiers, union included.
 * @note val and bits share storage, so writing one and reading the other reinterprets the same bytes.
 */
typedef struct
{
    union {
        double val;    /**< The value, when the caller supplies a double. */
        mmgr_u64 bits; /**< The same storage read as a bit pattern. */
    };
    mmgr_u64 sign; /**< Sign for merge, 0 or 1. */
    mmgr_u64 exp;  /**< Biased exponent for merge. */
    mmgr_u64 mant; /**< Stored mantissa for merge. */
} FractioCtx;

/**
 * @brief Returns the sign bit of c->bits, as 0 or 1.
 *
 * @param[in] c Bit pattern to read [BORROWS].
 * @return      0 for a positive value, 1 for a negative one.
 */
MMGR_INLINE mmgr_u64 fract_sign(const FractioCtx *c)
{
    return (c->bits & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

/**
 * @brief Returns the raw exponent field of c->bits, still biased.
 *
 * @param[in] c Bit pattern to read [BORROWS].
 * @return      The stored exponent, with MMGR_DBL_EXP_BIAS not yet removed.
 */
MMGR_INLINE mmgr_u64 fract_exp(const FractioCtx *c)
{
    return (c->bits & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

/**
 * @brief Returns the stored mantissa field of c->bits, without the implicit leading bit.
 *
 * @param[in] c Bit pattern to read [BORROWS].
 * @return      The stored mantissa alone.
 */
MMGR_INLINE mmgr_u64 fract_mant(const FractioCtx *c)
{
    return c->bits & MMGR_DBL_MANT_MASK;
}

/**
 * @brief Packs c->sign, c->exp and c->mant back into one bit pattern.
 *
 * @param[in] c The three fields to pack [BORROWS].
 * @return      The assembled bit pattern.
 * @note Each field is masked to its own width first, so a wide input cannot reach a neighbor.
 */
MMGR_INLINE mmgr_u64 fract_merge(const FractioCtx *c)
{
    return ((c->sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) |
           ((c->exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) | (c->mant & MMGR_DBL_MANT_MASK);
}

/**
 * @brief Reads the union as a double after the caller filled its bits member.
 *
 * @param[in] c Union holding the pattern [BORROWS].
 * @return      The same storage read as a double.
 */
MMGR_INLINE double fract_from_bits(const FractioCtx *c)
{
    return c->val;
}

/**
 * @brief Reads the union as a bit pattern after the caller filled its val member.
 *
 * @param[in] c Union holding the value [BORROWS].
 * @return      The same storage read as a bit pattern.
 */
MMGR_INLINE mmgr_u64 fract_to_bits(const FractioCtx *c)
{
    return c->bits;
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_fract_ and fract_ prefixes, which the two share.
 */
#define FRACT_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_fract_, fract_, FractioCtx, FractioCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in fractio.h.
 * @note The union member each line forwards is the one the caller filled: from_bits is given bits and
 *       reads val, to_bits is given val and reads bits. That reinterpretation is the point of both.
 */
FRACT_ENTRY(mmgr_u64, sign, .bits = c->bits)
FRACT_ENTRY(mmgr_u64, exp, .bits = c->bits)
FRACT_ENTRY(mmgr_u64, mant, .bits = c->bits)
FRACT_ENTRY(mmgr_u64, merge, .sign = c->sign, .exp = c->exp, .mant = c->mant)
FRACT_ENTRY(double, from_bits, .bits = c->bits)
FRACT_ENTRY(mmgr_u64, to_bits, .val = c->val)
