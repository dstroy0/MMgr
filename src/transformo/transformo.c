/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Turns a decimal mantissa and exponent into a double, or into a rounded 64-bit integer.
 *
 * @note Small exact cases go through plain double arithmetic; everything else goes through 128-bit fixed point.
 * @note The fixed-point path multiplies in one pow5 entry per set bit of the decimal exponent.
 */
#include "transformo/transformo.h"
#include "clz/clz.h"

/**
 * @brief Expands to 22, the largest power of ten a double holds exactly.
 *
 * @note Sizes mmgr_muto_ten, and bounds the exponent range muto_scale settles with double arithmetic.
 */
#define MMGR_MUTO_EXACT_POW10 22

/**
 * @brief Ten raised to 0 through 22, as doubles, indexed by the exponent itself.
 *
 * @note Every entry is exact, since 22 is the last power of ten a double represents without rounding.
 * @note muto_scale multiplies by one of these for a positive exponent and divides by one for a negative one.
 */
static const double mmgr_muto_ten[MMGR_MUTO_EXACT_POW10 + 1] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                                1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                                1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

/**
 * @brief Working state for the two scaling paths: the inputs, the 128-bit accumulator, and multiply scratch.
 *
 * @note mant, digit, e2, ex, dropped, above and neg are the inputs; dropped is TransformoCfg::rest under a new name.
 * @note hi, lo and fe2 carry the 128-bit significand and its binary exponent; rest records bits shifted away.
 * @note a, b, phi and plo are muto_mul's two operands and its product; pow points at the pow5 entry being applied.
 * @warning mant is written through, so muto_take changes the caller's mantissa [BORROWS].
 */
typedef struct
{
    mmgr_u64 *mant;
    char digit;
    mmgr_iword e2;
    mmgr_iword ex;
    mmgr_iword dropped;
    mmgr_word above;
    mmgr_bool neg;
    mmgr_u64 hi;
    mmgr_u64 lo;
    mmgr_iword fe2;
    mmgr_iword rest;
    mmgr_u64 a;
    mmgr_u64 b;
    mmgr_u64 phi;
    mmgr_u64 plo;

    const MmgrPow5 *pow;
} MutoCtx;

/**
 * @brief Appends args->digit to *args->mant as one more decimal digit.
 *
 * @param[in] args The mantissa to extend and the digit to append [BORROWS].
 * @return      MMGR_TRUE when the digit was appended, MMGR_FALSE when *args->mant was already too large.
 * @note Multiplies by ten and adds args->digit minus '0', so args->digit must be an ASCII decimal digit.
 * @note MMGR_MUTO_MANT_MAX is (~0 - 9) / 10, so the multiply and the add both stay inside a 64-bit value.
 * @warning Writes through args->mant, so the caller's mantissa changes [BORROWS].
 */
MMGR_INLINE mmgr_bool muto_take(const MutoCtx *args)
{
    if (*args->mant > MMGR_MUTO_MANT_MAX)
    {
        return MMGR_FALSE;
    }
    *args->mant = (*args->mant * 10u) + (mmgr_u64)(args->digit - '0');
    return MMGR_TRUE;
}

/**
 * @brief Multiplies args->a by args->b and leaves the 128-bit product in args->phi and args->plo.
 *
 * @param[in,out] args The two operands, and where the product is left [BORROWS].
 * @note Splits both operands at 32 bits and sums the four partial products, so no 128-bit type is needed.
 * @note mid holds the two middle partial products plus the carry out of the low one.
 */
MMGR_INLINE void muto_mul(MutoCtx *args)
{
    const mmgr_u64 half = (mmgr_u64)0xFFFFFFFFu;
    const mmgr_u64 a0 = args->a & half;
    const mmgr_u64 a1 = args->a >> 32;
    const mmgr_u64 b0 = args->b & half;
    const mmgr_u64 b1 = args->b >> 32;

    const mmgr_u64 p00 = a0 * b0;
    const mmgr_u64 p01 = a0 * b1;
    const mmgr_u64 p10 = a1 * b0;
    const mmgr_u64 p11 = a1 * b1;
    const mmgr_u64 mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    args->plo = (p00 & half) | (mid << 32);
    args->phi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief Shifts the 128-bit significand up until args->hi has its top bit set, lowering args->fe2 to match.
 *
 * @param[in,out] args The significand and its binary exponent [BORROWS].
 * @note Moves args->lo up into args->hi first when args->hi is zero, taking 64 off args->fe2.
 * @note Returns with the value untouched when both halves are zero, since there is no top bit to find.
 * @note clz.lead gives the remaining shift, and args->fe2 falls by exactly what the significand rises.
 */
MMGR_INLINE void muto_norm(MutoCtx *args)
{
    if (args->hi == 0u)
    {
        if (args->lo == 0u)
        {
            return;
        }
        args->hi = args->lo;
        args->lo = 0u;
        args->fe2 -= 64;
    }

    const mmgr_iword n = MMGR_CALL(clz.lead, ClzCfg, .val = args->hi);
    if (n != 0)
    {
        args->hi = (args->hi << n) | (args->lo >> (64 - n));
        args->lo <<= n;
        args->fe2 -= n;
    }
}

/**
 * @brief Multiplies the 128-bit significand by args->pow, keeps the top 128 bits, then renormalizes.
 *
 * @param[in,out] args The significand, its exponent, and the pow5 entry to apply [BORROWS].
 * @note Builds the 256-bit product from four 64-bit multiplies, then adds the columns with their carries.
 * @note Sets args->rest when either discarded column held anything, so the rounding still sees those bits.
 * @note Adds args->pow->e2 and 128 to args->fe2, the 128 standing for the bits the product was taken down by.
 */
MMGR_INLINE void muto_mul_pow5(MutoCtx *args)
{
    const mmgr_u64 fhi = args->hi;
    const mmgr_u64 flo = args->lo;
    const mmgr_u64 ghi = args->pow->hi;
    const mmgr_u64 glo = args->pow->lo;

    args->a = fhi;
    args->b = ghi;
    muto_mul(args);
    const mmgr_u64 hh_h = args->phi;
    const mmgr_u64 hh_l = args->plo;

    args->a = fhi;
    args->b = glo;
    muto_mul(args);
    const mmgr_u64 hl_h = args->phi;
    const mmgr_u64 hl_l = args->plo;

    args->a = flo;
    args->b = ghi;
    muto_mul(args);
    const mmgr_u64 lh_h = args->phi;
    const mmgr_u64 lh_l = args->plo;

    args->a = flo;
    args->b = glo;
    muto_mul(args);
    const mmgr_u64 ll_h = args->phi;
    const mmgr_u64 ll_l = args->plo;

    mmgr_u64 carry = 0u;
    mmgr_u64 col1 = ll_h + hl_l;
    carry += (col1 < ll_h) ? 1u : 0u;
    const mmgr_u64 col1b = col1 + lh_l;
    carry += (col1b < col1) ? 1u : 0u;
    col1 = col1b;

    mmgr_u64 col2 = hh_l + hl_h;
    mmgr_u64 carry2 = (col2 < hh_l) ? 1u : 0u;
    const mmgr_u64 col2b = col2 + lh_h;
    carry2 += (col2b < col2) ? 1u : 0u;
    col2 = col2b + carry;
    carry2 += (col2 < col2b) ? 1u : 0u;

    if ((ll_l != 0u) || (col1 != 0u))
    {
        args->rest = 1;
    }
    args->hi = hh_h + carry2;
    args->lo = col2;
    args->fe2 = (mmgr_iword)(args->fe2 + args->pow->e2 + 128);
    muto_norm(args);
}

/**
 * @brief Multiplies the significand by ten raised to args->ex.
 *
 * @param[in,out] args The significand, its exponent, and the decimal exponent to apply [BORROWS].
 * @note Walks the bits of the magnitude of args->ex and applies one pow5 entry for each bit that is set.
 * @note Takes entries from mmgr_pow5_down when args->ex is negative and from mmgr_pow5_up when it is not.
 * @note Adding args->ex to args->fe2 at the end supplies the two raised to args->ex half of ten raised to args->ex.
 * @warning Only MMGR_POW5_STEPS bits are walked, so any magnitude above MMGR_POW5_MAX loses its higher bits.
 */
MMGR_INLINE void muto_apply_pow10(MutoCtx *args)
{
    const mmgr_iword k = (args->ex < 0) ? (mmgr_iword)(-args->ex) : args->ex;

    for (mmgr_iword i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            args->pow = (args->ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];
            muto_mul_pow5(args);
        }
    }
    args->fe2 += args->ex;
}

/**
 * @brief Loads *args->mant into the 128-bit significand and normalizes it.
 *
 * @param[in,out] args The mantissa, its binary exponent, and the bits already dropped [BORROWS].
 * @note Puts the mantissa in args->hi with args->lo zero, so args->fe2 starts 64 below args->e2.
 * @note Carries args->dropped into args->rest, so a truncation the caller already made still reaches the rounding.
 */
MMGR_INLINE void muto_seat(MutoCtx *args)
{
    args->hi = *args->mant;
    args->lo = 0u;
    args->fe2 = args->e2 - 64;
    args->rest = args->dropped;
    muto_norm(args);
}

/**
 * @brief Rounds the 128-bit significand to a double, ties to even.
 *
 * @param[in] args The significand, its exponent, the bits already dropped and the sign [BORROWS].
 * @return      The rounded double, signed by args->neg.
 * @note Takes the top 53 bits of args->hi as the mantissa, the next bit as the halfway bit, and folds the rest in.
 * @note Shifts right into the subnormal range when the biased exponent lands at zero or below.
 * @note Rounds up on a halfway bit only when something is left below it or the mantissa is odd, which ties to even.
 * @warning Returns a signed infinity at MMGR_DBL_EXP_ALL, and a signed zero when the value falls below the range.
 */
MMGR_INLINE double muto_round(const MutoCtx *args)
{
    if ((args->hi | args->lo) == 0u)
    {
        return args->neg ? -0.0 : 0.0;
    }

    mmgr_u64 mant = args->hi >> 11;
    mmgr_u64 half = (args->hi >> 10) & 1u;
    mmgr_u64 rest = (mmgr_u64)args->rest | ((args->lo != 0u) ? 1u : 0u) | (((args->hi & 0x3FFu) != 0u) ? 1u : 0u);
    mmgr_iword be = args->fe2 + 75 + (mmgr_iword)MMGR_DBL_MANT_BITS + MMGR_DBL_BIAS;

    if (be <= 0)
    {
        mmgr_iword shift = 1 - be;
        if (shift > 60)
        {
            return args->neg ? -0.0 : 0.0;
        }
        while (shift-- > 0)
        {
            rest |= half;
            half = mant & 1u;
            mant >>= 1;
        }
        be = 0;
    }

    if ((half != 0u) && ((rest != 0u) || ((mant & 1u) != 0u)))
    {
        mant += 1u;
        if ((mant >> 53) != 0u)
        {
            mant >>= 1;
            be += 1;
        }
        else if ((be == 0) && ((mant >> MMGR_DBL_MANT_BITS) != 0u))
        {
            be = 1;
        }
    }

    if (be >= (mmgr_iword)MMGR_DBL_EXP_ALL)
    {
        const double big = 1.0e308 * 10.0;
        return args->neg ? -big : big;
    }
    const mmgr_u64 bits = MMGR_CALL(fract.merge, FractioCfg, .sign = (mmgr_u64)(args->neg ? MMGR_DBL_SIGN_ONE : 0u),
                                    .exp = (mmgr_u64)be, .mant = mant & MMGR_DBL_MANT_MASK);

    return MMGR_CALL(fract.from_bits, FractioCfg, .bits = bits);
}

/**
 * @brief Rounds the 128-bit significand to a 64-bit integer, ties to even.
 *
 * @param[in] args The significand, its exponent, the bits already dropped and the tie bias [BORROWS].
 * @return      The rounded integer, 0 when the value rounds below one, or all ones when it needs more than 64 bits.
 * @note k is the negated exponent, so it says how far right the significand must move to become an integer.
 * @note The three branches cover a shift of exactly 64, one under 64, and one of 64 or more, in that order.
 * @note args->above is exclusive-ored into the low bit before the tie test, so a caller can steer which way a tie goes.
 */
MMGR_INLINE mmgr_u64 muto_to_u64(const MutoCtx *args)
{
    if ((args->hi | args->lo) == 0u)
    {
        return 0u;
    }

    const mmgr_iword k = -(args->fe2);

    if (k > 128)
    {
        return 0u;
    }
    if (k < 64)
    {
        return ~(mmgr_u64)0;
    }

    const mmgr_word j = (mmgr_word)(k - 64);
    const mmgr_u64 low_mask = (j == 0u) ? 0u : (((mmgr_u64)1 << (j - 1u)) - 1u);
    mmgr_u64 whole;
    mmgr_u64 half;
    mmgr_u64 rest = (args->rest != 0) ? 1u : 0u;

    if (j == 0u)
    {
        whole = args->hi;
        half = (args->lo >> 63) & 1u;
        rest |= ((args->lo & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
    }
    else if (j < 64u)
    {
        whole = args->hi >> j;
        half = (args->hi >> (j - 1u)) & 1u;
        rest |= ((args->hi & low_mask) != 0u) ? 1u : 0u;
        rest |= (args->lo != 0u) ? 1u : 0u;
    }
    else
    {
        whole = 0u;
        half = args->hi >> 63;
        rest |= ((args->hi & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
        rest |= (args->lo != 0u) ? 1u : 0u;
    }

    const mmgr_word odd = ((mmgr_word)(whole & 1u)) ^ (args->above & 1u);

    if ((half != 0u) && ((rest != 0u) || (odd != 0u)))
    {
        whole += 1u;
    }
    return whole;
}

/**
 * @brief Turns *args->mant times ten raised to args->ex into a double.
 *
 * @param[in,out] args The mantissa, the decimal exponent, the dropped bits and the sign [BORROWS].
 * @return          The value as a double, signed by args->neg.
 * @note Uses plain double arithmetic when nothing was dropped, the mantissa is under 2^53, and args->ex is within 22.
 * @note Otherwise seats the mantissa, applies the power of ten, and rounds, all in 128-bit fixed point.
 * @note A zero mantissa returns a signed zero before either path is taken.
 * @warning Returns a signed infinity above MMGR_POW5_MAX and a signed zero below its negative.
 */
MMGR_INLINE double muto_scale(MutoCtx *args)
{
    if (*args->mant == 0u)
    {
        return args->neg ? -0.0 : 0.0;
    }

    if ((args->dropped == 0) && (*args->mant < ((mmgr_u64)1 << 53)) && (args->ex >= -MMGR_MUTO_EXACT_POW10) &&
        (args->ex <= MMGR_MUTO_EXACT_POW10))
    {
        double v = (double)*args->mant;

        if (args->ex > 0)
        {
            v *= mmgr_muto_ten[args->ex];
        }
        else if (args->ex < 0)
        {
            v /= mmgr_muto_ten[-args->ex];
        }
        return args->neg ? -v : v;
    }
    if (args->ex > MMGR_POW5_MAX)
    {
        const double big = 1.0e308 * 10.0;
        return args->neg ? -big : big;
    }
    if (args->ex < -MMGR_POW5_MAX)
    {
        return args->neg ? -0.0 : 0.0;
    }

    muto_seat(args);
    muto_apply_pow10(args);
    return muto_round(args);
}

/**
 * @brief Turns *args->mant times ten raised to args->ex into a rounded 64-bit integer.
 *
 * @param[in,out] args The mantissa, its binary exponent, the decimal exponent and the tie bias [BORROWS].
 * @return          The rounded integer, or 0 when *args->mant is zero.
 * @note Always takes the 128-bit path, with no exact double shortcut, unlike muto_scale.
 * @note Reads args->e2, which muto_scale leaves at zero.
 * @warning Does not bound args->ex against MMGR_POW5_MAX the way muto_scale does, so a larger one loses its high bits.
 */
MMGR_INLINE mmgr_u64 muto_scale_to_u64(MutoCtx *args)
{
    if (*args->mant == 0u)
    {
        return 0u;
    }

    muto_seat(args);
    muto_apply_pow10(args);
    return muto_to_u64(args);
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_muto_ and muto_ prefixes, which the two share.
 */
#define MUTO_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_muto_, muto_, MutoCtx, TransformoCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in transformo.h.
 * @note scale forwards args->rest as MutoCtx::dropped; the two structs give that member different names.
 * @note The members each line leaves out stay zero in the compound literal. scale omits e2, so its
 *       mantissa carries no binary exponent, and scale_to_u64 omits rest and neg.
 */
MUTO_ENTRY(mmgr_bool, take, .mant = args->mant, .digit = args->digit)
MUTO_ENTRY(double, scale, .mant = args->mant, .ex = args->ex, .dropped = args->rest, .neg = args->neg)
MUTO_ENTRY(mmgr_u64, scale_to_u64, .mant = args->mant, .e2 = args->e2, .ex = args->ex, .above = args->above)
