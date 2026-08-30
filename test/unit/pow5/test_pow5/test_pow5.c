// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
/**
 * @file test_pow5.c
 * @brief Exercises the shape of the two pow5 tables, covering entry count, normalization, binary
 *        exponent ordering, and which entries pow5.h documents as exact.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note What each entry numerically is belongs to test_pow5_accuracy, which derives every value
 *       from exact arithmetic. This suite reads the tables for the structure the header states.
 */
#include "pow5/pow5.h"

#include "unity.h"

/**
 * @brief Expands to 5, the number of mmgr_pow5_up entries whose significand fits the high half alone.
 *
 * @note Lower than the six entries pow5.h documents as exact. Entry 5 is 5^32, which is exact at 75
 *       bits and still needs its low half, so it sits outside this count.
 */
#define MMGR_POW5_SINGLE_HALF_STEPS 5

/**
 * @brief Prepares the fixture Unity runs before each case in this suite.
 *
 * @note Empty because pow5.h declares no function and every case reads the two static const tables,
 *       which no case writes to.
 */
void setUp(void)
{
}

/**
 * @brief Releases the fixture Unity runs after each case in this suite.
 *
 * @note Empty because the tables are static const and outlive every call, so no case allocates and
 *       none leaves anything to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks that both tables hold one entry per bit of the exponent magnitude.
 *
 * @note MMGR_POW5_STEPS sizes both tables and bounds the walk that multiplies in one entry per set
 *       bit. A length disagreeing with the constant would read past a table's end.
 */
void test_both_tables_hold_one_entry_per_exponent_bit(void)
{
    TEST_ASSERT_EQUAL_INT(9, MMGR_POW5_STEPS);
    TEST_ASSERT_EQUAL_INT(511, MMGR_POW5_MAX);
    // Explicit cast takes MMGR_POW5_STEPS from the plain int it expands to into the size_t a sizeof
    // quotient has, and 9 is positive so the conversion to unsigned keeps its value
    TEST_ASSERT_EQUAL_size_t((size_t)MMGR_POW5_STEPS, sizeof mmgr_pow5_up / sizeof mmgr_pow5_up[0]);
    // Explicit cast takes MMGR_POW5_STEPS from the plain int it expands to into the size_t a sizeof
    // quotient has, and 9 is positive so the conversion to unsigned keeps its value
    TEST_ASSERT_EQUAL_size_t((size_t)MMGR_POW5_STEPS, sizeof mmgr_pow5_down / sizeof mmgr_pow5_down[0]);
}

/**
 * @brief Checks that the top bit of hi is set in every entry of both tables.
 *
 * @note pow5.h documents hi as always carrying its top bit set. A denormalized entry would still
 *       scale by its own e2 and multiply in at the wrong magnitude.
 */
void test_every_significand_is_normalized(void)
{
    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        // Explicit cast widens the literal to mmgr_u64 before the shift. Shifting a plain int by 63
        // is undefined where int is 32 bits, so the cast is what makes the mask well defined
        TEST_ASSERT_TRUE_MESSAGE((mmgr_pow5_up[step].hi & (mmgr_u64)1 << 63) != 0u,
                                 "an up entry whose top bit is clear is not normalized");
        // Explicit cast widens the literal to mmgr_u64 before the shift. Shifting a plain int by 63
        // is undefined where int is 32 bits, so the cast is what makes the mask well defined
        TEST_ASSERT_TRUE_MESSAGE((mmgr_pow5_down[step].hi & (mmgr_u64)1 << 63) != 0u,
                                 "a down entry whose top bit is clear is not normalized");
    }
}

/**
 * @brief Checks that each single-half entry reconstructs to five raised to two to its index.
 *
 * @note Squaring the running value each step gives 5, 25, 625, 390625 and 152587890625, the five
 *       powers whose significand fits mmgr_u64. The shift is derived from e2, so a wrong e2 fails
 *       this comparison even where hi holds the right digits.
 */
void test_the_exact_powers_reconstruct_to_five_raised_to_two_to_the_step(void)
{
    mmgr_u64 exact_value = 5u;

    for (int step = 0; step < MMGR_POW5_SINGLE_HALF_STEPS; step++)
    {
        // Explicit cast takes e2 from mmgr_iword into int for the negation and the addition. pow5.h
        // bounds e2 at -722, which fits int on every environment this suite builds for
        const int shift = -(64 + (int)mmgr_pow5_up[step].e2);

        // The two halves are combined because a shift is usable only when both hold, and neither
        // half carries a side effect. A shift outside this range would be undefined at the line below
        TEST_ASSERT_TRUE_MESSAGE(shift >= 0 && shift < 64, "the exact entries scale by a shift inside a word");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(exact_value << shift, mmgr_pow5_up[step].hi,
                                        "the significand is not five raised to two to the step");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, mmgr_pow5_up[step].lo, "an exact power of five needs no low half");
        exact_value = exact_value * exact_value;
    }
}

/**
 * @brief Checks that every entry at or past MMGR_POW5_SINGLE_HALF_STEPS carries a nonzero low half.
 *
 * @note 5^32 at index 5 needs 75 bits, and the three entries above it need more than 128. All four
 *       store digits below the high half.
 * @note An entry that lost its low half would still be normalized and would multiply in short by up
 *       to a full word.
 */
void test_the_wide_powers_carry_a_low_half(void)
{
    for (int step = MMGR_POW5_SINGLE_HALF_STEPS; step < MMGR_POW5_STEPS; step++)
    {
        TEST_ASSERT_NOT_EQUAL_HEX64_MESSAGE(0ULL, mmgr_pow5_up[step].lo,
                                            "a power of five wider than 64 bits carries digits in its low half");
    }
}

/**
 * @brief Checks that mmgr_pow5_down holds each negative power of five truncated toward zero.
 *
 * @note 5^-1 repeats in binary, so both halves of entry 0 are 0xCCCCCCCCCCCCCCCC. Rounding to
 *       nearest would end the low half in D.
 * @note No negative power of five terminates in binary, so every entry in the table carries a
 *       nonzero low half.
 */
void test_the_negative_powers_truncate_toward_zero(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xCCCCCCCCCCCCCCCCULL, mmgr_pow5_down[0].hi,
                                    "five to the minus one repeats, so rounding to nearest is the wrong table");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xCCCCCCCCCCCCCCCCULL, mmgr_pow5_down[0].lo,
                                    "a low half ending in D would mean the table rounded to nearest");

    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        TEST_ASSERT_NOT_EQUAL_HEX64_MESSAGE(0ULL, mmgr_pow5_down[step].lo,
                                            "no negative power of five terminates in binary, so no low half is zero");
    }
}

/**
 * @brief Checks that the binary exponent runs monotonically through both tables.
 *
 * @note e2 rises from -125 to 467 across mmgr_pow5_up and falls from -130 to -722 across
 *       mmgr_pow5_down. A repeated or reversed step would mean an entry was normalized against the
 *       wrong power.
 */
void test_the_binary_exponents_run_monotonically_in_both_tables(void)
{
    for (int step = 1; step < MMGR_POW5_STEPS; step++)
    {
        // Explicit casts take both exponents from mmgr_iword into the int Unity's integer comparison
        // works in. pow5.h bounds e2 between -722 and 467, which fits int on every environment this
        // suite builds for
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE((int)mmgr_pow5_up[step - 1].e2, (int)mmgr_pow5_up[step].e2,
                                             "a larger power of five needs a larger binary exponent");
        // Explicit casts take both exponents from mmgr_iword into the int Unity's integer comparison
        // works in. pow5.h bounds e2 between -722 and 467, which fits int on every environment this
        // suite builds for
        TEST_ASSERT_LESS_THAN_INT_MESSAGE((int)mmgr_pow5_down[step - 1].e2, (int)mmgr_pow5_down[step].e2,
                                          "a smaller power of five needs a smaller binary exponent");
    }
}

/**
 * @brief Checks that the narrowest legal mmgr_iword carries the widest exponent in the tables.
 *
 * @note mmgr_types.h raises #error unless MMGR_WORD_BITS is 16, 32 or 64, so the narrowest
 *       mmgr_iword is mmgr_i16. The word16 environment is where this check binds.
 * @note -722 sits at index 8 of mmgr_pow5_down and is the furthest any e2 reaches from zero.
 */
void test_the_widest_exponent_fits_the_narrowest_word(void)
{
    const mmgr_iword widest_exponent = mmgr_pow5_down[MMGR_POW5_STEPS - 1].e2;

    // Explicit cast takes the exponent from mmgr_iword into the int Unity's integer assertion
    // compares in. -722 fits mmgr_i16, the narrowest mmgr_iword, so it fits int everywhere
    TEST_ASSERT_EQUAL_INT_MESSAGE(-722, (int)widest_exponent,
                                  "the widest binary exponent is the one the header documents");
    // Explicit casts round -722 through mmgr_iword and back into int. A word that dropped the value
    // would return something other than -722 here
    TEST_ASSERT_EQUAL_INT_MESSAGE(-722, (int)(mmgr_iword)-722,
                                  "mmgr_i16 is the narrowest mmgr_iword and it carries -722");
}

/**
 * @brief Checks that walking one bit per step reaches the documented maximum exponent.
 *
 * @note MMGR_POW5_STEPS bits set from index 0 gives 511, which is MMGR_POW5_MAX. A tenth entry or a
 *       short walk would move that total and leave part of the exponent range unreachable.
 * @note pow5.h states that a double's decimal exponent stays well inside 511, so nine steps cover
 *       every value one can hold.
 */
void test_the_nine_steps_cover_every_exponent_a_double_carries(void)
{
    int largest_reachable_exponent = 0;

    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        largest_reachable_exponent |= 1 << step;
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_POW5_MAX, largest_reachable_exponent,
                                  "walking every bit of the exponent must reach the documented maximum");
}
