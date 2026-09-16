// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_fractio_accuracy.c
 * @brief Checks the six fract entries against the binary64 field positions taken from the format
 *        itself, over one pattern from each class a double can be: normal, subnormal, both zeros,
 *        the smallest and largest finite values, and an infinity.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note The field positions below are written from the binary64 definition and not taken from
 *       fractio.h. A wrong constant there would otherwise supply both the answer and the
 *       expectation, and every case would pass while every field was read from the wrong place.
 * @note Doubles are taken apart through a union declared here. mmgr_fract_to_bits performs that same
 *       reinterpretation, and using it to check itself would prove nothing.
 * @note Every expectation is either a literal a reader can work out from the format, or a
 *       reassembly built here from shifts. Neither reads a value the module produced.
 * @note Every integer type below comes from stdint.h. A wrong width alias would resize the shifts
 *       each field is placed by.
 * @note Contract checks on what the entries do with a null argument, and the static assertions that
 *       pin the layout constants against each other, live in test_fractio. This file asks whether
 *       the fields come out of the places the format puts them.
 */
#include <stdint.h>

#include "fractio/fractio.h"

#include "unity.h"

/**
 * @brief Expands to 63, the bit the binary64 sign occupies.
 *
 * @note Written from the format. fractio.h states the same position as MMGR_DBL_SIGN_SHIFT, and the
 *       point of repeating it here is that the two are then checkable against each other.
 */
#define MMGR_ACCURACY_SIGN_SHIFT 63u

/**
 * @brief Expands to 52, the bit the binary64 exponent field starts at.
 *
 * @note The stored mantissa fills bits 0 through 51, and the exponent sits directly above it.
 */
#define MMGR_ACCURACY_EXPONENT_SHIFT 52u

/**
 * @brief Expands to the eleven-bit mask an unshifted binary64 exponent fills.
 *
 * @note 0x7FF is eleven ones. A biased exponent runs 0 through 2047, and this is the whole of it.
 */
#define MMGR_ACCURACY_EXPONENT_MASK 0x7FFULL

/**
 * @brief Expands to the fifty-two-bit mask the stored binary64 mantissa fills.
 *
 * @note The leading one of a normal value is implicit and is not among these bits.
 */
#define MMGR_ACCURACY_MANTISSA_MASK 0x000FFFFFFFFFFFFFULL

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
 * @brief One pattern and the three fields the binary64 format puts in it.
 *
 * @note Every row below is worked out from the format by hand, which is what lets a row stand as an
 *       expectation on its own.
 */
typedef struct
{
    uint64_t bits;         /**< The whole sixty-four bit pattern. */
    uint64_t sign;         /**< Bit 63, as 0 or 1. */
    uint64_t exponent;     /**< The biased exponent, 0 through 2047. */
    uint64_t mantissa;     /**< The fifty-two stored bits. */
    const char *describes; /**< What class of value the row is, for a failing assertion to name. */
} AccuracyPattern;

/**
 * @brief One pattern from each class a double can be, with its fields worked out by hand.
 *
 * @note 1.0 is a biased exponent of 1023 over a zero mantissa. 3.0 is 1.5 times two, which is the
 *       same exponent plus one over a mantissa of a half, and a half of 2^52 is 0x8000000000000.
 * @note The smallest subnormal is a single low bit with an exponent field of zero, and the largest
 *       finite value is the highest exponent short of the all-ones one over a full mantissa.
 * @note An infinity is the all-ones exponent over a zero mantissa. No NaN row appears here: a
 *       signaling one can be quieted by passing through a double return, and the all-ones exponent
 *       is already covered.
 */
static const AccuracyPattern accuracy_pattern_of[] = {
    {0x3FF0000000000000ULL, 0ULL, 1023ULL, 0x0000000000000ULL, "one"},
    {0xBFF0000000000000ULL, 1ULL, 1023ULL, 0x0000000000000ULL, "minus one"},
    {0x4000000000000000ULL, 0ULL, 1024ULL, 0x0000000000000ULL, "two"},
    {0x3FE0000000000000ULL, 0ULL, 1022ULL, 0x0000000000000ULL, "a half"},
    {0x4008000000000000ULL, 0ULL, 1024ULL, 0x8000000000000ULL, "three"},
    {0x0000000000000000ULL, 0ULL, 0ULL, 0x0000000000000ULL, "a positive zero"},
    {0x8000000000000000ULL, 1ULL, 0ULL, 0x0000000000000ULL, "a negative zero"},
    {0x0000000000000001ULL, 0ULL, 0ULL, 0x0000000000001ULL, "the smallest subnormal"},
    {0x0010000000000000ULL, 0ULL, 1ULL, 0x0000000000000ULL, "the smallest normal"},
    {0x7FEFFFFFFFFFFFFFULL, 0ULL, 2046ULL, 0xFFFFFFFFFFFFFULL, "the largest finite value"},
    {0x7FF0000000000000ULL, 0ULL, 2047ULL, 0x0000000000000ULL, "an infinity"},
};

/**
 * @brief Returns the bit pattern of a double.
 *
 * @param[in] value Double to take apart.
 * @return          Its sixty-four bits, sign bit highest.
 * @note Every comparison in this suite goes through this. Reading a result back through the module
 *       that produced it would let one defect describe itself.
 */
static uint64_t accuracy_bits_of(double value)
{
    AccuracyDoubleBits reader;

    reader.value = value;
    return reader.bits;
}

/**
 * @brief Returns the double a bit pattern stands for.
 *
 * @param[in] bits Pattern to interpret.
 * @return         The same storage read as a double.
 * @note The reverse of accuracy_bits_of, and independent of mmgr_fract_from_bits for the same
 *       reason.
 */
static double accuracy_double_of(uint64_t bits)
{
    AccuracyDoubleBits reader;

    reader.bits = bits;
    return reader.value;
}

/**
 * @brief Assembles a bit pattern from a sign, a biased exponent and a stored mantissa.
 *
 * @param[in] sign     Sign bit, 0 or 1.
 * @param[in] exponent Biased exponent, 0 through 2047.
 * @param[in] mantissa Fifty-two stored bits.
 * @return             The three fields placed where the binary64 format puts them.
 * @note Built from the shifts declared at the top of this file, which is what makes it usable as an
 *       expectation for mmgr_fract_merge.
 * @note Each field is masked before it is shifted, matching what the format allows a field to hold.
 *       A caller here passes values already inside those bounds.
 */
static uint64_t accuracy_pattern_from(uint64_t sign, uint64_t exponent, uint64_t mantissa)
{
    return ((sign & 1ULL) << MMGR_ACCURACY_SIGN_SHIFT) |
           ((exponent & MMGR_ACCURACY_EXPONENT_MASK) << MMGR_ACCURACY_EXPONENT_SHIFT) |
           (mantissa & MMGR_ACCURACY_MANTISSA_MASK);
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every value this suite uses is either a file-scope constant or has automatic storage inside
 *       the case that builds it, and there is no shared state to prepare here.
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
 * @brief Checks the union, the assembly and the pattern table this suite rests on against values
 *        worked out by hand.
 *
 * @note Exists to catch a defect in the helpers as itself. Without this case a broken
 *       accuracy_pattern_from would surface as a merge mismatch, and fractio would be blamed for it.
 * @note The last check walks the whole table and reassembles each row from its own three fields, so
 *       a row whose fields do not add back up to its pattern fails here and not in a later case.
 */
void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FF0000000000000ULL, accuracy_bits_of(1.0), "one is not the bits of one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8000000000000000ULL, accuracy_bits_of(-0.0),
                                    "a negative zero is the sign bit alone");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FF0000000000000ULL, accuracy_bits_of(accuracy_double_of(0x3FF0000000000000ULL)),
                                    "a pattern taken to a double and back is not the pattern it started as");

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FF0000000000000ULL, accuracy_pattern_from(0ULL, 1023ULL, 0ULL),
                                    "the assembly does not place a biased exponent of 1023 at one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8000000000000000ULL, accuracy_pattern_from(1ULL, 0ULL, 0ULL),
                                    "the assembly does not place the sign at bit 63");

    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in. The table
    // holds eleven rows, which is far inside what an unsigned carries on any conforming target
    const unsigned row_count = (unsigned)(sizeof accuracy_pattern_of / sizeof accuracy_pattern_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const uint64_t reassembled = accuracy_pattern_from(
            accuracy_pattern_of[row].sign, accuracy_pattern_of[row].exponent, accuracy_pattern_of[row].mantissa);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].bits, reassembled, accuracy_pattern_of[row].describes);
    }
}

/**
 * @brief Checks that each of the three field entries returns the field the format puts at that
 *        position, across every class a double can be.
 *
 * @note The expectations come from the table, whose rows are worked out from the format. Nothing
 *       about them depends on the masks fractio declares.
 * @note A zero, a subnormal and an infinity are each in the table because the three entries read
 *       fields without interpreting them, and a defect that only shows on one class would otherwise
 *       go unseen.
 */
void test_every_field_comes_out_of_the_place_the_format_puts_it(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // case above
    const unsigned row_count = (unsigned)(sizeof accuracy_pattern_of / sizeof accuracy_pattern_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const embed_u64 produced_sign = EMBED_CALL(fract.sign, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const embed_u64 produced_exponent = EMBED_CALL(fract.exp, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const embed_u64 produced_mantissa = EMBED_CALL(fract.mant, FractioCfg, .bits = accuracy_pattern_of[row].bits);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].sign, produced_sign,
                                        accuracy_pattern_of[row].describes);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].exponent, produced_exponent,
                                        accuracy_pattern_of[row].describes);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].mantissa, produced_mantissa,
                                        accuracy_pattern_of[row].describes);
    }
}

/**
 * @brief Checks that the three fields taken out of a pattern put back together as that same pattern.
 *
 * @note Reads the fields through the module and reassembles them here. A split that dropped a bit or
 *       placed a field one position off fails even where each field looks plausible on its own.
 * @note Covers the whole word between them. The three fields tile all sixty-four bits, and a
 *       reassembly matching the original is what shows nothing fell between them.
 */
void test_the_three_fields_put_back_together_are_the_pattern_they_came_from(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned row_count = (unsigned)(sizeof accuracy_pattern_of / sizeof accuracy_pattern_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const embed_u64 produced_sign = EMBED_CALL(fract.sign, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const embed_u64 produced_exponent = EMBED_CALL(fract.exp, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const embed_u64 produced_mantissa = EMBED_CALL(fract.mant, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const uint64_t reassembled = accuracy_pattern_from(produced_sign, produced_exponent, produced_mantissa);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].bits, reassembled, accuracy_pattern_of[row].describes);
    }
}

/**
 * @brief Checks that mmgr_fract_merge places three fields where the format puts them.
 *
 * @note The expectation is the assembly declared in this file, built from its own shifts. A merge
 *       that shifted the exponent by the wrong amount fails against a position taken from the
 *       format.
 */
void test_merge_places_the_fields_where_the_format_puts_them(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned row_count = (unsigned)(sizeof accuracy_pattern_of / sizeof accuracy_pattern_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const uint64_t expected = accuracy_pattern_from(
            accuracy_pattern_of[row].sign, accuracy_pattern_of[row].exponent, accuracy_pattern_of[row].mantissa);
        const embed_u64 produced =
            EMBED_CALL(fract.merge, FractioCfg, .sign = accuracy_pattern_of[row].sign,
                       .exp = accuracy_pattern_of[row].exponent, .mant = accuracy_pattern_of[row].mantissa);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, produced, accuracy_pattern_of[row].describes);
    }
}

/**
 * @brief Checks that the two reinterpretation entries move a value between its two readings without
 *        changing a bit.
 *
 * @note Each is compared against this file's own union on the same storage. A defect in either
 *       direction shows up on its own, without the other covering for it.
 * @note Compared as patterns in both directions. A double comparison would report two NaN patterns
 *       unequal and two zero patterns equal, and neither is what this case asks.
 */
void test_the_two_readings_of_one_storage_agree_with_the_format(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned row_count = (unsigned)(sizeof accuracy_pattern_of / sizeof accuracy_pattern_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const double produced_double = EMBED_CALL(fract.from_bits, FractioCfg, .bits = accuracy_pattern_of[row].bits);
        const embed_u64 produced_bits =
            EMBED_CALL(fract.to_bits, FractioCfg, .val = accuracy_double_of(accuracy_pattern_of[row].bits));

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].bits, accuracy_bits_of(produced_double),
                                        accuracy_pattern_of[row].describes);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_pattern_of[row].bits, produced_bits,
                                        accuracy_pattern_of[row].describes);
    }
}
