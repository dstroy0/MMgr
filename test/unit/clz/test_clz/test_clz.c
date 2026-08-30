// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
/**
 * @file test_clz.c
 * @brief Exercises clz.lead and clz.trail at every bit position, at both ends of a word, and at
 *        the zero neither entry tells apart from a single set bit.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 */
#include "clz/clz.h"

#include "unity.h"

/**
 * @brief Prepares the fixture Unity runs before each case in this suite.
 *
 * @note Empty because clz.lead and clz.trail declare no static storage and read only args->val, so
 *       nothing carries from one case into the next.
 */
void setUp(void)
{
}

/**
 * @brief Releases the fixture Unity runs after each case in this suite.
 *
 * @note Empty because every case here holds its operands in automatic storage, so no case leaves an
 *       allocation to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks clz.lead against the known count at all 64 single-bit positions.
 *
 * @note Each position is measured twice. The second value sets every bit below the highest one, so
 *       an implementation that reads anything but the highest set bit fails here.
 */
void test_the_leading_zero_count_at_every_position(void)
{
    for (unsigned bit = 0; bit < 64u; bit++)
    {
        const mmgr_u64 single = (mmgr_u64)1 << bit;

        // Explicit cast takes the unsigned loop counter into the int the subtraction and Unity's
        // integer assertion both work in, and bit is bounded to 63 so the value survives it
        TEST_ASSERT_EQUAL_INT_MESSAGE(63 - (int)bit, MMGR_CALL(clz.lead, ClzCfg, .val = single),
                                      "wrong count for a single set bit");

        const mmgr_u64 noisy = single | (single - 1u);

        // Explicit cast takes the unsigned loop counter into the int the subtraction and Unity's
        // integer assertion both work in, and bit is bounded to 63 so the value survives it
        TEST_ASSERT_EQUAL_INT(63 - (int)bit, MMGR_CALL(clz.lead, ClzCfg, .val = noisy));
    }
}

/**
 * @brief Checks clz.trail against the known count at all 64 single-bit positions.
 *
 * @note Each position is measured twice. The second value sets every bit above the lowest one, so
 *       an implementation that reads anything but the lowest set bit fails here.
 */
void test_the_trailing_zero_count_at_every_position(void)
{
    for (unsigned bit = 0; bit < 64u; bit++)
    {
        const mmgr_u64 single = (mmgr_u64)1 << bit;

        // Explicit cast takes the unsigned loop counter into the int Unity's integer assertion
        // compares in, and bit is bounded to 63 so the value survives it
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)bit, MMGR_CALL(clz.trail, ClzCfg, .val = single),
                                      "wrong count for a single set bit");

        const mmgr_u64 noisy = ~(mmgr_u64)0 << bit;

        // Explicit cast takes the unsigned loop counter into the int Unity's integer assertion
        // compares in, and bit is bounded to 63 so the value survives it
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)bit, MMGR_CALL(clz.trail, ClzCfg, .val = noisy),
                                      "only the lowest set bit decides a trailing count");
    }
}

/**
 * @brief Checks that a bit set at position 0 does not move what clz.lead reports.
 *
 * @note The loop starts at position 1. At position 0 the added low bit is itself the highest set
 *       bit, which repeats what test_the_leading_zero_count_at_every_position already measures.
 */
void test_the_leading_count_reads_the_highest_set_bit_alone(void)
{
    for (unsigned bit = 1; bit < 64u; bit++)
    {
        const mmgr_u64 highest = (mmgr_u64)1 << bit;

        // Explicit cast takes the unsigned loop counter into the int the subtraction and Unity's
        // integer assertion both work in, and bit is bounded to 63 so the value survives it
        TEST_ASSERT_EQUAL_INT_MESSAGE(63 - (int)bit, MMGR_CALL(clz.lead, ClzCfg, .val = highest | 1u),
                                      "a bit set at the bottom must not move the leading count");
    }
}

/**
 * @brief Checks that a bit set at position 63 does not move what clz.trail reports.
 *
 * @note The loop stops before position 63. There the added high bit is itself the lowest set bit,
 *       which repeats what test_the_trailing_zero_count_at_every_position already measures.
 */
void test_the_trailing_count_reads_the_lowest_set_bit_alone(void)
{
    for (unsigned bit = 0; bit < 63u; bit++)
    {
        const mmgr_u64 lowest = (mmgr_u64)1 << bit;
        const mmgr_u64 top = (mmgr_u64)1 << 63;

        // Explicit cast takes the unsigned loop counter into the int Unity's integer assertion
        // compares in, and bit is bounded to 62 here so the value survives it
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)bit, MMGR_CALL(clz.trail, ClzCfg, .val = lowest | top),
                                      "a bit set at the top must not move the trailing count");
    }
}

/**
 * @brief Checks that the two counts of one set bit sum to 63 at every position.
 *
 * @note A word holds 64 bits, so one set bit leaves 63 zeros divided between the two ends. A count
 *       that is off by one at either end fails this sum.
 */
void test_the_two_counts_bracket_a_single_set_bit(void)
{
    for (unsigned bit = 0; bit < 64u; bit++)
    {
        const mmgr_u64 single = (mmgr_u64)1 << bit;
        const mmgr_iword lead = MMGR_CALL(clz.lead, ClzCfg, .val = single);
        const mmgr_iword trail = MMGR_CALL(clz.trail, ClzCfg, .val = single);

        // Explicit casts take both counts from mmgr_iword into the int Unity's integer assertion
        // compares in. clz.h documents each count as 0 through 63, so the sum stays at 63 and fits
        // whichever of the two types is narrower
        TEST_ASSERT_EQUAL_INT_MESSAGE(63, (int)lead + (int)trail,
                                      "one set bit leaves 63 zeros split between the two ends");
    }
}

/**
 * @brief Checks that a word with every bit set counts zero at both ends.
 *
 * @note Both ends answer 0 here because bit 0 and bit 63 are both set. An implementation that
 *       returned a width rather than a count would pass the single-bit cases above and fail here.
 */
void test_a_full_word_counts_no_zeros_at_either_end(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, MMGR_CALL(clz.lead, ClzCfg, .val = ~(mmgr_u64)0),
                                  "a word with every bit set has no leading zeros");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, MMGR_CALL(clz.trail, ClzCfg, .val = ~(mmgr_u64)0),
                                  "a word with every bit set has no trailing zeros");
}

/**
 * @brief Checks that both counts answer 63 for a value of zero.
 *
 * @note clz.h warns that clz.lead gives 0 the same answer as 1, and that clz.trail gives 0 the same
 *       answer as 2^63. This pins both warnings so a caller that can be handed zero knows it has to
 *       test for zero before calling.
 */
void test_zero_is_not_told_apart_from_the_single_bit_at_the_end_each_counts_from(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(63, MMGR_CALL(clz.lead, ClzCfg, .val = 0u),
                                  "the header warns that zero and one give the same leading count");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_CALL(clz.lead, ClzCfg, .val = 1u), MMGR_CALL(clz.lead, ClzCfg, .val = 0u),
                                  "a caller that can be handed zero has to test for it first");

    TEST_ASSERT_EQUAL_INT_MESSAGE(63, MMGR_CALL(clz.trail, ClzCfg, .val = 0u),
                                  "the header warns that zero and the top bit give the same trailing count");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_CALL(clz.trail, ClzCfg, .val = (mmgr_u64)1 << 63),
                                  MMGR_CALL(clz.trail, ClzCfg, .val = 0u),
                                  "a caller that can be handed zero has to test for it first");
}

/**
 * @brief Checks that both counts stay within 0 through 63 across a widening run of set bits.
 *
 * @note The loop shifts a set bit in on each step, so it measures the 64 values of the form 2^n
 *       minus 1. A count outside the documented range would index past a lane table at any caller
 *       that used it as an index.
 */
void test_both_counts_stay_inside_the_documented_range(void)
{
    mmgr_u64 walk = 1u;

    for (int step = 0; step < 64; step++)
    {
        const mmgr_iword lead = MMGR_CALL(clz.lead, ClzCfg, .val = walk);
        const mmgr_iword trail = MMGR_CALL(clz.trail, ClzCfg, .val = walk);

        // Explicit casts take the count from mmgr_iword into int so both halves of the range test
        // compare in one type. The two halves are combined because a count is in range only when
        // both hold, and neither half carries a side effect
        TEST_ASSERT_TRUE_MESSAGE((int)lead >= 0 && (int)lead <= 63, "a leading count outside 0 through 63");
        // Explicit casts take the count from mmgr_iword into int so both halves of the range test
        // compare in one type. The two halves are combined because a count is in range only when
        // both hold, and neither half carries a side effect
        TEST_ASSERT_TRUE_MESSAGE((int)trail >= 0 && (int)trail <= 63, "a trailing count outside 0 through 63");
        walk = (walk << 1) | 1u;
    }
}
