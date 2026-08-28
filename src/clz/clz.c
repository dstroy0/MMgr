/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Branchless count of the leading and trailing zero bits in a 64-bit value.
 */
#include "clz/clz.h"

/**
 * @brief Argument type built by MMGR_CALL in mmgr_clz_lead.
 *
 * @note Mirrors ClzCfg without its const qualifier.
 */
typedef struct
{
    mmgr_u64 val; /**< Value whose leading zeros are counted. */
} ClzCtx;

/**
 * @brief Counts the zero bits above the highest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
 * @return      Leading zero count, 0 through 63.
 * @note Halves the search five times, then tests the top bit, so no step branches on the data.
 * @warning A args->val of 0 returns 63, the same answer as an args->val of 1.
 */
MMGR_INLINE mmgr_iword clz_lead(const ClzCtx *args)
{
#if MMGR_HAS_BUILTIN(__builtin_clzll)
    // Setting the low bit cannot move the highest set one, and it turns the zero the builtin leaves
    // undefined into a one, whose count is the 63 the fold below answers with. So this is the same
    // function for every input, including that one, and it carries no branch to say so.
    // Explicit cast takes the builtin's int into the mmgr_iword the entry returns
    return (mmgr_iword)__builtin_clzll((unsigned long long)(args->val | 1u));
#else
    mmgr_u64 x = args->val;
    mmgr_u64 shift;
    mmgr_iword n = 0;

    // Each step below: the comparison gives 0 or 1, cast to mmgr_u64 so the shift builds 32, 16, 8, 4 or 2
    // Explicit cast converts that step into the signed mmgr_iword total
    shift = (mmgr_u64)((x >> 32) == 0u) << 5;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 48) == 0u) << 4;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 56) == 0u) << 3;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 60) == 0u) << 2;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 62) == 0u) << 1;
    x <<= shift;
    n += (mmgr_iword)shift;
    // Explicit cast keeps the last add in mmgr_iword after the comparison promotes to int
    n = (mmgr_iword)(n + ((x >> 63) == 0u));
    return n;
#endif
}

/**
 * @brief Counts the zero bits below the lowest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
 * @return      Trailing zero count, 0 through 63.
 * @note Isolates the lowest set bit, whose leading zero count is 63 minus its index.
 * @note Or-ing in the top bit gives a zero value a bit to find, so no step branches on the data.
 * @warning A args->val of 0 returns 63, the same answer clz_lead reports for 0.
 */
MMGR_INLINE mmgr_iword clz_trail(const ClzCtx *args)
{
    // Explicit cast builds the top bit at mmgr_u64 width, which stands in for an absent lowest bit
    const mmgr_u64 x = args->val | ((mmgr_u64)1 << 63);

#if MMGR_HAS_BUILTIN(__builtin_ctzll)
    // The top bit set above is what makes this defined for a value of zero, and 63 is the answer the
    // isolate and count below reaches for that input, so the two agree on every input
    // Explicit cast takes the builtin's int into the mmgr_iword the entry returns
    return (mmgr_iword)__builtin_ctzll((unsigned long long)x);
#else
    // Explicit cast keeps the two's complement negation at mmgr_u64, isolating the lowest set bit
    const mmgr_u64 iso = x & (mmgr_u64)(0u - x);

    // Explicit cast keeps the subtraction in mmgr_iword, which is what clz_lead reports in
    return (mmgr_iword)(63 - MMGR_CALL(clz_lead, ClzCtx, .val = iso));
#endif
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_clz_ and clz_ prefixes, which the two share.
 */
#define CLZ_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_clz_, clz_, ClzCtx, ClzCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in clz.h.
 */
CLZ_ENTRY(mmgr_iword, lead, .val = args->val)
CLZ_ENTRY(mmgr_iword, trail, .val = args->val)
