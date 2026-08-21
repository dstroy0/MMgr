// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "fractio/fractio.h"
#include "config/mmgr_config.h"
#include "pow5/pow5.h"

MMGR_INCIPE_DECLS

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

/**
 * @brief @p mant times ten to the @p ex, as the double it names.
 * @param mant Mantissa.
 * @param ex Decimal exponent.
 * @param rest Whether digits were dropped past what the mantissa could hold.
 * @param neg Whether the value was negative.
 * @return The double, correctly rounded.
 */
double mmgr_muto_scale(mmgr_u64 mant, int ex, int rest, mmgr_bool neg);

/**
 * @brief @p mant times two to the @p e2, times ten to the @p ex, rounded to a whole number.
 * @param mant Mantissa.
 * @param e2 Binary exponent that goes with it.
 * @param ex Decimal exponent to apply.
 * @param above Parity of the rest of the number this is a field of; zero when it stands alone.
 * @return The integer, ties to even.
 */
mmgr_u64 mmgr_muto_scale_to_u64(mmgr_u64 mant, int e2, int ex, unsigned above);

MMGR_FINIS_DECLS

#endif
