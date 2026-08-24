/**
 * @brief Field access on the bit pattern of a binary64 double.
 */
#include "fractio/fractio.h"

/**
 * @brief Returns the sign bit of c->bits, as 0 or 1.
 *
 * @note Documented at the declaration in fractio.h.
 */
mmgr_u64 mmgr_fract_sign(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

/**
 * @brief Returns the raw exponent field of c->bits, still biased.
 *
 * @note Documented at the declaration in fractio.h.
 */
mmgr_u64 mmgr_fract_exp(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

/**
 * @brief Returns the stored mantissa field of c->bits, without the implicit leading bit.
 *
 * @note Documented at the declaration in fractio.h.
 */
mmgr_u64 mmgr_fract_mant(const FractioCfg *c)
{
    return c->bits & MMGR_DBL_MANT_MASK;
}

/**
 * @brief Packs c->sign, c->exp and c->mant back into one bit pattern.
 *
 * @note Each field is masked to its own width first, so a wide input cannot reach a neighbor.
 * @note Documented at the declaration in fractio.h.
 */
mmgr_u64 mmgr_fract_merge(const FractioCfg *c)
{
    return ((c->sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) |
           ((c->exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) | (c->mant & MMGR_DBL_MANT_MASK);
}

/**
 * @brief Reads the union as a double after the caller filled its bits member.
 *
 * @note Documented at the declaration in fractio.h.
 */
double mmgr_fract_from_bits(const FractioCfg *c)
{
    return c->val;
}

/**
 * @brief Reads the union as a bit pattern after the caller filled its val member.
 *
 * @note Documented at the declaration in fractio.h.
 */
mmgr_u64 mmgr_fract_to_bits(const FractioCfg *c)
{
    return c->bits;
}
