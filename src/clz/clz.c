/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file clz.c
 * @brief Branchless count of the leading and trailing zero bits in a 64-bit value.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note Each call has two arms. Where the compiler offers the builtin it is taken, and where it
 *       does not the count is written out. Both arms answer the same for every input including
 *       zero, which is what lets the choice be a build detail rather than a behavior difference.
 * @note Reaches nothing outside config.
 */
#include "clz/clz.h"

/**
 * @brief Argument type built by MMGR_CALL in the two entry points, and again in the clz_trail arm
 *        without the builtin.
 *
 * @note Mirrors ClzCfg without its const qualifier.
 */
typedef struct
{
    mmgr_u64 val; /**< Value whose leading or trailing zeros are counted. */
} ClzCtx;

/**
 * @brief Counts the zero bits above the highest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
<<<<<<< HEAD
 * @return      Leading zero count, 0 through 63.
 * @note Halves the search five times, then tests the top bit, so no step branches on the data.
 * @warning A args->val of 0 returns 63, the same answer as an args->val of 1.
 */
MMGR_INLINE mmgr_iword clz_lead(const ClzCtx *args)
{
    mmgr_u64 x = args->val;
=======
 * @return         Leading zero count, 0 through 63.
 * @note Runs in a fixed number of steps, none of which branches on the value. The arm without the
 *       builtin halves the search five times, then tests the top bit.
 * @warning An args->val of 0 returns 63, the same answer as an args->val of 1.
 */
MMGR_INLINE mmgr_iword clz_lead(const ClzCtx *args)
{
#if MMGR_HAS_BUILTIN(__builtin_clzll)
    // Setting the low bit cannot move the highest set one, and it turns the zero the builtin leaves
    // undefined into a one, whose count is the 63 the fold below answers with. So this is the same
    // function for every input, including that one, and it carries no branch to say so.
    // Explicit casts put the value in the unsigned long long the ll builtin counts, then take its int
    // result into the mmgr_iword the entry returns
    return (mmgr_iword)__builtin_clzll((unsigned long long)(args->val | 1u));
#else
    mmgr_u64 remaining = args->val;
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
    mmgr_u64 shift;
    mmgr_iword zeros = 0;

    // Each step: the comparison gives 0 or 1, cast to mmgr_u64 so the shift builds 32, 16, 8, 4 or 2.
    // A step that finds the top half empty shifts it away and adds that half's width to the count,
    // so five halvings narrow the search to one bit. Explicit cast converts each step into the
    // signed mmgr_iword total
    shift = (mmgr_u64)((remaining >> 32) == 0u) << 5;
    remaining <<= shift;
    zeros += (mmgr_iword)shift;
    shift = (mmgr_u64)((remaining >> 48) == 0u) << 4;
    remaining <<= shift;
    zeros += (mmgr_iword)shift;
    shift = (mmgr_u64)((remaining >> 56) == 0u) << 3;
    remaining <<= shift;
    zeros += (mmgr_iword)shift;
    shift = (mmgr_u64)((remaining >> 60) == 0u) << 2;
    remaining <<= shift;
    zeros += (mmgr_iword)shift;
    shift = (mmgr_u64)((remaining >> 62) == 0u) << 1;
    remaining <<= shift;
    zeros += (mmgr_iword)shift;
    // Explicit cast keeps the last add in mmgr_iword after the comparison promotes to int
    zeros = (mmgr_iword)(zeros + ((remaining >> 63) == 0u));
    return zeros;
#endif
}

/**
 * @brief Counts the zero bits below the lowest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
<<<<<<< HEAD
 * @return      Trailing zero count, 0 through 63.
 * @note Isolates the lowest set bit, whose leading zero count is 63 minus its index.
 * @note Or-ing in the top bit gives a zero value a bit to find, so no step branches on the data.
 * @warning A args->val of 0 returns 63, the same answer clz_lead reports for 0.
=======
 * @return         Trailing zero count, 0 through 63.
 * @note The arm without the builtin isolates the lowest set bit, whose leading zero count is 63 minus
 *       its index.
 * @note Or-ing in the top bit gives a zero value a bit to find, so no step branches on the data.
 * @warning An args->val of 0 returns 63, the same answer as an args->val of 2^63.
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
 */
MMGR_INLINE mmgr_iword clz_trail(const ClzCtx *args)
{
    // Explicit cast builds the top bit at mmgr_u64 width, which stands in for an absent lowest bit
<<<<<<< HEAD
    const mmgr_u64 x = args->val | ((mmgr_u64)1 << 63);
=======
    const mmgr_u64 with_floor = args->val | ((mmgr_u64)1 << 63);

#if MMGR_HAS_BUILTIN(__builtin_ctzll)
    // The top bit set above is what makes this defined for a value of zero, and 63 is the answer the
    // isolate and count below reaches for that input, so the two agree on every input
    // Explicit casts put the value in the unsigned long long the ll builtin counts, then take its int
    // result into the mmgr_iword the entry returns
    return (mmgr_iword)__builtin_ctzll((unsigned long long)with_floor);
#else
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
    // Explicit cast keeps the two's complement negation at mmgr_u64, isolating the lowest set bit
    const mmgr_u64 lowest_bit = with_floor & (mmgr_u64)(0u - with_floor);

    // Explicit cast keeps the subtraction in mmgr_iword, which is what clz_lead reports in
    return (mmgr_iword)(63 - MMGR_CALL(clz_lead, ClzCtx, .val = lowest_bit));
#endif
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ReturnType_ Return type of the entry point.
 * @param[in] name_       Name after the mmgr_clz_ and clz_ prefixes, which the two share.
 * @param[in] ...         Initializers for the ClzCtx literal, written in terms of args.
 * @note Four of GENERIC_ENTRY's six arguments are the same at both entries here, so they are bound
 *       once and each entry below states only what differs.
 */
#define CLZ_ENTRY(ReturnType_, name_, ...)                                                                             \
    GENERIC_ENTRY(mmgr_clz_, clz_, ClzCtx, ClzCfg, ReturnType_, name_, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in clz.h.
 */
CLZ_ENTRY(mmgr_iword, lead, .val = args->val)
CLZ_ENTRY(mmgr_iword, trail, .val = args->val)
