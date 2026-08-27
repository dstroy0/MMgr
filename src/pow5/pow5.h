/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Powers of five as 128-bit significands, for scaling a decimal mantissa into a binary one.
 *
 * @note transformo multiplies in one entry per set bit of the decimal exponent, so nine entries reach 511.
 * @note Declares no function; both tables are static data.
 */
#ifndef MMGR_POW5_H
#define MMGR_POW5_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Expands to 9, the number of entries in each table.
 *
 * @note Entry i holds five raised to two to the i, so the nine entries run from 5^1 to 5^256.
 */
#define MMGR_POW5_STEPS 9

/**
 * @brief Expands to ((1 << 9) - 1), which is 511, the largest decimal exponent the tables reach.
 *
 * @note Every one of the nine entries multiplied together gives 5^511.
 * @note muto_scale returns infinity above this and zero below its negative, so it never indexes past the tables.
 */
#define MMGR_POW5_MAX ((1 << MMGR_POW5_STEPS) - 1)

/**
 * @brief One power of five, as a 128-bit significand with a binary exponent.
 *
 * @note The value is (hi * 2^64 + lo) * 2^e2, and hi always has its top bit set.
 */
typedef struct
{
    mmgr_u64 hi;   /**< High 64 bits of the significand, with its top bit set. */
    mmgr_u64 lo;   /**< Low 64 bits of the significand. */
    mmgr_iword e2; /**< Binary exponent the significand is scaled by. */
} MmgrPow5;

/**
 * @brief Five raised to each power of two, from 5^1 at index 0 to 5^256 at index 8.
 *
 * @note muto_apply_pow10 multiplies in entry i when bit i of a positive decimal exponent is set.
 */
static const MmgrPow5 mmgr_pow5_up[MMGR_POW5_STEPS] MMGR_UNUSED = {
    {0xA000000000000000ULL, 0x0000000000000000ULL, -125}, {0xC800000000000000ULL, 0x0000000000000000ULL, -123},
    {0x9C40000000000000ULL, 0x0000000000000000ULL, -118}, {0xBEBC200000000000ULL, 0x0000000000000000ULL, -109},
    {0x8E1BC9BF04000000ULL, 0x0000000000000000ULL, -90},  {0x9DC5ADA82B70B59DULL, 0xF020000000000000ULL, -53},
    {0xC2781F49FFCFA6D5ULL, 0x3CBF6B71C76B25FBULL, 21},   {0x93BA47C980E98CDFULL, 0xC66F336C36B10137ULL, 170},
    {0xAA7EEBFB9DF9DE8DULL, 0xDDBB901B98FEEAB7ULL, 467},
};

/**
 * @brief The reciprocal of each mmgr_pow5_up entry, from 5^-1 at index 0 to 5^-256 at index 8.
 *
 * @note muto_apply_pow10 multiplies in entry i when bit i of a negative decimal exponent is set.
 * @note Each entry is rounded, since a negative power of five does not end in binary.
 */
static const MmgrPow5 mmgr_pow5_down[MMGR_POW5_STEPS] MMGR_UNUSED = {
    {0xCCCCCCCCCCCCCCCCULL, 0xCCCCCCCCCCCCCCCCULL, -130}, {0xA3D70A3D70A3D70AULL, 0x3D70A3D70A3D70A3ULL, -132},
    {0xD1B71758E219652BULL, 0xD3C36113404EA4A8ULL, -137}, {0xABCC77118461CEFCULL, 0xFDC20D2B36BA7C3DULL, -146},
    {0xE69594BEC44DE15BULL, 0x4C2EBE687989A9B3ULL, -165}, {0xCFB11EAD453994BAULL, 0x67DE18EDA5814AF2ULL, -202},
    {0xA87FEA27A539E9A5ULL, 0x3F2398D747B36224ULL, -276}, {0xDDD0467C64BCE4A0ULL, 0xAC7CB3F6D05DDBDEULL, -425},
    {0xC0314325637A1939ULL, 0xFA911155FEFB5308ULL, -722},
};

/**
 * @brief Asserts MMGR_POW5_MAX is at least 511.
 *
 * @note A double's decimal exponent stays well inside 511, so nine steps cover every value one can hold.
 */
MMGR_STATIC_ASSERT(MMGR_POW5_MAX >= 511, "the tables do not reach the exponents a double can carry");

MMGR_FINIS_DECLS

#endif
