// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_cellularum_laboro_accuracy.c
 * @brief Checks what the four text conversions actually produce, against exact integers where the
 *        value is one and against the compiler's own reading of the same decimal where it is not.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note The string verbs are not here. Whether len stops at a terminator or find reports the right
 *       offset is a contract test_cellularum_laboro asks about. This file asks what number comes out
 *       of a run of digits.
 * @note A decimal a double cannot hold is checked against the same decimal written as a literal, and
 *       the compiler is what converts that one. GCC folds a floating literal through MPFR and rounds
 *       it correctly, which makes it a second implementation of the conversion under test and not a
 *       second copy of it.
 * @warning That oracle is the toolchain's. A disagreement names a difference between the two
 *          implementations and does not by itself say which one is wrong, though a correctly rounded
 *          reference is the far likelier of the two to be right.
 * @note Doubles are compared bit for bit through a union declared here. An approximate comparison
 *       passes a conversion that is one unit in the last place out, which is the whole of what this
 *       file is looking for.
 * @note Every integer type below comes from stdint.h. A wrong width alias would resize the values
 *       each comparison is made in.
 */
#include <stdint.h>

#include "cellularum_laboro/cellularum_laboro.h"

#include "unity.h"

/**
 * @brief Reads the bits of a double without going through the library.
 *
 * @note fractio performs this same reinterpretation, and mmgr_cellul_to_double assembles its result
 *       through it. Declaring the union here keeps the comparison independent of both.
 */
typedef union {
    double value;  /**< The double being read. */
    uint64_t bits; /**< The same storage as an integer. */
} AccuracyDoubleBits;

/**
 * @brief One decimal and the value a correctly rounded conversion gives for it.
 *
 * @note expected carries the compiler's reading of the same digits text holds. The two are written
 *       side by side in every row below, which is what lets a reader check that they say the same
 *       number.
 */
typedef struct
{
    const char *text; /**< Decimal to convert [BORROWS]. */
    double expected;  /**< The same decimal as a literal, converted by the compiler. */
} AccuracyDecimal;

/**
 * @brief Decimals a double holds with nothing rounded.
 *
 * @note Every value here is a fraction whose denominator is a power of two, or an integer under
 *       2^53. A conversion has one correct answer for each and no rounding to get wrong, which is
 *       what separates a placement defect from a rounding defect.
 * @note The exponent forms are here to check they reach the same value as the plain ones. 1e2 and
 *       100 and 0.01e4 are one number written three ways.
 */
static const AccuracyDecimal accuracy_exact_of[] = {
    {"1", 1.0},
    {"0", 0.0},
    {"3", 3.0},
    {"100", 100.0},
    {"0.5", 0.5},
    {"0.25", 0.25},
    {"0.0625", 0.0625},
    {"-2.5", -2.5},
    {"1e2", 100.0},
    {"1E2", 100.0},
    {"1.0e2", 100.0},
    {"0.01e4", 100.0},
    {"9007199254740991", 9007199254740991.0},
};

/**
 * @brief Decimals no double holds, where the answer is the correctly rounded one.
 *
 * @note A tenth and a fifth are repeating binary fractions. Pi to fourteen places and the two powers
 *       of ten at the ends of the exact range each need rounding as well.
 * @note 1e22 is the last power of ten a double holds outright and 1e23 is the first it does not.
 *       That pair is where a converter taking a shortcut through repeated multiplication parts
 *       company with a correctly rounded one.
 */
static const AccuracyDecimal accuracy_rounded_of[] = {
    {"0.1", 0.1},
    {"0.2", 0.2},
    {"1e-1", 1e-1},
    {"3.14159265358979", 3.14159265358979},
    {"1e22", 1e22},
    {"1e23", 1e23},
    {"2.2250738585072014e-308", 2.2250738585072014e-308},
    {"1.7976931348623157e308", 1.7976931348623157e308},
};

/**
 * @brief Returns the bit pattern of a double.
 *
 * @param[in] value Double to take apart.
 * @return          Its sixty-four bits, sign bit highest.
 * @note Every double comparison in this suite goes through this. A direct comparison would pass a
 *       result that differs in its last bit.
 */
static uint64_t accuracy_bits_of(double value)
{
    AccuracyDoubleBits reader;

    reader.value = value;
    return reader.bits;
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
 * @brief Checks the bit reader this suite rests on against values worked out by hand.
 *
 * @note Exists to catch a defect in the helper as itself. Without this case a broken
 *       accuracy_bits_of would surface as a conversion mismatch, and cellularum_laboro would be
 *       blamed for it.
 * @note A double of 1.0 is a biased exponent of 1023 over a zero significand, which is
 *       0x3FF0000000000000. A negative zero is the sign bit and nothing else.
 */
void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x3FF0000000000000ULL, accuracy_bits_of(1.0), "one is not the bits of one");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8000000000000000ULL, accuracy_bits_of(-0.0),
                                    "a negative zero is the sign bit alone");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, accuracy_bits_of(0.0), "a positive zero is every bit clear");
}

/**
 * @brief Checks that a signed decimal integer comes back as the integer it names.
 *
 * @note The expectations are literals. A run of digits under the accumulator's width has one value
 *       and no rounding, which makes the comparison exact.
 * @note Leading whitespace and an explicit sign are included because the entry documents accepting
 *       both, and a value that survives them is the value the digits name.
 */
void test_a_signed_integer_comes_back_as_the_integer_it_names(void)
{
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "0"), "zero");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(7, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "7"), "one digit");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "12345"), "five digits");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "-12345"),
                                    "a minus sign");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(42, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "+42"), "a plus sign");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(42, EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = "   42"),
                                    "leading whitespace");
}

/**
 * @brief Checks that an unsigned decimal integer comes back as the integer it names.
 *
 * @note The largest value here is the widest run of digits that fits every environment's accumulator,
 *       since the entry documents that accumulator as embed_word wide and that width changes between
 *       builds.
 */
void test_an_unsigned_integer_comes_back_as_the_integer_it_names(void)
{
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = "0"), "zero");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(7u, EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = "7"), "one digit");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(12345u, EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = "12345"),
                                     "five digits");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(42u, EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = "+42"), "a plus sign");
}

/**
 * @brief Checks that a decimal a double holds outright comes back exactly.
 *
 * @note These rows have one correct answer and no rounding to get wrong. A failure here is a
 *       misplaced digit or a mishandled exponent, and not a rounding that went the wrong way.
 * @note The three ways of writing one hundred are in the table together, and each is compared
 *       against the same literal.
 */
void test_a_decimal_a_double_holds_outright_comes_back_exactly(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in. The table
    // holds thirteen rows, which is far inside what an unsigned carries on any conforming target
    const unsigned row_count = (unsigned)(sizeof accuracy_exact_of / sizeof accuracy_exact_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const double produced = EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = accuracy_exact_of[row].text);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(accuracy_exact_of[row].expected), accuracy_bits_of(produced),
                                        accuracy_exact_of[row].text);
    }
}

/**
 * @brief Checks that a decimal no double holds comes back as the correctly rounded value.
 *
 * @note This is the case the file exists for. Every row needs rounding, and a converter that is one
 *       unit in the last place out fails here while passing every exact row above.
 * @note The expectation is the compiler's reading of the same digits, which is a second
 *       implementation of the conversion and shares no table with this one.
 */
void test_a_decimal_no_double_holds_comes_back_correctly_rounded(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // case above
    const unsigned row_count = (unsigned)(sizeof accuracy_rounded_of / sizeof accuracy_rounded_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        const double produced = EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = accuracy_rounded_of[row].text);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(accuracy_rounded_of[row].expected), accuracy_bits_of(produced),
                                        accuracy_rounded_of[row].text);
    }
}

/**
 * @brief Checks that the float entry gives the double entry's value narrowed to float.
 *
 * @note The expectation is the double entry's own result put through the same narrowing the C
 *       conversion performs, which is what the entry documents itself as doing.
 * @note Compared as floats through TEST_ASSERT_EQUAL_FLOAT's exact form for equal values. A float
 *       carries twenty four significand bits, and two values agreeing there agree in every bit.
 */
void test_the_float_entry_is_the_double_entry_narrowed(void)
{
    // Explicit cast narrows the size_t sizeof quotient to the unsigned the loop counts in, as in the
    // cases above
    const unsigned row_count = (unsigned)(sizeof accuracy_rounded_of / sizeof accuracy_rounded_of[0]);

    for (unsigned row = 0u; row < row_count; row++)
    {
        // Explicit cast narrows the correctly rounded double to the float the entry returns. This is
        // the same conversion the entry performs on its own result, and it is what makes the two
        // comparable
        const float expected = (float)accuracy_rounded_of[row].expected;
        const float produced = EMBED_CALL(cellul.to_float, TransfiguroCfg, .src = accuracy_rounded_of[row].text);

        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(expected, produced, accuracy_rounded_of[row].text);
    }
}

/**
 * @brief Checks a case-insensitive compare of the same word typed two ways, where the word carries
 *        an accent.
 *
 * @note This is what a caller does with a case-insensitive compare. Two ways of writing one word, from
 *       a form field or a header, matched without caring which case the user typed.
 * @note The accented letter is identical in both strings and takes no part in what the answer should
 *       be. Only the ASCII letters around it differ in case, and a case-insensitive compare is
 *       defined to call the two equal.
 * @note The bytes are written as escapes so the answer does not depend on what encoding this file is
 *       saved in. Latin-1 is what a legacy protocol, an older filesystem or a serial device hands a
 *       caller, and 0xFC is its lowercase u with a diaeresis.
 * @note The plain ASCII pair is checked first. It is the same comparison with the accent removed,
 *       and it is what shows the difference is the accent and not the comparison.
 * @note The buffers are padded past the terminator because the compare reads a whole word at a time,
 *       which its own warning states.
 */
void test_a_case_insensitive_compare_holds_on_an_accented_word(void)
{
    // "muller" and "MULLER", the same name with no umlaut on it
    static const char plain_lower[32] = {'m', 'u', 'l', 'l', 'e', 'r', '\0'};
    static const char plain_upper[32] = {'M', 'U', 'L', 'L', 'E', 'R', '\0'};
    // The same name spelled with the umlaut, as Latin-1. 0xFC is a lowercase u with diaeresis, and
    // it is the identical byte in both
    static const char accented_lower[32] = {'m', '\xFC', 'l', 'l', 'e', 'r', '\0'};
    static const char accented_upper[32] = {'M', '\xFC', 'L', 'L', 'E', 'R', '\0'};

    TEST_ASSERT_TRUE_MESSAGE(
        EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = plain_lower, .other = plain_upper, .cap = 31u, .ci = EMBED_TRUE),
        "cafe and CAFe did not match without regard to case");

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = accented_lower, .other = accented_upper,
                                        .cap = 31u, .ci = EMBED_TRUE),
                             "the same two words stopped matching once the e carried an accent");

    // "yuz" and "YUZ" with the u carrying a diaeresis, as Latin-5. That is the Turkish word for a
    // face or a hundred, and it puts a z directly after the accented byte. The range test in
    // scrut_alpha lands on exactly the high bit for a lane holding z, and the borrow the accented
    // byte takes out of that lane costs it exactly the one bit the answer rests on
    static const char turkish_lower[32] = {'y', '\xFC', 'z', '\0'};
    static const char turkish_upper[32] = {'Y', '\xFC', 'Z', '\0'};

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = turkish_lower, .other = turkish_upper,
                                        .cap = 31u, .ci = EMBED_TRUE),
                             "a Turkish word stopped matching its own uppercase form");
}

/**
 * @brief Checks a case-insensitive search for a word that follows an accented one.
 *
 * @note The other thing a caller does with these entries: find a word in a line of text without
 *       caring about its case. The line here is a Latin-1 name of the kind a directory listing or a
 *       contact record carries.
 * @note The needle is entirely ASCII, so nothing about the search itself involves a byte at or above
 *       0x80. What differs from the plain line is only that the text around the needle carries one.
 */
void test_a_case_insensitive_search_finds_a_word_after_an_accented_one(void)
{
    // "Muller Str" and the same with the u carrying a diaeresis, as Latin-1
    static const char plain_line[32] = {'M', 'u', 'l', 'l', 'e', 'r', ' ', 'S', 't', 'r', '\0'};
    static const char accented_line[32] = {'M', '\xFC', 'l', 'l', 'e', 'r', ' ', 'S', 't', 'r', '\0'};
    static const char needle[8] = {'l', 'l', 'e', 'r', '\0'};

    TEST_ASSERT_NOT_NULL_MESSAGE(EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = plain_line, .cap = 31u,
                                            .other = needle, .other_cap = 7u, .ci = EMBED_TRUE),
                                 "menu was not found in the line without the accent");

    TEST_ASSERT_NOT_NULL_MESSAGE(EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = accented_line, .cap = 31u,
                                            .other = needle, .other_cap = 7u, .ci = EMBED_TRUE),
                                 "the same word stopped being found once the line carried an accent");
}

/**
 * @brief Sweeps every byte at every position to find where a case-insensitive compare stops folding.
 *
 * @note Builds two strings holding the same bytes, one lowercase and one uppercase in its ASCII
 *       letters, with one non-letter byte planted at a chosen position in both. A case-insensitive
 *       compare is defined to call every such pair equal, whatever the planted byte is.
 * @note Reports the first pair that disagrees, naming the byte and the position, so the domain the
 *       lane comparison actually holds over is measured and not guessed at.
 */
void test_a_case_insensitive_compare_holds_for_every_planted_byte(void)
{
    for (unsigned planted = 0x80u; planted < 0x100u; planted++)
    {
        for (unsigned length = 1u; length <= 12u; length++)
        {
            const unsigned at = 0u;
            char lower_of[32] = {0};
            char upper_of[32] = {0};

            for (unsigned index = 0u; index < length; index++)
            {
                // Explicit casts narrow the loop counters to the char a position holds. Both stay
                // inside what a byte carries
                lower_of[index] = (char)('a' + (int)(index % 26u));
                upper_of[index] = (char)('A' + (int)(index % 26u));
            }
            lower_of[at] = (char)planted;
            upper_of[at] = (char)planted;
            lower_of[length] = '\0';
            upper_of[length] = '\0';

            // The planted byte and the string length are folded into the reported value. A failure
            // then names both without needing a formatted message: the low byte is the planted
            // value and the next byte up is the length the string was built at
            const unsigned planted_and_position = planted | (length << 8);
            const unsigned matched =
                EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = lower_of, .other = upper_of, .cap = 31u, .ci = EMBED_TRUE)
                    ? planted_and_position
                    : 0u;

            TEST_ASSERT_EQUAL_HEX32_MESSAGE(planted_and_position, matched,
                                            "a case-insensitive compare reported two cased forms unequal; the "
                                            "expected value is the planted byte with its position above it");
        }
    }
}

/**
 * @brief Checks that a sign in the text reaches the value it belongs to.
 *
 * @note A negative zero is included because it is the one value where the sign is the only bit that
 *       carries it, and a conversion that dropped the sign would return a positive zero that
 *       compares equal to it under every test but this one.
 */
void test_a_sign_in_the_text_reaches_the_value(void)
{
    const double negative_zero = EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = "-0.0");
    const double negative_tenth = EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = "-0.1");

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(-0.0), accuracy_bits_of(negative_zero),
                                    "a negative zero came back without its sign");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(-0.1), accuracy_bits_of(negative_tenth),
                                    "a negative tenth is not the positive one with its sign bit set");
}
