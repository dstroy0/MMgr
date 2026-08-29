/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file transformo.c
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
 * @brief Working state for the two scaling paths: the inputs, the 128-bit accumulator, and the
 *        multiply's own members.
 *
 * @note mant, digit, e2, ex, dropped, above and neg are the inputs; dropped is TransformoCfg::rest under a new name.
 * @note hi, lo and fe2 carry the 128-bit significand and its binary exponent; rest records bits shifted away.
 * @note a, b, phi and plo are muto_mul's two operands and its product; pow points at the pow5 entry being applied.
 * @note pow addresses an entry of mmgr_pow5_up or mmgr_pow5_down, which are static const tables that
 *       outlive every call, so it is only ever read through [BORROWS].
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
 * @param[in,out] args The mantissa to extend and the digit to append [BORROWS].
 * @return          MMGR_TRUE when the digit was appended, MMGR_FALSE when *args->mant was already too large.
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
    // Explicit cast widens the digit's value to the mmgr_u64 the mantissa accumulates in
    *args->mant = (*args->mant * 10u) + (mmgr_u64)(args->digit - '0');
    return MMGR_TRUE;
}

/**
 * @brief Multiplies args->a by args->b and leaves the 128-bit product in args->phi and args->plo.
 *
 * @param[in,out] args The two operands, and where the product is left [BORROWS].
 * @note Splits both operands at 32 bits and sums the four partial products, so no 128-bit type is needed.
 * @note mid holds the low half of each middle partial product plus the carry out of the low one; the
 *       two high halves and the carry out of mid go into args->phi.
 * @note The halves are held at 32 bits, so each partial product is a 32 by 32 widening multiply the
 *       target carries in hardware rather than one a compiler must narrow for itself.
 */
MMGR_INLINE void muto_mul(MutoCtx *args)
{
    // Explicit casts hold each half at the 32 bits it actually carries, so the products below are
    // widening multiplies rather than full 64-bit ones
    const uint32_t a0 = (uint32_t)args->a;
    const uint32_t a1 = (uint32_t)(args->a >> 32);
    const uint32_t b0 = (uint32_t)args->b;
    const uint32_t b1 = (uint32_t)(args->b >> 32);

    const mmgr_u64 p00 = (mmgr_u64)a0 * b0;
    const mmgr_u64 p01 = (mmgr_u64)a0 * b1;
    const mmgr_u64 p10 = (mmgr_u64)a1 * b0;
    const mmgr_u64 p11 = (mmgr_u64)a1 * b1;
    // Explicit casts take the low half of the two middle partial products, the part this column
    // holds; their high halves go into phi below. The leading mmgr_u64 term carries the sum at 64
    // bits, so mid keeps its own carry rather than dropping it
    const mmgr_u64 mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;

    args->plo = (mmgr_u64)(uint32_t)p00 | (mid << 32);
    args->phi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief Shifts the 128-bit significand up until args->hi has its top bit set, lowering args->fe2 to match.
 *
 * @param[in,out] args The significand and its binary exponent [BORROWS].
 * @note Moves args->lo up into args->hi first when args->hi is zero, taking 64 off args->fe2.
 * @note Returns with the value untouched when both halves are zero, since there is no top bit to find.
 * @note clz.lead gives the remaining shift, and args->fe2 falls by exactly what the significand rises.
 * @note A count of zero is tested for rather than shifted by, since the complementary shift would
 *       then be by the full width, which is undefined.
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
    // Explicit cast holds the summed exponent at the mmgr_iword fe2 carries
    args->fe2 = (mmgr_iword)(args->fe2 + args->pow->e2 + 128);
    muto_norm(args);
}

/**
 * @brief Expands to 18, the largest power of ten held exactly in a 64-bit integer here.
 *
 * @note Ten to the eighteenth is under two to the sixtieth, so every entry of mmgr_muto_pow10 below
 *       is exact. MMGR_FIXED_MAX_DECIMALS is eighteen as well, so verba_fixed never asks for more.
 */
#define MMGR_MUTO_EXACT_U64_POW10 18

/**
 * @brief Ten raised to 0 through 18, as exact 64-bit integers.
 *
 * @note muto_apply_pow10 applies these for a positive decimal exponent, one outright when the
 *       exponent is within the table and otherwise in chunks of the largest, rather than walking
 *       the pow5 tables a set bit at a time.
 */
static const mmgr_u64 mmgr_muto_pow10[MMGR_MUTO_EXACT_U64_POW10 + 1] = {
    1ull,                  10ull,                 100ull,                1000ull,
    10000ull,              100000ull,             1000000ull,            10000000ull,
    100000000ull,          1000000000ull,         10000000000ull,        100000000000ull,
    1000000000000ull,      10000000000000ull,     100000000000000ull,    1000000000000000ull,
    10000000000000000ull,  100000000000000000ull, 1000000000000000000ull};

/**
 * @brief Multiplies the 128-bit significand by one exact 64-bit power of ten, then renormalizes.
 *
 * @param[in,out] args The significand, its exponent, and the power to apply as args->b [BORROWS].
 * @note Two multiplies where muto_mul_pow5 takes four, and one column sum where it takes three.
 * @note The power is applied whole, both its five and its two, so nothing is added to args->fe2 for it.
 * @note Sets args->rest from the discarded low 64 bits, as muto_mul_pow5 does from its columns.
 * @note Adds 64 to args->fe2, for the bits the 192-bit product was taken down by.
 */
MMGR_INLINE void muto_mul_pow10(MutoCtx *args)
{
    const mmgr_u64 fhi = args->hi;
    const mmgr_u64 flo = args->lo;
    const mmgr_u64 g = args->b;

    args->a = fhi;
    args->b = g;
    muto_mul(args);
    const mmgr_u64 hh_h = args->phi;
    const mmgr_u64 hh_l = args->plo;

    args->a = flo;
    args->b = g;
    muto_mul(args);
    const mmgr_u64 lh_h = args->phi;
    const mmgr_u64 lh_l = args->plo;

    const mmgr_u64 col = hh_l + lh_h;
    const mmgr_u64 carry = (col < hh_l) ? 1u : 0u;

    if (lh_l != 0u)
    {
        args->rest = 1;
    }
    args->hi = hh_h + carry;
    args->lo = col;
    // Explicit cast holds the summed exponent at the mmgr_iword fe2 carries
    args->fe2 = (mmgr_iword)(args->fe2 + 64);
    muto_norm(args);
}

/**
 * @brief Multiplies the significand by ten raised to args->ex.
 *
 * @param[in,out] args The significand, its exponent, and the decimal exponent to apply [BORROWS].
 * @note A positive exponent takes exact powers of ten, one outright within the table's reach and
 *       otherwise in chunks of the largest, and returns without touching the pow5 tables.
 * @note Everything else walks the bits of the magnitude of args->ex and applies one pow5 entry for
 *       each bit that is set, from mmgr_pow5_down when args->ex is negative and mmgr_pow5_up when it
 *       is not.
 * @note Adding args->ex to args->fe2 at the end of that walk supplies the two raised to args->ex half
 *       of ten raised to args->ex; the exact power path needs no such addition, since it applies the
 *       whole power.
 * @warning Only MMGR_POW5_STEPS bits are walked, so a magnitude above MMGR_POW5_MAX that reaches the
 *          walk loses its higher bits.
 */
MMGR_INLINE void muto_apply_pow10(MutoCtx *args)
{
    // A positive exponent is applied as exact powers of ten rather than by walking the bits of the
    // wide tables. Ten to the eighteenth is the widest that fits 64 bits, so a larger exponent goes
    // on in chunks of it: each chunk is a 128 by 64 multiply, where every set bit of the walk is a
    // 128 by 128 one, and the walk needs one per bit rather than one per eighteen.
    if (args->ex > 0)
    {
        // Below the table's reach one power covers the whole exponent, so it needs no loop
        if (args->ex <= MMGR_MUTO_EXACT_U64_POW10)
        {
            args->b = mmgr_muto_pow10[args->ex];
            muto_mul_pow10(args);
            return;
        }

        mmgr_iword left = args->ex;

        while (left > 0)
        {
            const mmgr_iword take = (left > MMGR_MUTO_EXACT_U64_POW10) ? MMGR_MUTO_EXACT_U64_POW10 : left;

            args->b = mmgr_muto_pow10[take];
            muto_mul_pow10(args);
            left -= take;
        }
        return;
    }

    // Explicit cast holds the negated exponent at the mmgr_iword the walk counts down in
    mmgr_iword k = (args->ex < 0) ? (mmgr_iword)(-args->ex) : args->ex;

    // Two bounds: the step count keeps an exponent past the tables from reading off the end, and the
    // emptied magnitude ends the walk once no bits are left to apply
    for (mmgr_iword i = 0; (i < MMGR_POW5_STEPS) && (k != 0); ++i)
    {
        if ((k & 1) != 0)
        {
            args->pow = (args->ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];
            muto_mul_pow5(args);
        }
        k >>= 1;
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
    // Explicit cast widens the dropped-bits flag to the mmgr_u64 the sticky bits are gathered in
    mmgr_u64 rest = (mmgr_u64)args->rest | ((args->lo != 0u) ? 1u : 0u) | (((args->hi & 0x3FFu) != 0u) ? 1u : 0u);
    // Explicit cast holds the mantissa width at the mmgr_iword the biased exponent is summed in
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

    // Explicit cast holds the all-ones exponent at the mmgr_iword be is compared in
    if (be >= (mmgr_iword)MMGR_DBL_EXP_ALL)
    {
        const double big = 1.0e308 * 10.0;
        return args->neg ? -big : big;
    }

    // Explicit casts widen the sign selection and the biased exponent to the mmgr_u64 fields
    // FractioCfg carries; be is non-negative here, since the branches above pinned it
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
 * @note The three branches split on j, the part of the shift past 64: none of it, under 64 more, and
 *       64 or more, in that order.
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
        // Explicit cast pins the all-ones saturation value at the mmgr_u64 this returns
        return ~(mmgr_u64)0;
    }

    // Explicit cast holds the shift width at the mmgr_word the shifts below take; the tests above
    // established k is between 64 and 128, so the subtraction cannot wrap
    const mmgr_word j = (mmgr_word)(k - 64);
    // The j == 0u arm stands in for the empty mask: j - 1u would wrap to the largest mmgr_word there,
    // and a shift by that is undefined. Explicit cast widens the one to the mmgr_u64 the mask covers,
    // so the shift has room
    const mmgr_u64 low_mask = (j == 0u) ? 0u : (((mmgr_u64)1 << (j - 1u)) - 1u);
    mmgr_u64 whole;
    mmgr_u64 half;
    mmgr_u64 rest = (args->rest != 0) ? 1u : 0u;

    if (j == 0u)
    {
        whole = args->hi;
        half = (args->lo >> 63) & 1u;
        // Explicit cast widens the one to the mmgr_u64 the low word is masked in
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
        // Explicit cast widens the one to the mmgr_u64 the high word is masked in
        rest |= ((args->hi & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
        rest |= (args->lo != 0u) ? 1u : 0u;
    }

    // Explicit cast narrows the low bit to the mmgr_word above is carried in, so the two meet at one width
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
 * @note The divide below is the expensive half of the exact path and is meant to stay a divide. On
 *       an ESP32-S3 a soft double divide measured 639 cycles against 184 for a multiply, and that
 *       one divide is fifty six percent of what cellul.to_double costs end to end. Multiplying by
 *       the reciprocal would buy all of it and would not be the same function: a divide by a power
 *       of ten the table holds exactly rounds correctly, and the inverse of that power is not
 *       representable, so the last bit moves for some inputs. The other exact route is the fixed
 *       point path below, which measured about three times the divide.
 * @warning Returns a signed infinity above MMGR_POW5_MAX and a signed zero below its negative.
 */
MMGR_INLINE double muto_scale(MutoCtx *args)
{
    if (*args->mant == 0u)
    {
        return args->neg ? -0.0 : 0.0;
    }

    // The gate for the exact path: nothing dropped, a mantissa a double holds outright, and an
    // exponent mmgr_muto_ten covers. Explicit cast widens the one to the mmgr_u64 the mantissa is
    // compared in, so the shift has room
    if ((args->dropped == 0) && (*args->mant < ((mmgr_u64)1 << 53)) && (args->ex >= -MMGR_MUTO_EXACT_POW10) &&
        (args->ex <= MMGR_MUTO_EXACT_POW10))
    {
        // Explicit cast converts the mantissa to the double this path scales it in, exact below 2^53
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
 * @param[in] ...  The MutoCtx member initializers this entry point fills from its TransformoCfg.
 */
#define MUTO_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_muto_, muto_, MutoCtx, TransformoCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in transformo.h.
 * @note scale forwards args->rest as MutoCtx::dropped; the two structs give that member different names.
 * @note The members each line leaves out stay zero in the compound literal. scale omits e2, so its
 *       mantissa carries no binary exponent, and scale_to_u64 omits dropped and neg.
 */
MUTO_ENTRY(mmgr_bool, take, .mant = args->mant, .digit = args->digit)
MUTO_ENTRY(double, scale, .mant = args->mant, .ex = args->ex, .dropped = args->rest, .neg = args->neg)
MUTO_ENTRY(mmgr_u64, scale_to_u64, .mant = args->mant, .e2 = args->e2, .ex = args->ex, .above = args->above)
