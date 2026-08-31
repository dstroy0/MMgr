// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_transformo_accuracy.c
 * @brief Checks what mmgr_muto_scale and mmgr_muto_scale_to_u64 actually produce against products
 *        worked out in exact integer arithmetic, over the range where the answer is representable
 *        and the comparison can therefore be exact.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Nothing here reads a pow5 entry, a power of ten table, or a converted value to build an
 *       expectation. Every product is raised from 1 by repeated multiplication in this file. A
 *       defect shared between the tables and the scaling then cannot cancel itself out.
 * @note Doubles are taken apart through a union declared here. fractio is one of the modules a wrong
 *       answer would come from, and reading a result through it would let one defect describe
 *       itself.
 * @note Every integer type below comes from stdint.h. A wrong width alias would resize the
 *       arithmetic each comparison is made in.
 * @note The two entries are compared bit for bit. An approximate comparison passes a conversion that
 *       is one unit in the last place out, which is the defect this suite exists to catch.
 * @note Contract checks on what the entries do with a mantissa of zero or an exponent past the
 *       tables live in test_transformo. This file asks what the number is.
 */
#include <stdint.h>

#include "transformo/transformo.h"

#include "unity.h"

/**
 * @brief Expands to the first integer a double can no longer hold exactly, which is two to the 53.
 *
 * @note A double carries a 53-bit significand. Every integer below this converts with nothing
 *       rounded, which is what lets the conversion stand as an expectation. The cases below assert a
 *       product is under this before comparing against it.
 */
#define MMGR_ACCURACY_DOUBLE_EXACT_MAX (1ULL << 53)

/**
 * @brief Expands to 19, one past the largest power of ten accuracy_ten_to_the will raise.
 *
 * @note Ten to the nineteenth passes what a uint64_t holds. Stopping at eighteen keeps every power
 *       this file builds exact.
 */
#define MMGR_ACCURACY_TEN_STEPS 19

/**
 * @brief Reads the bits of a double without going through the library.
 *
 * @note A union is the reinterpretation this suite needs and the one fractio performs internally.
 *       Declaring it here keeps the comparison independent of the module whose output it reads.
 */
typedef union {
    double value;  /**< The double being read. */
    uint64_t bits; /**< The same storage as an integer. */
} AccuracyDoubleBits;

/**
 * @brief Returns the bit pattern of a double.
 *
 * @param[in] value Double to take apart.
 * @return          Its sixty-four bits, sign bit highest.
 * @note Every comparison in this suite goes through this. A direct double comparison would pass a
 *       result that differs in its last bit.
 */
static uint64_t accuracy_bits_of(double value)
{
    AccuracyDoubleBits reader;

    reader.value = value;
    return reader.bits;
}

/**
 * @brief Raises ten to a given power, exactly, in a 64-bit integer.
 *
 * @param[in] exponent Power to raise ten to, 0 through 18.
 * @return             Ten raised to exponent.
 * @note Built from 1 by repeated multiplication. It shares nothing with the powers of ten transformo
 *       applies. A defect in one of those tables cannot appear on both sides of a comparison.
 * @warning An exponent at or above MMGR_ACCURACY_TEN_STEPS overflows a uint64_t and wraps. Every
 *          caller here passes a literal inside that bound, and the assertion below pins it.
 */
static uint64_t accuracy_ten_to_the(unsigned exponent)
{
    uint64_t power = 1ULL;

    TEST_ASSERT_LESS_THAN_UINT_MESSAGE(MMGR_ACCURACY_TEN_STEPS, exponent,
                                       "a power of ten past the eighteenth does not fit a uint64_t");
    for (unsigned applied = 0u; applied < exponent; applied++)
    {
        power *= 10ULL;
    }
    return power;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every value this suite uses has automatic storage inside the case that builds it, and there
 *       is no shared state to prepare here.
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
 * @brief Checks the bit reader and the exact powers this suite rests on against values worked out by
 *        hand.
 *
 * @note Exists to catch a defect in the helpers as itself. Without this case a broken
 *       accuracy_bits_of would surface as a conversion mismatch, and transformo would be blamed for
 *       it.
 * @note Every expectation here is a literal a reader can check without running anything. A double of
 *       1.0 is a biased exponent of 1023 over a zero significand, which is 0x3FF0000000000000, and
 *       each doubling or halving moves that exponent by one.
 */
void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FF0000000000000ULL, accuracy_bits_of(1.0), "one is not the bits of one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x4000000000000000ULL, accuracy_bits_of(2.0), "two is one exponent above one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FE0000000000000ULL, accuracy_bits_of(0.5), "a half is one exponent below one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, accuracy_bits_of(0.0), "a positive zero is every bit clear");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8000000000000000ULL, accuracy_bits_of(-0.0),
                                    "a negative zero is the sign bit alone");

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(1ULL, accuracy_ten_to_the(0u), "ten to the zero is one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(1000ULL, accuracy_ten_to_the(3u), "ten to the three is a thousand");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(1000000000000000000ULL, accuracy_ten_to_the(18u),
                                    "ten to the eighteen is a billion billion");
}

/**
 * @brief Checks that a mantissa scaled by a non-negative exponent comes back as the exact product
 *        wherever a double holds that product outright.
 *
 * @note The expectation is the integer product converted by the compiler, which the language makes
 *       exact below MMGR_ACCURACY_DOUBLE_EXACT_MAX. Nothing this library does is involved in it.
 * @note The pairs span both routes muto_scale takes. An exponent at or under twenty-two with a small
 *       mantissa is settled in double arithmetic, and the last pair passes that bound and goes
 *       through the 128-bit path, so one expectation covers both.
 */
void test_a_product_a_double_holds_exactly_comes_back_exactly(void)
{
    static const uint64_t mantissa_of[] = {1ULL, 123ULL, 7ULL, 999999ULL, 4503599627370495ULL, 2ULL};
    static const unsigned exponent_of[] = {0u, 2u, 6u, 3u, 0u, 15u};
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in. The array
    // holds six entries, which is far inside what an unsigned carries on any conforming target
    const unsigned pair_count = (unsigned)(sizeof mantissa_of / sizeof mantissa_of[0]);

    for (unsigned pair = 0u; pair < pair_count; pair++)
    {
        const uint64_t product = mantissa_of[pair] * accuracy_ten_to_the(exponent_of[pair]);

        TEST_ASSERT_LESS_THAN_UINT64_MESSAGE(MMGR_ACCURACY_DOUBLE_EXACT_MAX, product,
                                             "this case only holds where a double carries the product outright");

        // Explicit cast converts the exact integer product to the double it is compared against. The
        // assertion above holds it under 2^53, where the conversion rounds nothing
        const double expected = (double)product;
        embed_u64 mantissa = mantissa_of[pair];
        // Explicit cast narrows the loop's unsigned exponent to the embed_iword the entry carries.
        // Every value in exponent_of is under nineteen, which fits an embed_iword of any width
        const double produced = EMBED_CALL(muto.scale, TransformoCfg, .mant = &mantissa,
                                           .ex = (embed_iword)exponent_of[pair], .neg = EMBED_FALSE);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(expected), accuracy_bits_of(produced),
                                        "a product a double holds exactly did not come back exactly");
    }
}

/**
 * @brief Checks the negative exponents whose value ends in binary, where the exact answer is a
 *        literal.
 *
 * @note Five over ten, twenty five over a hundred and so on are each a half of a power of two, which
 *       a double holds with nothing rounded. An entry that reached these through a
 *       reciprocal would land one unit low, and comparing bits is what shows that.
 * @note These are the only negative exponents this file can pin without exact rational arithmetic.
 *       Every other one, a tenth included, is a repeating binary fraction whose correct rounding is
 *       not a literal anyone can write by hand.
 */
void test_a_negative_exponent_that_ends_in_binary_is_exact(void)
{
    static const uint64_t mantissa_of[] = {5ULL, 25ULL, 125ULL, 625ULL};
    static const embed_iword exponent_of[] = {-1, -2, -3, -4};
    static const double expected_of[] = {0.5, 0.25, 0.125, 0.0625};
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // case above
    const unsigned pair_count = (unsigned)(sizeof mantissa_of / sizeof mantissa_of[0]);

    for (unsigned pair = 0u; pair < pair_count; pair++)
    {
        embed_u64 mantissa = mantissa_of[pair];
        const double produced =
            EMBED_CALL(muto.scale, TransformoCfg, .mant = &mantissa, .ex = exponent_of[pair], .neg = EMBED_FALSE);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(expected_of[pair]), accuracy_bits_of(produced),
                                        "a value that ends in binary did not come back exactly");
    }
}

/**
 * @brief Checks that the sign is applied to the value and changes nothing else about it.
 *
 * @note Compares against the negation of the positive result, which holds whatever the magnitude
 *       rounds to. What it pins is that the sign bit is the one bit differing between the two.
 */
void test_the_sign_changes_one_bit_and_nothing_else(void)
{
    embed_u64 positive_mantissa = 123ULL;
    embed_u64 negative_mantissa = 123ULL;

    const double positive =
        EMBED_CALL(muto.scale, TransformoCfg, .mant = &positive_mantissa, .ex = 2, .neg = EMBED_FALSE);
    const double negative =
        EMBED_CALL(muto.scale, TransformoCfg, .mant = &negative_mantissa, .ex = 2, .neg = EMBED_TRUE);

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(positive) | 0x8000000000000000ULL, accuracy_bits_of(negative),
                                    "the negative result is not the positive one with its sign bit set");
}

/**
 * @brief Checks that the integer entry returns the exact product wherever that product fits a
 *        64-bit integer.
 *
 * @note The expectation is the same repeated multiplication the case above uses, so nothing about
 *       the answer comes from the powers of ten or the pow5 entries the entry applies.
 * @note A binary exponent of zero is left unset, which the compound literal zeroes. The mantissa
 *       reaches the entry as a plain integer, and the product is the whole of what is checked.
 */
void test_the_integer_entry_returns_the_exact_product(void)
{
    static const uint64_t mantissa_of[] = {1ULL, 123ULL, 7ULL, 999999ULL, 1ULL};
    static const unsigned exponent_of[] = {0u, 2u, 10u, 3u, 18u};
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned pair_count = (unsigned)(sizeof mantissa_of / sizeof mantissa_of[0]);

    for (unsigned pair = 0u; pair < pair_count; pair++)
    {
        const uint64_t expected = mantissa_of[pair] * accuracy_ten_to_the(exponent_of[pair]);
        embed_u64 mantissa = mantissa_of[pair];
        // Explicit cast narrows the loop's unsigned exponent to the embed_iword the entry carries,
        // as in the double case above
        const embed_u64 produced =
            EMBED_CALL(muto.scale_to_u64, TransformoCfg, .mant = &mantissa, .ex = (embed_iword)exponent_of[pair]);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, produced, "the integer entry did not return the exact product");
    }
}

/**
 * @brief Checks that a binary exponent scales the mantissa by exactly that power of two.
 *
 * @note Reads args->e2, which the double entry leaves at zero. That is the one input the two entries
 *       do not share.
 * @note The expectation is a shift, which is exact for every case here. A scaling that lost or
 *       gained a bit shows up as a wrong integer.
 */
void test_a_binary_exponent_shifts_the_integer_result(void)
{
    static const embed_iword binary_exponent_of[] = {1, 4, 10};
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned exponent_count = (unsigned)(sizeof binary_exponent_of / sizeof binary_exponent_of[0]);

    for (unsigned step = 0u; step < exponent_count; step++)
    {
        embed_u64 mantissa = 3ULL;
        const uint64_t expected = 3ULL << binary_exponent_of[step];
        const embed_u64 produced =
            EMBED_CALL(muto.scale_to_u64, TransformoCfg, .mant = &mantissa, .e2 = binary_exponent_of[step], .ex = 0);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, produced, "a binary exponent did not scale by its power of two");
    }
}
