// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "fractio/fractio.h"
#include "mmgr_config.h"
#include "pow5/pow5.h"

MMGR_BEGIN_DECLS

/**
 * @file transformo.h
 * @brief Turning a decimal mantissa and exponent into the double it names, exactly.
 *
 * A decimal value is mant times ten to the ex, and ten to the ex is five to the ex times two to the
 * ex. The twos never need arithmetic - a power of two is the exponent field, so applying one is an
 * add to that field and cannot round. Only the fives need carrying, and they are the same nine
 * numbers every time, which is what pow5 holds.
 *
 * The intermediate is 128 bits and never grows. An exact decimal expansion would need thousands -
 * a subnormal wants five to the 1074th, which is a 2494 bit integer - but nobody needs the
 * expansion. What is needed is enough bits to decide the rounding: fifty three that become the
 * mantissa, one below them, and one bit saying whether anything at all is set under that. A
 * hundred and twenty eight carries all three with seventy four to spare, because normalising after
 * each step holds the fraction in place and pushes the growth into an int.
 *
 * The rounding reads the same three places every time, whatever the value was. Round bit clear is
 * down. Set with something below is up. Set with nothing below is the tie, and the tie goes to
 * even. Nothing here looks at what the value was.
 *
 * This is the engine, not a policy. It was written inside the parser and lives here because the
 * render side has the same problem in the other direction and should not solve it twice. See
 * @ref qa_numeric.
 */

/** @brief Largest mantissa that can still take another digit without wrapping. */
#define MMGR_MUTO_MANT_MAX ((mmgr_u64)((~(mmgr_u64)0 - 9u) / 10u))

/** @brief Beyond this the value is an infinity or a zero and the digits stop mattering. */
#define MMGR_MUTO_EXP_LIMIT 400

/**
 * @brief Take one more decimal digit, if there is room for it.
 * @param mant In/out. Running mantissa.
 * @param c The digit.
 * @return MMGR_TRUE when it was taken.
 *
 * A digit past what the mantissa can hold is not dropped, it is counted: an integer digit that did
 * not fit is a factor of ten the exponent has to carry instead.
 */
MMGR_INLINE mmgr_bool mmgr_muto_take(mmgr_u64 *mant, char c)
{
    if (*mant > MMGR_MUTO_MANT_MAX)
    {
        return MMGR_FALSE;
    }
    *mant = (*mant * 10u) + (mmgr_u64)(c - '0');
    return MMGR_TRUE;
}

/**
 * @brief A 128 bit fraction with the top bit set, times two to the e2.
 *
 * Wide enough to decide the rounding of a double and no wider. Fifty three bits become the
 * mantissa, one below them decides the direction, and everything under that only has to be known
 * to be zero or not - so a hundred and twenty eight bits carries the answer with seventy four to
 * spare, and nothing here ever grows.
 */
typedef struct
{
    mmgr_u64 hi;
    mmgr_u64 lo;
    int e2;
    int rest; /* something was shifted off the bottom, so a tie is not a tie */
} mmgr_muto_fix;

/**
 * @brief @p a times @p b, as a 128 bit answer.
 * @param a First.
 * @param b Second.
 * @param hi Out. High half.
 * @param lo Out. Low half.
 *
 * Four partial products of the halves, reassembled with the carries written out. The same shape at
 * any width: at sixty four bits the halves are thirty two, at thirty two they are sixteen, and the
 * reassembly does not change.
 */
MMGR_INLINE void mmgr_muto_mul(mmgr_u64 a, mmgr_u64 b, mmgr_u64 *hi, mmgr_u64 *lo)
{
    const mmgr_u64 half = (mmgr_u64)0xFFFFFFFFu;
    const mmgr_u64 a0 = a & half;
    const mmgr_u64 a1 = a >> 32;
    const mmgr_u64 b0 = b & half;
    const mmgr_u64 b1 = b >> 32;

    const mmgr_u64 p00 = a0 * b0;
    const mmgr_u64 p01 = a0 * b1;
    const mmgr_u64 p10 = a1 * b0;
    const mmgr_u64 p11 = a1 * b1;
    const mmgr_u64 mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    *lo = (p00 & half) | (mid << 32);
    *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief Bring the top bit up, moving the exponent to match.
 * @param f In/out. The fraction.
 */
MMGR_INLINE int mmgr_muto_clz(mmgr_u64 x)
{
    int n = 0;

    /* Halve the unknown each time. Six tests for sixty four bits, and no table and no builtin -
       __builtin_clzll is a call to libgcc on a baseline target, which is what this avoids. */
    if ((x >> 32) == 0u)
    {
        n += 32;
        x <<= 32;
    }
    if ((x >> 48) == 0u)
    {
        n += 16;
        x <<= 16;
    }
    if ((x >> 56) == 0u)
    {
        n += 8;
        x <<= 8;
    }
    if ((x >> 60) == 0u)
    {
        n += 4;
        x <<= 4;
    }
    if ((x >> 62) == 0u)
    {
        n += 2;
        x <<= 2;
    }
    if ((x >> 63) == 0u)
    {
        n += 1;
    }
    return n;
}

MMGR_INLINE void mmgr_muto_norm(mmgr_muto_fix *f)
{
    if (f->hi == 0u)
    {
        if (f->lo == 0u)
        {
            return;
        }
        f->hi = f->lo;
        f->lo = 0u;
        f->e2 -= 64;
    }

    const int n = mmgr_muto_clz(f->hi);
    if (n != 0)
    {
        f->hi = (f->hi << n) | (f->lo >> (64 - n));
        f->lo <<= n;
        f->e2 -= n;
    }
}

/**
 * @brief @p f times one of the table entries, keeping the top 128 bits.
 * @param f In/out. The fraction.
 * @param g The power of five.
 *
 * The low half of the product is thrown away, but not forgotten: whether it was zero is what
 * decides a tie later, so it goes into rest.
 */
MMGR_INLINE void mmgr_muto_mul_pow5(mmgr_muto_fix *f, const MmgrPow5 *g)
{
    mmgr_u64 hh_h;
    mmgr_u64 hh_l;
    mmgr_u64 hl_h;
    mmgr_u64 hl_l;
    mmgr_u64 lh_h;
    mmgr_u64 lh_l;
    mmgr_u64 ll_h;
    mmgr_u64 ll_l;

    mmgr_muto_mul(f->hi, g->hi, &hh_h, &hh_l);
    mmgr_muto_mul(f->hi, g->lo, &hl_h, &hl_l);
    mmgr_muto_mul(f->lo, g->hi, &lh_h, &lh_l);
    mmgr_muto_mul(f->lo, g->lo, &ll_h, &ll_l);

    /* Four columns, low to high, so every carry lands where it belongs. The bottom two are
       dropped and only their emptiness is kept. */
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
        f->rest = 1;
    }
    f->hi = hh_h + carry2;
    f->lo = col2;
    f->e2 += g->e2 + 128;
    mmgr_muto_norm(f);
}

/**
 * @brief Round the fraction to a double.
 * @param f The fraction.
 * @param neg Whether the value was negative.
 * @return The double.
 *
 * The three places that decide it are always the same three: the fifty three that become the
 * mantissa, the one below them, and whether anything at all is set under that. Clear below means
 * down. Set with something under it means up. Set with nothing under it is the tie, and a tie goes
 * to even. Nothing here looks at what the value was.
 */
MMGR_INLINE double mmgr_muto_round(const mmgr_muto_fix *f, mmgr_bool neg)
{
    /* Ored rather than anded: a high word of nothing with a low word of something is a state the
       normalise cannot leave behind, so asking the two questions separately invents a third answer
       that never happens. One or and one compare says the same thing about the states that do. */
    if ((f->hi | f->lo) == 0u)
    {
        return neg ? -0.0 : 0.0;
    }

    mmgr_u64 mant = f->hi >> 11;
    mmgr_u64 half = (f->hi >> 10) & 1u;
    mmgr_u64 rest = (mmgr_u64)f->rest | ((f->lo != 0u) ? 1u : 0u) | (((f->hi & 0x3FFu) != 0u) ? 1u : 0u);
    int be = f->e2 + 75 + (int)MMGR_DBL_MANT_BITS + MMGR_DBL_BIAS;

    if (be <= 0)
    {
        /* Below the smallest normal the mantissa comes down until the field reads one, and what
           falls off the bottom joins the rest. */
        int shift = 1 - be;
        if (shift > 60)
        {
            return neg ? -0.0 : 0.0;
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
            be = 1; /* rounded up out of the subnormals */
        }
    }

    if (be >= (int)MMGR_DBL_EXP_ALL)
    {
        const double big = 1.0e308 * 10.0;
        return neg ? -big : big;
    }
    return mmgr_fract_from_bits(
        mmgr_fract_merge(neg ? MMGR_DBL_SIGN_ONE : 0u, (mmgr_u64)be, mant & MMGR_DBL_MANT_MASK));
}

/**
 * @brief Assemble @p mant times ten to the @p ex.
 * @param mant Digits, as an integer.
 * @param ex Decimal exponent.
 * @param rest Whether digits were dropped for not fitting.
 * @param neg Whether the value was negative.
 * @return The value, correctly rounded.
 */
/**
 * @brief The powers of ten that are exactly a double.
 *
 * Ten to the twenty two is the last one: above that a power of ten needs more than fifty three
 * bits and the constant is already a rounding of the number it is named after.
 */
#define MMGR_MUTO_EXACT_POW10 22

static const double mmgr_muto_ten[MMGR_MUTO_EXACT_POW10 + 1] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                              1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                              1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

MMGR_INLINE double mmgr_muto_scale(mmgr_u64 mant, int ex, int rest, mmgr_bool neg)
{
    if (mant == 0u)
    {
        return neg ? -0.0 : 0.0;
    }

    /* Both sides exact means the hardware's rounding is the right rounding, so there is nothing
       here to decide and nothing to carry. Every digit had to fit and the power has to be one of
       the twenty three that are exactly a double; that is most of what gets parsed. */
    if ((rest == 0) && (mant < ((mmgr_u64)1 << 53)) && (ex >= -MMGR_MUTO_EXACT_POW10) && (ex <= MMGR_MUTO_EXACT_POW10))
    {
        double v = (double)mant;

        if (ex > 0)
        {
            v *= mmgr_muto_ten[ex];
        }
        else if (ex < 0)
        {
            v /= mmgr_muto_ten[-ex];
        }
        return neg ? -v : v;
    }
    if (ex > MMGR_POW5_MAX)
    {
        const double big = 1.0e308 * 10.0;
        return neg ? -big : big;
    }
    if (ex < -MMGR_POW5_MAX)
    {
        return neg ? -0.0 : 0.0;
    }

    mmgr_muto_fix f;
    f.hi = mant;
    f.lo = 0u;
    f.e2 = -64;
    f.rest = rest;
    mmgr_muto_norm(&f);

    const int k = (ex < 0) ? -ex : ex;
    for (int i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            mmgr_muto_mul_pow5(&f, (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i]);
        }
    }

    f.e2 += ex; /* and the twos, which never needed a multiply */
    return mmgr_muto_round(&f, neg);
}

/**
 * @brief Round the fraction to the nearest whole number.
 * @param f The fraction.
 * @param above Parity of whatever sits above this integer, when it is a field of a longer number.
 * @return The integer, ties to even.
 *
 * The same three places decide it as in mmgr_muto_round: what is above the point, the bit just
 * below it, and whether anything at all is set under that. This one stops at the integer instead
 * of going on to assemble a double.
 *
 * Ties go to even, and even means even in the number that gets written, which is not always this
 * integer. A renderer laying down digits after a point holds this one as a field of ip.10^d + frac,
 * and the parity of that sum is the parity of frac only while there is a frac to speak of - ask for
 * no decimals at all and the tie is decided by ip instead. @p above carries that in: it is the
 * parity of the rest of the number, zero when this integer stands alone.
 *
 * The caller is expected to know the answer fits. The fraction is normalised, so its 128 bits sit
 * at the top and the binary point is -e2 places down: fewer than 64 of those and the whole number
 * needs more than 64 bits to hold, which is a question this cannot answer and does not try to.
 */
MMGR_INLINE mmgr_u64 mmgr_muto_to_u64(const mmgr_muto_fix *f, unsigned above)
{
    if ((f->hi | f->lo) == 0u)
    {
        return 0u;
    }

    const int k = -(f->e2);

    if (k > 128)
    {
        return 0u; /* below half of one, so nothing survives the rounding */
    }
    if (k < 64)
    {
        return ~(mmgr_u64)0; /* would not fit; the caller was supposed to have checked */
    }

    const unsigned j = (unsigned)(k - 64);
    const mmgr_u64 low_mask = (j == 0u) ? 0u : (((mmgr_u64)1 << (j - 1u)) - 1u);
    mmgr_u64 whole;
    mmgr_u64 half;
    mmgr_u64 rest = (f->rest != 0) ? 1u : 0u;

    if (j == 0u)
    {
        whole = f->hi;
        half = (f->lo >> 63) & 1u;
        rest |= ((f->lo & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
    }
    else if (j < 64u)
    {
        whole = f->hi >> j;
        half = (f->hi >> (j - 1u)) & 1u;
        rest |= ((f->hi & low_mask) != 0u) ? 1u : 0u;
        rest |= (f->lo != 0u) ? 1u : 0u;
    }
    else
    {
        whole = 0u; /* k is 128 exactly: everything is below the point */
        half = f->hi >> 63;
        rest |= ((f->hi & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
        rest |= (f->lo != 0u) ? 1u : 0u;
    }

    /* Parity of the truncated number as it will be written, not of this field on its own. */
    const unsigned odd = ((unsigned)(whole & 1u)) ^ (above & 1u);

    if ((half != 0u) && ((rest != 0u) || (odd != 0u)))
    {
        whole += 1u;
    }
    return whole;
}

/**
 * @brief @p mant times two to the @p e2, times ten to the @p ex, rounded to a whole number.
 * @param mant Mantissa.
 * @param e2 Binary exponent that goes with it.
 * @param ex Decimal exponent to apply.
 * @param above Parity of the rest of the number this is a field of; zero when it stands alone.
 * @return The integer, ties to even.
 */
MMGR_INLINE mmgr_u64 mmgr_muto_scale_to_u64(mmgr_u64 mant, int e2, int ex, unsigned above)
{
    if (mant == 0u)
    {
        return 0u;
    }

    mmgr_muto_fix f;
    f.hi = mant;
    f.lo = 0u;
    f.e2 = e2 - 64;
    f.rest = 0;
    mmgr_muto_norm(&f);

    const int k = (ex < 0) ? -ex : ex;
    for (int i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            mmgr_muto_mul_pow5(&f, (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i]);
        }
    }
    f.e2 += ex;
    return mmgr_muto_to_u64(&f, above);
}

MMGR_END_DECLS

#endif
