// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
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
 * @brief Expands to the number of entries mmgr_pow5_up actually holds.
 *
 * @note Sized from the array rather than from MMGR_POW5_STEPS, so a table that gained or lost an
 *       entry is walked in full without an edit here. Taking the count from the library would let
 *       the library decide how much of itself this suite reads.
 * @note The cast takes the sizeof quotient from size_t into the int a loop counter carries. The
 *       assertion below holds the count at 9, which is positive and far inside int.
 */
#define MMGR_POW5_UP_ENTRIES ((int)(sizeof mmgr_pow5_up / sizeof mmgr_pow5_up[0]))

/**
 * @brief Expands to the number of entries mmgr_pow5_down actually holds.
 *
 * @note Sized from its own array rather than shared with MMGR_POW5_UP_ENTRIES, so a change to one
 *       table cannot silently set the bound for walks over the other.
 */
#define MMGR_POW5_DOWN_ENTRIES ((int)(sizeof mmgr_pow5_down / sizeof mmgr_pow5_down[0]))

/**
 * @brief Asserts mmgr_pow5_up holds one entry per bit of the exponent magnitude.
 *
 * @note MMGR_POW5_STEPS bounds the walk that multiplies in one entry per set bit. A table shorter
 *       than the constant lets that walk read past its end.
 */
EMBED_STATIC_ASSERT(MMGR_POW5_UP_ENTRIES == MMGR_POW5_STEPS, "mmgr_pow5_up is not MMGR_POW5_STEPS entries long");

/**
 * @brief Asserts mmgr_pow5_down holds one reciprocal per mmgr_pow5_up entry.
 *
 * @note The walk picks one table or the other by the sign of the exponent and indexes both the same
 *       way, so a length that differs between them reads past the end of the shorter one.
 */
EMBED_STATIC_ASSERT(MMGR_POW5_DOWN_ENTRIES == MMGR_POW5_UP_ENTRIES,
                    "mmgr_pow5_down does not hold one reciprocal per mmgr_pow5_up entry");

/**
 * @brief Asserts the tables still hold the nine entries this suite was written against.
 *
 * @note Nine is checked against a literal rather than against MMGR_POW5_STEPS, which would compare
 *       the constant with itself. The counts above are what tie the arrays to the constant.
 */
EMBED_STATIC_ASSERT(MMGR_POW5_UP_ENTRIES == 9, "the pow5 tables changed length; this suite expects nine entries");

/**
 * @brief Asserts MMGR_POW5_MAX expands to 511.
 *
 * @note pow5.h asserts the same constant is at least 511. This pins the exact value, so a change to
 *       the expansion that still cleared that floor is caught here.
 */
EMBED_STATIC_ASSERT(MMGR_POW5_MAX == 511, "MMGR_POW5_MAX no longer expands to 511");

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
 * @brief Checks that the top bit of hi is set in every entry of both tables.
 *
 * @note pow5.h documents hi as always carrying its top bit set. A denormalized entry would still
 *       scale by its own e2 and multiply in at the wrong magnitude.
 */
void test_every_significand_is_normalized(void)
{
    for (int step = 0; step < MMGR_POW5_UP_ENTRIES; step++)
    {
        // Explicit cast widens the literal to embed_u64 before the shift. Shifting a plain int by 63
        // is undefined where int is 32 bits, so the cast is what makes the mask well defined
        TEST_ASSERT_TRUE_MESSAGE((mmgr_pow5_up[step].hi & (embed_u64)1 << 63) != 0u,
                                 "an up entry whose top bit is clear is not normalized");
        // Explicit cast widens the literal to embed_u64 before the shift. Shifting a plain int by 63
        // is undefined where int is 32 bits, so the cast is what makes the mask well defined
        TEST_ASSERT_TRUE_MESSAGE((mmgr_pow5_down[step].hi & (embed_u64)1 << 63) != 0u,
                                 "a down entry whose top bit is clear is not normalized");
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
    for (int step = MMGR_POW5_SINGLE_HALF_STEPS; step < MMGR_POW5_UP_ENTRIES; step++)
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

    for (int step = 0; step < MMGR_POW5_DOWN_ENTRIES; step++)
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
    for (int step = 1; step < MMGR_POW5_UP_ENTRIES; step++)
    {
        // Explicit casts take both exponents from embed_iword into the int Unity's integer comparison
        // works in. pow5.h bounds e2 between -722 and 467, which fits int on every environment this
        // suite builds for
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE((int)mmgr_pow5_up[step - 1].e2, (int)mmgr_pow5_up[step].e2,
                                             "a larger power of five needs a larger binary exponent");
        // Explicit casts take both exponents from embed_iword into the int Unity's integer comparison
        // works in. pow5.h bounds e2 between -722 and 467, which fits int on every environment this
        // suite builds for
        TEST_ASSERT_LESS_THAN_INT_MESSAGE((int)mmgr_pow5_down[step - 1].e2, (int)mmgr_pow5_down[step].e2,
                                          "a smaller power of five needs a smaller binary exponent");
    }
}

/**
 * @brief Checks that the narrowest legal embed_iword carries the widest exponent in the tables.
 *
 * @note mmgr_types.h raises #error unless EMBED_WORD_BITS is 16, 32 or 64, so the narrowest
 *       embed_iword is embed_i16. The word16 environment is where this check binds.
 * @note -722 sits at index 8 of mmgr_pow5_down and is the furthest any e2 reaches from zero.
 */
void test_the_widest_exponent_fits_the_narrowest_word(void)
{
    const embed_iword widest_exponent = mmgr_pow5_down[MMGR_POW5_DOWN_ENTRIES - 1].e2;

    // Explicit cast takes the exponent from embed_iword into the int Unity's integer assertion
    // compares in. -722 fits embed_i16, the narrowest embed_iword, so it fits int everywhere
    TEST_ASSERT_EQUAL_INT_MESSAGE(-722, (int)widest_exponent,
                                  "the widest binary exponent is the one the header documents");
    // Explicit casts round -722 through embed_iword and back into int. A word that dropped the value
    // would return something other than -722 here
    TEST_ASSERT_EQUAL_INT_MESSAGE(-722, (int)(embed_iword)-722,
                                  "embed_i16 is the narrowest embed_iword and it carries -722");
}

/**
 * @brief Checks that walking one entry per set bit reaches a decimal exponent of 511.
 *
 * @note 511 is written as a literal rather than as MMGR_POW5_MAX. That constant expands to
 *       ((1 << MMGR_POW5_STEPS) - 1), which is what this loop computes, so comparing the two would
 *       assert a value against itself and hold for any entry count.
 * @note pow5.h states that a double's decimal exponent stays well inside 511, so a walk reaching it
 *       covers every value one can hold.
 */
void test_the_table_walk_reaches_the_exponent_range_a_double_needs(void)
{
    int largest_reachable_exponent = 0;

    for (int step = 0; step < MMGR_POW5_UP_ENTRIES; step++)
    {
        largest_reachable_exponent |= 1 << step;
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(511, largest_reachable_exponent,
                                  "one entry per set bit must reach a decimal exponent of 511");
}
