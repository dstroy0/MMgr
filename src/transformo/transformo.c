// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "transformo/transformo.h"

/**
 * @file transformo.c
 * @brief The engine, in one place, on one context.
 *
 * Every entry below takes one parameter, a pointer to MutoCtx, and nothing else. What was asked
 * for, the fraction working on it, and the multiply's operands and answer are one conversion, so
 * they are one struct.
 *
 * They are all MMGR_INLINE and file local, which is what makes the pieces fold into each other -
 * the multiply into the power of five step, that into the scale, the normalise into all of them.
 * What leaves this file is the two entries a caller asks for, and those are real functions, so the
 * engine exists once in the library rather than once in every module that converts a number. The
 * header was inline when one module converted numbers; at two, always_inline put a copy of the
 * whole thing in each.
 */

/** @brief The powers of ten that are exactly a double.
 *
 * Twenty two is the bound because five to the twenty two is 2384185791015625, which is the last
 * power of five that fits in fifty three bits - five to the twenty third does not. Ten to the k is
 * five to the k times two to the k, and the twos are the exponent field, so they cost nothing and
 * change nothing. It is the fives that run out.
 *
 * Worth the 184 bytes: with both operands exact one hardware multiply is correctly rounded by
 * definition, and measured against the 128 bit path on the same hundred thousand values it is 4.72
 * ns against 100.72, agreeing on every one. Building the constant instead of storing it is 37 ns. */
#define MMGR_MUTO_EXACT_POW10 22

static const double mmgr_muto_ten[MMGR_MUTO_EXACT_POW10 + 1] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                                1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                                1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

/**
 * @brief The conversion, in one place.
 *
 * What was asked for, the fraction working on it, and the multiply it works with.
 *
 * The fraction is 128 bits and never grows. An exact decimal expansion would need thousands - a
 * subnormal wants five to the 1074th, which is a 2494 bit integer - but nobody needs the expansion.
 * What is needed is enough bits to decide the rounding: fifty three that become the mantissa, one
 * below them, and one bit saying whether anything at all is set under that. A hundred and twenty
 * eight carries all three with seventy four to spare, because normalising after each step holds the
 * fraction in place and pushes the growth into an int.
 */
typedef struct
{
    /* what was asked for */
    mmgr_u64 mant;  /**< The digits, as an integer. */
    int e2;         /**< Binary exponent that goes with them. */
    int ex;         /**< Decimal exponent to apply. */
    int dropped;    /**< Digits were dropped past what @c mant could hold. */
    unsigned above; /**< Parity of the rest of the number this is a field of. */
    mmgr_bool neg;  /**< The value was negative. */

    /* the fraction working on it */
    mmgr_u64 hi;
    mmgr_u64 lo;
    int fe2;
    int rest; /**< Something was shifted off the bottom, so a tie is not a tie. */

    /* the multiply: two operands in, a 128 bit answer out */
    mmgr_u64 a;
    mmgr_u64 b;
    mmgr_u64 phi;
    mmgr_u64 plo;

    const MmgrPow5 *pow; /**< The table entry being applied. */
} MutoCtx;

/**
 * @brief @c a times @c b, into @c phi and @c plo.
 * @param c In/out. The conversion.
 *
 * Four partial products of the halves, reassembled with the carries written out. The same shape at
 * any width: at sixty four bits the halves are thirty two, at thirty two they are sixteen, and the
 * reassembly does not change.
 */
MMGR_INLINE void muto_mul(MutoCtx *c)
{
    const mmgr_u64 half = (mmgr_u64)0xFFFFFFFFu;
    const mmgr_u64 a0 = c->a & half;
    const mmgr_u64 a1 = c->a >> 32;
    const mmgr_u64 b0 = c->b & half;
    const mmgr_u64 b1 = c->b >> 32;

    const mmgr_u64 p00 = a0 * b0;
    const mmgr_u64 p01 = a0 * b1;
    const mmgr_u64 p10 = a1 * b0;
    const mmgr_u64 p11 = a1 * b1;
    const mmgr_u64 mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    c->plo = (p00 & half) | (mid << 32);
    c->phi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief Bring the fraction's top bit up, moving its exponent to match.
 * @param c In/out. The conversion.
 */
MMGR_INLINE void muto_norm(MutoCtx *c)
{
    if (c->hi == 0u)
    {
        if (c->lo == 0u)
        {
            return;
        }
        c->hi = c->lo;
        c->lo = 0u;
        c->fe2 -= 64;
    }

    const int n = mmgr_muto_clz(c->hi);
    if (n != 0)
    {
        c->hi = (c->hi << n) | (c->lo >> (64 - n));
        c->lo <<= n;
        c->fe2 -= n;
    }
}

/**
 * @brief The fraction times the table entry in @c pow, keeping the top 128 bits.
 * @param c In/out. The conversion.
 *
 * The low half of the product is thrown away, but not forgotten: whether it was zero is what
 * decides a tie later, so it goes into @c rest.
 */
MMGR_INLINE void muto_mul_pow5(MutoCtx *c)
{
    const mmgr_u64 fhi = c->hi;
    const mmgr_u64 flo = c->lo;
    const mmgr_u64 ghi = c->pow->hi;
    const mmgr_u64 glo = c->pow->lo;

    c->a = fhi;
    c->b = ghi;
    muto_mul(c);
    const mmgr_u64 hh_h = c->phi;
    const mmgr_u64 hh_l = c->plo;

    c->a = fhi;
    c->b = glo;
    muto_mul(c);
    const mmgr_u64 hl_h = c->phi;
    const mmgr_u64 hl_l = c->plo;

    c->a = flo;
    c->b = ghi;
    muto_mul(c);
    const mmgr_u64 lh_h = c->phi;
    const mmgr_u64 lh_l = c->plo;

    c->a = flo;
    c->b = glo;
    muto_mul(c);
    const mmgr_u64 ll_h = c->phi;
    const mmgr_u64 ll_l = c->plo;

    /* Four columns, low to high, so every carry lands where it belongs. The bottom two are dropped
       and only their emptiness is kept. */
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
        c->rest = 1;
    }
    c->hi = hh_h + carry2;
    c->lo = col2;
    c->fe2 += c->pow->e2 + 128;
    muto_norm(c);
}

/**
 * @brief Apply the decimal exponent to the fraction.
 * @param c In/out. The conversion.
 *
 * Ten to the ex is five to the ex times two to the ex. The twos never need arithmetic - a power of
 * two is the exponent field, so applying one is an add to it and cannot round. Only the fives need
 * carrying, and any exponent below 512 is the product of at most nine table entries, so a loop that
 * would have run four hundred times runs nine.
 */
MMGR_INLINE void muto_apply_pow10(MutoCtx *c)
{
    const int k = (c->ex < 0) ? -c->ex : c->ex;

    for (int i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            c->pow = (c->ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];
            muto_mul_pow5(c);
        }
    }
    c->fe2 += c->ex; /* and the twos, which never needed a multiply */
}

/**
 * @brief Seat the mantissa in the fraction and normalise it.
 * @param c In/out. The conversion.
 */
MMGR_INLINE void muto_seat(MutoCtx *c)
{
    c->hi = c->mant;
    c->lo = 0u;
    c->fe2 = c->e2 - 64;
    c->rest = c->dropped;
    muto_norm(c);
}

/**
 * @brief Round the fraction to a double.
 * @param c The conversion.
 * @return The double.
 *
 * The three places that decide it are always the same three: the fifty three that become the
 * mantissa, the one below them, and whether anything at all is set under that. Clear below means
 * down. Set with something under it means up. Set with nothing under it is the tie, and a tie goes
 * to even. Nothing here looks at what the value was.
 */
MMGR_INLINE double muto_round(const MutoCtx *c)
{
    /* Ored rather than anded: a high word of nothing with a low word of something is a state the
       normalise cannot leave behind, so asking the two questions separately invents a third answer
       that never happens. One or and one compare says the same thing about the states that do. */
    if ((c->hi | c->lo) == 0u)
    {
        return c->neg ? -0.0 : 0.0;
    }

    mmgr_u64 mant = c->hi >> 11;
    mmgr_u64 half = (c->hi >> 10) & 1u;
    mmgr_u64 rest = (mmgr_u64)c->rest | ((c->lo != 0u) ? 1u : 0u) | (((c->hi & 0x3FFu) != 0u) ? 1u : 0u);
    int be = c->fe2 + 75 + (int)MMGR_DBL_MANT_BITS + MMGR_DBL_BIAS;

    if (be <= 0)
    {
        /* Below the smallest normal the mantissa comes down until the field reads one, and what
           falls off the bottom joins the rest. */
        int shift = 1 - be;
        if (shift > 60)
        {
            return c->neg ? -0.0 : 0.0;
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
        return c->neg ? -big : big;
    }
    return fract.from_bits(
        fract.merge(c->neg ? MMGR_DBL_SIGN_ONE : 0u, (mmgr_u64)be, mant & MMGR_DBL_MANT_MASK));
}

/**
 * @brief Round the fraction to the nearest whole number.
 * @param c The conversion.
 * @return The integer, ties to even.
 *
 * The same three places decide it as in muto_round: what is above the point, the bit just below it,
 * and whether anything at all is set under that. This one stops at the integer instead of going on
 * to assemble a double.
 *
 * Ties go to even, and even means even in the number that gets written, which is not always this
 * integer. A renderer laying down digits after a point holds this one as a field of ip.10^d + frac,
 * and the parity of that sum is the parity of frac only while there is a frac to speak of - ask for
 * no decimals at all and the tie is decided by ip instead. @c above carries that in: it is the
 * parity of the rest of the number, zero when this integer stands alone.
 *
 * The caller is expected to know the answer fits. The fraction is normalised, so its 128 bits sit
 * at the top and the binary point is -fe2 places down: fewer than 64 of those and the whole number
 * needs more than 64 bits to hold, which is a question this cannot answer and does not try to.
 */
MMGR_INLINE mmgr_u64 muto_to_u64(const MutoCtx *c)
{
    if ((c->hi | c->lo) == 0u)
    {
        return 0u;
    }

    const int k = -(c->fe2);

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
    mmgr_u64 rest = (c->rest != 0) ? 1u : 0u;

    if (j == 0u)
    {
        whole = c->hi;
        half = (c->lo >> 63) & 1u;
        rest |= ((c->lo & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
    }
    else if (j < 64u)
    {
        whole = c->hi >> j;
        half = (c->hi >> (j - 1u)) & 1u;
        rest |= ((c->hi & low_mask) != 0u) ? 1u : 0u;
        rest |= (c->lo != 0u) ? 1u : 0u;
    }
    else
    {
        whole = 0u; /* k is 128 exactly: everything is below the point */
        half = c->hi >> 63;
        rest |= ((c->hi & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
        rest |= (c->lo != 0u) ? 1u : 0u;
    }

    /* Parity of the truncated number as it will be written, not of this field on its own. */
    const unsigned odd = ((unsigned)(whole & 1u)) ^ (c->above & 1u);

    if ((half != 0u) && ((rest != 0u) || (odd != 0u)))
    {
        whole += 1u;
    }
    return whole;
}

/**
 * @brief The conversion, to a double.
 * @param c In/out. The conversion.
 * @return The value, correctly rounded.
 */
MMGR_INLINE double muto_scale(MutoCtx *c)
{
    if (c->mant == 0u)
    {
        return c->neg ? -0.0 : 0.0;
    }

    /* Both sides exact means the hardware's rounding is the right rounding, so there is nothing
       here to decide and nothing to carry. Every digit had to fit and the power has to be one of
       the twenty three that are exactly a double; that is most of what gets parsed. */
    if ((c->dropped == 0) && (c->mant < ((mmgr_u64)1 << 53)) && (c->ex >= -MMGR_MUTO_EXACT_POW10) &&
        (c->ex <= MMGR_MUTO_EXACT_POW10))
    {
        double v = (double)c->mant;

        if (c->ex > 0)
        {
            v *= mmgr_muto_ten[c->ex];
        }
        else if (c->ex < 0)
        {
            v /= mmgr_muto_ten[-c->ex];
        }
        return c->neg ? -v : v;
    }
    if (c->ex > MMGR_POW5_MAX)
    {
        const double big = 1.0e308 * 10.0;
        return c->neg ? -big : big;
    }
    if (c->ex < -MMGR_POW5_MAX)
    {
        return c->neg ? -0.0 : 0.0;
    }

    muto_seat(c);
    muto_apply_pow10(c);
    return muto_round(c);
}

/**
 * @brief The conversion, to a whole number.
 * @param c In/out. The conversion.
 * @return The integer, ties to even.
 */
MMGR_INLINE mmgr_u64 muto_scale_to_u64(MutoCtx *c)
{
    if (c->mant == 0u)
    {
        return 0u;
    }

    muto_seat(c);
    muto_apply_pow10(c);
    return muto_to_u64(c);
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each builds the context and hands it to the body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

double mmgr_muto_scale(mmgr_u64 mant, int ex, int rest, mmgr_bool neg)
{
    return MMGR_CALL(muto_scale, MutoCtx, .mant = mant, .ex = ex, .dropped = rest, .neg = neg);
}

mmgr_u64 mmgr_muto_scale_to_u64(mmgr_u64 mant, int e2, int ex, unsigned above)
{
    return MMGR_CALL(muto_scale_to_u64, MutoCtx, .mant = mant, .e2 = e2, .ex = ex, .above = above);
}
