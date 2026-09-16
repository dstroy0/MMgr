// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_clz_accuracy.c
 * @brief Checks both zero counts against a bit-at-a-time scan, over every single-bit value, every
 *        pair of set bits, the run masks and a long deterministic sweep.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The module halves the search five times and folds the result, or hands the value to a
 *       builtin. The reference walks one bit at a time from the end it counts from, which is the
 *       construction the halving replaced and shares no step with it.
 * @note Both counts are exact integers, so every comparison here is an equality and none of them has
 *       a tolerance.
 * @note Which arm the module compiled is a build detail. The header states both arms agree on every
 *       input, and the cases below are written against the documented count, so whichever arm this
 *       build took is the one being measured.
 * @note The reference reports 64 for a value of zero, which is the true count. The module documents
 *       63 for that input, and the case that covers zero states the documented value directly
 *       instead of bending the reference to it.
 */
#include <stdint.h>
#include <stdio.h>

#include "clz/clz.h"

#include "unity.h"

/**
 * @brief Expands to 64u, the bits in the value both counts are taken over.
 *
 * @note The argument is an embed_u64 whatever the machine word is, so this is 64 in every build
 *       environment and not a width that moves with the target.
 */
#define MMGR_ACCURACY_CLZ_BITS 64u

/**
 * @brief Expands to 63, the count both calls return for a value of zero.
 *
 * @note The header documents this. Zero has no set bit to count from, and the module folds it to the
 *       same count a single bit at the counted end gives.
 */
#define MMGR_ACCURACY_CLZ_ZERO_COUNT 63

/**
 * @brief Returns the zero bits above the highest set bit of a value.
 *
 * @param[in] value Value to measure.
 * @return          Leading zero count, 0 through 64, with 64 for a value of zero.
 * @note One test per bit from the top down, stopping at the first set bit. This is the whole
 *       reference for the leading count.
 * @note A value of zero falls out of the loop having counted all 64, which is the true count and not
 *       what the module returns. The case covering zero handles that difference in the open.
 */
static int accuracy_leading_zeros(uint64_t value)
{
    int zeros = 0;

    for (unsigned position = MMGR_ACCURACY_CLZ_BITS; position-- > 0u;)
    {
        if (((value >> position) & 1u) != 0u)
        {
            break;
        }
        zeros++;
    }
    return zeros;
}

/**
 * @brief Returns the zero bits below the lowest set bit of a value.
 *
 * @param[in] value Value to measure.
 * @return          Trailing zero count, 0 through 64, with 64 for a value of zero.
 * @note One test per bit from the bottom up, stopping at the first set bit. This is the whole
 *       reference for the trailing count.
 * @note A value of zero counts all 64 here, for the reason given on accuracy_leading_zeros.
 */
static int accuracy_trailing_zeros(uint64_t value)
{
    int zeros = 0;

    for (unsigned position = 0u; position < MMGR_ACCURACY_CLZ_BITS; position++)
    {
        if (((value >> position) & 1u) != 0u)
        {
            break;
        }
        zeros++;
    }
    return zeros;
}

/**
 * @brief Returns what clz.lead reported for a value, as an int.
 *
 * @param[in] value Value to measure.
 * @return          The leading count the module produced.
 * @note Flattens the embed_iword to the int the reference returns, which keeps every comparison
 *       below between two counts and not between two containers.
 */
static int accuracy_module_leading(uint64_t value)
{
    // Explicit cast takes the embed_iword the entry returns into the int the reference counts in.
    // Every count is 0 through 63, far inside what an int carries
    return (int)EMBED_CALL(clz.lead, ClzCfg, .val = (embed_u64)value);
}

/**
 * @brief Returns what clz.trail reported for a value, as an int.
 *
 * @param[in] value Value to measure.
 * @return          The trailing count the module produced.
 * @note Flattens the embed_iword to the int the reference returns, as accuracy_module_leading does.
 */
static int accuracy_module_trailing(uint64_t value)
{
    // Explicit cast takes the embed_iword the entry returns into the int the reference counts in
    return (int)EMBED_CALL(clz.trail, ClzCfg, .val = (embed_u64)value);
}

/**
 * @brief Advances a deterministic pattern and returns the next 64-bit value.
 *
 * @param[in,out] state Generator state [BORROWS].
 * @return              The advanced state.
 * @note A fixed recurrence. A failure reproduces on the next run with the same values in the same
 *       order, and nothing here is seeded from the clock.
 * @note The multiplier and increment are the ones Knuth gives for a 64-bit linear congruential
 *       sequence. The values only have to be varied and repeatable, and no statistical property of
 *       them is relied on.
 */
static uint64_t accuracy_next_pattern(uint64_t *state)
{
    *state = (*state * UINT64_C(6364136223846793005)) + UINT64_C(1442695040888963407);
    return *state;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Neither call reads or writes anything outside its argument, and there is no state to
 *       prepare.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Required alongside setUp, since the generated runner calls both around every case.
 * @note Nothing here allocates, so there is nothing to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks the bit scans this suite rests on against counts worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. Without this case a broken scan would
 *       surface as a module mismatch, and the module would be blamed for it.
 * @note The expectations are values a reader can check. 0xFF has 56 zeros above it and none below.
 *       A single bit at the top has none above and 63 below.
 * @note Zero is checked to give 64 from both scans, which is the true count and the one place the
 *       reference and the module are documented to differ.
 */
void test_the_exact_bit_scans_this_suite_relies_on_are_themselves_right(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(63, accuracy_leading_zeros(UINT64_C(1)), "a value of one has 63 zeros above it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_trailing_zeros(UINT64_C(1)), "a value of one has no zeros below it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_leading_zeros(UINT64_C(1) << 63),
                                  "a bit at the top has no zeros above it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(63, accuracy_trailing_zeros(UINT64_C(1) << 63),
                                  "a bit at the top has 63 zeros below it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(56, accuracy_leading_zeros(UINT64_C(0xFF)), "0xFF has 56 zeros above it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_trailing_zeros(UINT64_C(0xFF)), "0xFF has no zeros below it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(55, accuracy_leading_zeros(UINT64_C(0x100)), "0x100 has 55 zeros above it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, accuracy_trailing_zeros(UINT64_C(0x100)), "0x100 has eight zeros below it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(64, accuracy_leading_zeros(0u), "every bit of zero is a leading zero");
    TEST_ASSERT_EQUAL_INT_MESSAGE(64, accuracy_trailing_zeros(0u), "every bit of zero is a trailing zero");
}

/**
 * @brief Checks both counts at every one of the 64 single-bit values.
 *
 * @note One set bit is the case that pins the whole index. A leading count of 63 minus the position
 *       and a trailing count of the position hold for every one of them, and a fold that lost a step
 *       is off by that step's width at half the positions.
 * @note The two counts are checked against each other as well: for a single bit they add to 63, and
 *       an arm that agreed with the reference on one end and not the other still fails that.
 */
void test_both_counts_are_right_at_every_single_bit_value(void)
{
    for (unsigned position = 0u; position < MMGR_ACCURACY_CLZ_BITS; position++)
    {
        const uint64_t value = UINT64_C(1) << position;
        char message[96];

        (void)snprintf(message, sizeof message, "the leading count is wrong for a single bit at %u", position);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_leading_zeros(value), accuracy_module_leading(value), message);

        (void)snprintf(message, sizeof message, "the trailing count is wrong for a single bit at %u", position);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_trailing_zeros(value), accuracy_module_trailing(value), message);

        (void)snprintf(message, sizeof message, "the two counts do not add to 63 for a single bit at %u", position);
        TEST_ASSERT_EQUAL_INT_MESSAGE(63, accuracy_module_leading(value) + accuracy_module_trailing(value), message);
    }
}

/**
 * @brief Checks both counts at every value with exactly two bits set.
 *
 * @note The leading count is fixed by the highest set bit and the trailing count by the lowest, and
 *       a value with two set bits is the smallest one where those are different bits. 64 positions
 *       against 64 gives 4096 values, which covers every ordered pair of ends.
 * @note The bits between the two ends stay clear here on purpose. A fold that read a bit it should
 *       have shifted past reaches the wrong end without anything in the middle to hide behind.
 */
void test_both_counts_are_right_at_every_pair_of_set_bits(void)
{
    for (unsigned high = 0u; high < MMGR_ACCURACY_CLZ_BITS; high++)
    {
        for (unsigned low = 0u; low <= high; low++)
        {
            const uint64_t value = (UINT64_C(1) << high) | (UINT64_C(1) << low);
            char message[96];

            (void)snprintf(message, sizeof message, "the leading count is wrong for bits %u and %u", high, low);
            TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_leading_zeros(value), accuracy_module_leading(value), message);

            (void)snprintf(message, sizeof message, "the trailing count is wrong for bits %u and %u", high, low);
            TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_trailing_zeros(value), accuracy_module_trailing(value), message);
        }
    }
}

/**
 * @brief Checks both counts across the run masks at every length.
 *
 * @note Three families, each 64 values: the low bits set, the high bits set, and one run of ones
 *       sitting in the middle. A run fills the bits a single-bit value leaves clear, which is where
 *       a halving step that tested the wrong half of the remainder shows up.
 * @note The middle runs are the ones that move both ends at once. Every other family here pins one
 *       end at a boundary, and a defect that only moves the end it is not looking at survives them.
 */
void test_both_counts_are_right_across_the_run_masks(void)
{
    for (unsigned length = 1u; length <= MMGR_ACCURACY_CLZ_BITS; length++)
    {
        // A length of 64 would shift a 64-bit value by its full width, which C leaves undefined, so
        // the all ones mask is written out instead of computed
        const uint64_t low_run = (length == MMGR_ACCURACY_CLZ_BITS) ? UINT64_MAX : ((UINT64_C(1) << length) - 1u);
        const uint64_t high_run = low_run << (MMGR_ACCURACY_CLZ_BITS - length);
        const uint64_t middle_run = low_run << ((MMGR_ACCURACY_CLZ_BITS - length) / 2u);
        const uint64_t run_of[] = {low_run, high_run, middle_run};
        // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
        const unsigned run_count = (unsigned)(sizeof run_of / sizeof run_of[0]);

        for (unsigned run_index = 0u; run_index < run_count; run_index++)
        {
            char message[96];

            (void)snprintf(message, sizeof message, "the leading count is wrong for run %u of length %u", run_index,
                           length);
            TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_leading_zeros(run_of[run_index]),
                                          accuracy_module_leading(run_of[run_index]), message);

            (void)snprintf(message, sizeof message, "the trailing count is wrong for run %u of length %u", run_index,
                           length);
            TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_trailing_zeros(run_of[run_index]),
                                          accuracy_module_trailing(run_of[run_index]), message);
        }
    }
}

/**
 * @brief Checks both counts over a long deterministic sweep of arbitrary values.
 *
 * @note The cases above are all built values with structure. This one covers values with none, where
 *       the bits between the two ends are whatever the pattern gave, and 65536 of them is enough to
 *       reach every leading and trailing count many times.
 * @note Every value is masked down through the widths as well, so the sweep reaches small values and
 *       not only ones with a bit near the top. An unmasked 64-bit pattern has a leading count of 0
 *       or 1 almost every time.
 */
void test_both_counts_are_right_over_a_long_sweep_of_arbitrary_values(void)
{
    uint64_t state = UINT64_C(0x0123456789ABCDEF);

    for (unsigned step = 0u; step < 65536u; step++)
    {
        const uint64_t pattern = accuracy_next_pattern(&state);
        const unsigned width = (step % MMGR_ACCURACY_CLZ_BITS) + 1u;
        // A width of 64 would shift a 64-bit value by its full width, which C leaves undefined, so
        // the all ones mask is written out instead of computed
        const uint64_t mask = (width == MMGR_ACCURACY_CLZ_BITS) ? UINT64_MAX : ((UINT64_C(1) << width) - 1u);
        const uint64_t value = pattern & mask;

        if (value == 0u)
        {
            continue;
        }

        char message[128];

        (void)snprintf(message, sizeof message, "the leading count is wrong at step %u", step);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_leading_zeros(value), accuracy_module_leading(value), message);

        (void)snprintf(message, sizeof message, "the trailing count is wrong at step %u", step);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_trailing_zeros(value), accuracy_module_trailing(value), message);
    }
}

/**
 * @brief Checks that a value of zero gives the documented count from both calls.
 *
 * @note Zero has no set bit, and the header documents 63 from both calls, which is the same count a
 *       single bit at the counted end gives. The reference counts 64 for it, and this case states
 *       the documented value directly instead of teaching the reference an exception.
 * @note Pinned here because the two arms reach it differently. One sets a bit the builtin needs and
 *       the other folds to it, and an arm that dropped that step returns whatever the builtin does
 *       with an input it leaves undefined.
 */
void test_a_value_of_zero_gives_the_documented_count_from_both_calls(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_ACCURACY_CLZ_ZERO_COUNT, accuracy_module_leading(0u),
                                  "a leading count of zero did not give the documented count");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_ACCURACY_CLZ_ZERO_COUNT, accuracy_module_trailing(0u),
                                  "a trailing count of zero did not give the documented count");
    TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_module_leading(UINT64_C(1)), accuracy_module_leading(0u),
                                  "zero and one do not give the same leading count");
    TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_module_trailing(UINT64_C(1) << 63), accuracy_module_trailing(0u),
                                  "zero and a bit at the top do not give the same trailing count");
}
