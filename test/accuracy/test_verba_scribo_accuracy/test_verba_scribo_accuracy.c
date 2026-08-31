// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_verba_scribo_accuracy.c
 * @brief Checks the digits every entry writes against the C library's own formatting, and checks the
 *        floating point form by reading it back with strtod and comparing bit patterns.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The integer entries have an exact printf equivalent, so snprintf is the reference for them. It
 *       is a separate implementation of the same formatting and shares nothing with this tree.
 * @note The floating point form is checked by round trip instead. Seventeen significant digits are
 *       enough to name any double exactly. A value written at that width and read back with strtod
 *       has to give the same bits, which is a statement about the digits with no reference formatter
 *       in it at all, and it fails on a single wrong digit anywhere in the run.
 * @note At fewer than seventeen digits the round trip cannot be exact, so what is checked there is the
 *       error bound the digit count implies. A form that dropped a digit or rounded the wrong way
 *       lands outside it.
 * @note The two forms g picks between are chosen on where the decimal exponent falls, and the values
 *       below straddle both boundaries in both directions, since a form chosen wrongly is still a
 *       readable number.
 * @note Contract checks live in test_verba_scribo. This file asks what the digits are.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verba_scribo/verba_scribo.h"

#include "unity.h"

/**
 * @brief Expands to 512u, the bytes every destination buffer here holds.
 *
 * @note Far past the longest string any case writes, which keeps a short buffer from ever being the
 *       reason a case fails.
 */
#define MMGR_ACCURACY_VERBA_BUFFER 512u

/**
 * @brief Expands to 17u, the significant digits that name a double exactly.
 *
 * @note A binary64 value written at this many decimal digits and read back reaches the same bits. At
 *       sixteen two values can share a rendering, and at eighteen the extra digit carries nothing.
 */
#define MMGR_ACCURACY_VERBA_EXACT_DIGITS 17u

/**
 * @brief Returns the bit pattern of a double, read as an integer.
 *
 * @param[in] value Value to take apart.
 * @return          Its eight bytes as a uint64_t.
 * @note Goes through memcpy and not a union or a pointer cast, which keeps this independent of how
 *       the library reads a double's bits and bends no aliasing rule to do it.
 */
static uint64_t accuracy_bits_of(double value)
{
    uint64_t bits = 0u;

    (void)memcpy(&bits, &value, sizeof bits);
    return bits;
}

/**
 * @brief Terminates a buffer at the offset a run of entries reached.
 *
 * @param[out] out Destination buffer [BORROWS].
 * @param[in]  at  Offset the last entry reported.
 * @return         The length the terminator was stored at.
 * @note Every case writes its fields and then finishes, and doing that in one place keeps the cases
 *       reading as the values and widths they are checking.
 * @warning The capacity is fixed at MMGR_ACCURACY_VERBA_BUFFER here, so out must be a buffer of that
 *          size. The one case that uses a smaller buffer terminates it itself.
 */
static size_t accuracy_finish_at(char *out, size_t at)
{
    return EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = out, .cap = MMGR_ACCURACY_VERBA_BUFFER, .at = at);
}

/**
 * @brief Returns ten raised to a small exponent.
 *
 * @param[in] exponent Power to raise ten to, 0 through 17.
 * @return             The result.
 * @note Multiplied out and not taken from a library call, which leaves the error bound below resting
 *       on nothing this file did not compute. Every power in that range is exact in a binary64, since
 *       the odd part of ten to the seventeenth is still under two to the fifty-third.
 */
static double accuracy_power_of_ten(unsigned exponent)
{
    double value = 1.0;

    for (unsigned step = 0u; step < exponent; step++)
    {
        value *= 10.0;
    }
    return value;
}

/**
 * @brief Returns whether a value sits exactly halfway between two renderings at a decimal count.
 *
 * @param[in] value    Value being written.
 * @param[in] decimals Digits after the point.
 * @return             1 when the value is an exact tie at that count, 0 otherwise.
 * @note A tie is the one input the two rounding rules disagree about, and which rule a C library
 *       applies is not fixed by the standard. This module documents ties to even and the host's
 *       printf on this toolchain rounds them away from zero. A comparison at a tie would then report
 *       a difference of convention as a defect.
 * @note Ties are checked directly against the documented rule in their own case below, so nothing is
 *       lost by leaving them out of the comparison against the reference.
 * @warning Only meaningful where the scaled value is small enough to carry a fractional part. A
 *          magnitude past two to the fifty-third has no fraction left and reports no tie, which is
 *          correct: there is nothing there to round.
 */
static int accuracy_is_tie(double value, unsigned decimals)
{
    const double scaled = fabs(value) * accuracy_power_of_ten(decimals);

    return ((scaled - floor(scaled)) == 0.5) ? 1 : 0;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every buffer here has automatic storage inside the case that builds it, so there is no shared
 *       state to prepare.
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
 * @brief Checks that the reference formatting and the round trip behave as the cases assume.
 *
 * @note Exists to catch a wrong assumption about the references as themselves. The cases lean on
 *       snprintf zero padding and on strtod reading back what snprintf wrote, and a platform that did
 *       either differently would report the module as wrong everywhere.
 * @note The round trip is checked on the C library alone: a value written by snprintf at seventeen
 *       digits and read back by strtod has to give the same bits, or the round trip cases below prove
 *       nothing about the module.
 * @note The bit reader is checked against values whose patterns are known, since every floating point
 *       comparison here goes through it.
 */
void test_the_references_this_suite_relies_on_behave_as_assumed(void)
{
    static const double value_of[] = {0.1, 1.0 / 3.0, 1.7976931348623157e308, 5e-324, 12345.6789, -2.5e-17};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);
    char reference[64];

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x0000000000000000), accuracy_bits_of(0.0),
                                    "the bit reader does not read a positive zero");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x8000000000000000), accuracy_bits_of(-0.0),
                                    "the bit reader does not read a negative zero");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x3FF0000000000000), accuracy_bits_of(1.0),
                                    "the bit reader does not read a one");

    (void)snprintf(reference, sizeof reference, "%0*llu", 5, 42uLL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("00042", reference, "the reference does not zero pad to a width");

    (void)snprintf(reference, sizeof reference, "%*llu", 6, 42uLL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("    42", reference, "the reference does not pad with spaces to a column");

    for (unsigned index = 0u; index < value_count; index++)
    {
        char message[160];

        (void)snprintf(reference, sizeof reference, "%.*e", (int)(MMGR_ACCURACY_VERBA_EXACT_DIGITS - 1u),
                       value_of[index]);
        (void)snprintf(message, sizeof message, "the C library does not round trip %s at seventeen digits", reference);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(value_of[index]), accuracy_bits_of(strtod(reference, NULL)),
                                        message);
    }
}

/**
 * @brief Checks the unsigned and signed base ten entries against snprintf.
 *
 * @note The extremes of both types are included. The most negative signed value is the one input with
 *       no positive counterpart, and a magnitude taken by negating it wraps back to itself.
 * @note Four entries write base ten and differ in what they pad with and where the width comes from.
 *       Each is checked separately, since a table that pointed two of them at one function would pass
 *       whichever case it was written for.
 */
void test_the_base_ten_entries_match_the_reference(void)
{
    static const uint64_t unsigned_of[] = {0uLL,
                                           1uLL,
                                           9uLL,
                                           10uLL,
                                           99uLL,
                                           100uLL,
                                           255uLL,
                                           65535uLL,
                                           999999999uLL,
                                           4294967295uLL,
                                           9999999999999999999uLL,
                                           18446744073709551615uLL};
    static const int64_t signed_of[] = {
        0LL, 1LL, -1LL, 9LL, -9LL, 10LL, -10LL, 9223372036854775807LL, -9223372036854775807LL - 1LL};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in
    const unsigned unsigned_count = (unsigned)(sizeof unsigned_of / sizeof unsigned_of[0]);
    const unsigned signed_count = (unsigned)(sizeof signed_of / sizeof signed_of[0]);

    for (unsigned index = 0u; index < unsigned_count; index++)
    {
        char produced[MMGR_ACCURACY_VERBA_BUFFER];
        char reference[64];
        char message[160];

        (void)snprintf(reference, sizeof reference, "%llu", (unsigned long long)unsigned_of[index]);
        (void)snprintf(message, sizeof message, "an unsigned value of %llu", (unsigned long long)unsigned_of[index]);

        const size_t after_u64 = EMBED_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = produced, .cap = sizeof produced,
                                            .at = 0u, .val = unsigned_of[index]);

        TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), accuracy_finish_at(produced, after_u64), message);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

        const size_t after_u32 = EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = produced, .cap = sizeof produced,
                                            .at = 0u, .val = unsigned_of[index]);

        (void)accuracy_finish_at(produced, after_u32);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

        const size_t after_uint =
            EMBED_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = produced, .cap = sizeof produced, .at = 0u,
                       .val = unsigned_of[index], .base = 10u, .min = 1u);

        (void)accuracy_finish_at(produced, after_uint);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
    }

    for (unsigned index = 0u; index < signed_count; index++)
    {
        char produced[MMGR_ACCURACY_VERBA_BUFFER];
        char reference[64];
        char message[160];

        (void)snprintf(reference, sizeof reference, "%lld", (long long)signed_of[index]);
        (void)snprintf(message, sizeof message, "a signed value of %lld", (long long)signed_of[index]);

        const size_t after = EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = produced, .cap = sizeof produced,
                                        .at = 0u, .sval = signed_of[index]);

        TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), accuracy_finish_at(produced, after), message);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
    }
}

/**
 * @brief Checks the padded and based entries against snprintf at every width.
 *
 * @note The padding rules are where these entries differ from each other, and a width narrower than
 *       the value is checked to widen the field and not to cut the value.
 * @note Base sixteen is checked to come out lower case, which the header states and which a reference
 *       written with the upper case conversion would disagree with at every letter digit.
 * @note Base eight is included, since the entry that takes a base is documented to write ten for
 *       anything other than eight or sixteen, and a base that fell through to ten silently would look
 *       correct for every value below eight.
 */
void test_the_padded_and_based_entries_match_the_reference(void)
{
    static const uint64_t value_of[] = {0uLL,  1uLL,   7uLL,    8uLL,          15uLL,
                                        16uLL, 255uLL, 4096uLL, 4294967295uLL, 18446744073709551615uLL};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned width = 0u; width <= 20u; width++)
        {
            char produced[MMGR_ACCURACY_VERBA_BUFFER];
            char reference[64];
            char message[192];
            // Explicit cast narrows the loop counter to the uint8_t the min member holds
            const uint8_t min = (uint8_t)width;

            (void)snprintf(reference, sizeof reference, "%0*llx", (int)width, (unsigned long long)value_of[index]);
            (void)snprintf(message, sizeof message, "0x%llX in hexadecimal at a width of %u",
                           (unsigned long long)value_of[index], width);
            (void)accuracy_finish_at(produced,
                                     EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = produced,
                                                .cap = sizeof produced, .at = 0u, .val = value_of[index], .min = min));
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

            (void)snprintf(reference, sizeof reference, "%0*llu", (int)width, (unsigned long long)value_of[index]);
            (void)snprintf(message, sizeof message, "%llu in base ten at a width of %u",
                           (unsigned long long)value_of[index], width);
            (void)accuracy_finish_at(produced,
                                     EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = produced,
                                                .cap = sizeof produced, .at = 0u, .val = value_of[index], .min = min));
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

            (void)snprintf(reference, sizeof reference, "%0*llo", (int)width, (unsigned long long)value_of[index]);
            (void)snprintf(message, sizeof message, "%llu in octal at a width of %u",
                           (unsigned long long)value_of[index], width);
            (void)accuracy_finish_at(produced, EMBED_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = produced,
                                                          .cap = sizeof produced, .at = 0u, .val = value_of[index],
                                                          .base = 8u, .min = min));
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

            (void)snprintf(reference, sizeof reference, "%*llu", (int)width, (unsigned long long)value_of[index]);
            (void)snprintf(message, sizeof message, "%llu right aligned in %u columns",
                           (unsigned long long)value_of[index], width);
            (void)accuracy_finish_at(produced, EMBED_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = produced,
                                                          .cap = sizeof produced, .at = 0u, .val = value_of[index],
                                                          .columns = min));
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
        }
    }
}

/**
 * @brief Checks that a double written at seventeen significant digits reads back as the same value.
 *
 * @note This is the case the file exists for. Seventeen digits name a binary64 exactly, so the value
 *       that comes back through strtod has to carry the same bits. A single wrong digit anywhere in
 *       the run gives a different double.
 * @note Nothing here compares against a reference rendering, so the module is free to write the digits
 *       in whatever form it picks. What is checked is that the number those digits stand for is the
 *       number that went in.
 * @note The values straddle both form boundaries in both directions, cover both signs, and include
 *       the extremes a binary64 carries along with a subnormal.
 */
void test_a_double_written_at_seventeen_digits_reads_back_the_same(void)
{
    static const double value_of[] = {0.0,
                                      -0.0,
                                      1.0,
                                      -1.0,
                                      0.1,
                                      1.0 / 3.0,
                                      2.0 / 3.0,
                                      123456.7890625,
                                      1e-5,
                                      9.999e-5,
                                      1e-4,
                                      0.5,
                                      1e15,
                                      1e16,
                                      1e17,
                                      1.7976931348623157e308,
                                      2.2250738585072014e-308,
                                      5e-324,
                                      -4.9406564584124654e-324,
                                      3.141592653589793,
                                      2.718281828459045,
                                      6.02214076e23,
                                      -1.602176634e-19};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        char produced[MMGR_ACCURACY_VERBA_BUFFER];
        char message[192];

        (void)accuracy_finish_at(produced, EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = produced,
                                                      .cap = sizeof produced, .at = 0u, .real = value_of[index],
                                                      .sig = MMGR_ACCURACY_VERBA_EXACT_DIGITS));

        (void)snprintf(message, sizeof message, "the value written as \"%s\" did not read back the same", produced);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_bits_of(value_of[index]), accuracy_bits_of(strtod(produced, NULL)),
                                        message);
    }
}

/**
 * @brief Checks that a double written at fewer digits stays inside the error that count allows.
 *
 * @note Below seventeen digits the round trip cannot be exact, so the claim is the weaker one that
 *       holds at every width: the value read back differs from the original by no more than half a
 *       unit in the last digit written.
 * @note Every significant digit count from one to seventeen is offered on every value, so the bound
 *       is checked as it tightens instead of at one convenient width.
 * @note A form that dropped a digit, or rounded away from the nearest, lands outside the bound at the
 *       width where it did so and stays outside at every width past it.
 * @note Zero is left out of the ratio, since a relative error has no meaning there. It is covered by
 *       the exact case above.
 */
void test_a_double_written_at_fewer_digits_stays_inside_the_allowed_error(void)
{
    static const double value_of[] = {1.0,
                                      0.1,
                                      1.0 / 3.0,
                                      2.0 / 3.0,
                                      123456.7890625,
                                      0.5,
                                      1e-5,
                                      9.999e-5,
                                      1e15,
                                      1e17,
                                      3.141592653589793,
                                      2.718281828459045,
                                      6.02214076e23,
                                      -1.602176634e-19,
                                      -9.75,
                                      1.7976931348623157e308};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned sig = 1u; sig <= MMGR_ACCURACY_VERBA_EXACT_DIGITS; sig++)
        {
            char produced[MMGR_ACCURACY_VERBA_BUFFER];
            char message[224];
            // Explicit cast narrows the loop counter to the uint8_t the sig member holds
            const uint8_t digits = (uint8_t)sig;

            (void)accuracy_finish_at(produced, EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = produced,
                                                          .cap = sizeof produced, .at = 0u, .real = value_of[index],
                                                          .sig = digits));

            const double parsed = strtod(produced, NULL);
            const double original = value_of[index];

            // A value near the top of the range rounds up out of it at a small digit count. The
            // largest double written to one digit is 2e+308, which no double carries, so the reading
            // back overflows and there is no error to measure. That is the digits being right
            if (!isfinite(parsed))
            {
                continue;
            }

            const double difference = (parsed > original) ? (parsed - original) : (original - parsed);
            const double magnitude = (original < 0.0) ? -original : original;
            // Half a unit in the last of sig digits, which is five parts in ten to the sig
            const double allowed = magnitude * (5.0 / accuracy_power_of_ten(sig));

            (void)snprintf(message, sizeof message, "%.17g at %u digits gave \"%s\", which is too far off", original,
                           sig, produced);
            TEST_ASSERT_TRUE_MESSAGE(difference <= allowed, message);
        }
    }
}

/**
 * @brief Checks the fixed form against snprintf at every decimal count.
 *
 * @note The fixed form and the printf conversion agree on what they mean, so this is a direct
 *       comparison. The header states ties round to even, which is what the reference does under the
 *       default rounding mode.
 * @note A decimal count of zero is documented to write no point at all, which is the boundary a count
 *       taken as one-based would get wrong, and the reference writes it the same way.
 * @note Exact ties are included. The module takes a half up on the magnitude and writes the sign
 *       separately, which is away from zero, and that is what this toolchain's printf does with one
 *       too. A toolchain whose printf broke ties to even instead would disagree here, and the case
 *       below is the one that pins the rule without a formatter in it.
 */
void test_the_fixed_form_matches_the_reference(void)
{
    static const double value_of[] = {0.0, 0.5, 1.25, 12.0625, 3.0, -0.5, -12.0625, 1024.0, 0.03125};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned decimals = 0u; decimals <= 10u; decimals++)
        {
            char produced[MMGR_ACCURACY_VERBA_BUFFER];
            char reference[64];
            char message[192];
            // Explicit cast narrows the loop counter to the uint8_t the decimals member holds
            const uint8_t count = (uint8_t)decimals;

            (void)snprintf(reference, sizeof reference, "%.*f", (int)decimals, value_of[index]);
            (void)accuracy_finish_at(produced, EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = produced,
                                                          .cap = sizeof produced, .at = 0u, .real = value_of[index],
                                                          .decimals = count));
            (void)snprintf(message, sizeof message, "%f at %u decimals", value_of[index], decimals);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
        }
    }
}

/**
 * @brief Checks that an exact tie rounds up.
 *
 * @note The header states a half rounds up, and this is the case that holds it to that. Every
 *       expectation is written out by hand from the rule rather than taken from a formatter, since
 *       the C standard leaves the rule open and a library is free to pick either.
 * @note At no decimals every half moves to the next integer. That is what separates this from the
 *       tie-to-even rule it replaced, which took a half down to an even neighbor half the time.
 * @note All the values are exact in binary, so each one really is a tie and not a value that merely
 *       prints like one.
 * @note The negative ties are what show the direction. The sign is written ahead of the magnitude and
 *       the magnitude is what rounds up, so a negative half moves away from zero.
 */
void test_an_exact_tie_rounds_up(void)
{
    static const struct
    {
        double value;         /**< The tie being written. */
        uint8_t decimals;     /**< Digits after the point. */
        const char *expected; /**< What rounding to even gives [BORROWS]. */
    } tie_of[] = {
        {0.5, 0u, "1"},      {1.5, 0u, "2"},    {2.5, 0u, "3"},      {3.5, 0u, "4"},        {-0.5, 0u, "-1"},
        {-1.5, 0u, "-2"},    {-2.5, 0u, "-3"},  {0.125, 2u, "0.13"}, {0.375, 2u, "0.38"},   {0.625, 2u, "0.63"},
        {0.875, 2u, "0.88"}, {2.25, 1u, "2.3"}, {2.75, 1u, "2.8"},   {0.0625, 3u, "0.063"}, {0.1875, 3u, "0.188"},
    };
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned tie_count = (unsigned)(sizeof tie_of / sizeof tie_of[0]);

    for (unsigned index = 0u; index < tie_count; index++)
    {
        char produced[MMGR_ACCURACY_VERBA_BUFFER];
        char message[192];

        TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_is_tie(tie_of[index].value, tie_of[index].decimals),
                                      "a value listed as a tie is not one");

        (void)accuracy_finish_at(produced, EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = produced,
                                                      .cap = sizeof produced, .at = 0u, .real = tie_of[index].value,
                                                      .decimals = tie_of[index].decimals));
        (void)snprintf(message, sizeof message, "%f at %u decimals did not round to even", tie_of[index].value,
                       tie_of[index].decimals);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(tie_of[index].expected, produced, message);
    }
}

/**
 * @brief Checks the text entries against the bytes they are documented to write.
 *
 * @note The escaping entries differ from the plain one in exactly which bytes they replace, and the
 *       expected strings are written out by hand from the two formats' rules.
 * @note The counted write is offered a length shorter than the string it points at, which is what
 *       shows the count is read and the terminator is not.
 * @note The clipping write is offered a buffer too small for its text, which is the one text entry
 *       documented to write what fits instead of refusing.
 */
void test_the_text_entries_write_the_bytes_they_are_documented_to(void)
{
    char produced[MMGR_ACCURACY_VERBA_BUFFER];

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = "plain text"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("plain text", produced, "the plain write changed a byte");

    (void)accuracy_finish_at(produced,
                             EMBED_CALL(verba_textus.put_n, VerbaTextusCfg, .out = produced, .cap = sizeof produced,
                                        .at = 0u, .text = "abcdefgh", .text_len = 3u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abc", produced, "the counted write did not stop at its count");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.xml, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = "a<b>c&d\"e'f"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("a&lt;b&gt;c&amp;d&quot;e'f", produced,
                                     "the XML write did not substitute what XML requires");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.json, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = "a\"b\\c\td"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"a\\\"b\\\\c\\td\"", produced,
                                     "the JSON write did not escape what JSON requires");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.json, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = "\x01"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"\\u0001\"", produced,
                                     "a control byte with no short escape did not become a \\u escape");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.json, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = NULL));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"\"", produced, "a null string did not become an empty pair of quotes");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_textus.xml, VerbaTextusCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .text = NULL));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", produced, "a null string did not write nothing through the XML entry");

    // A capacity of eight leaves seven bytes for text once the terminator is held back
    char small[8];
    const size_t clipped = EMBED_CALL(verba_textus.put_clip, VerbaTextusCfg, .out = small, .cap = sizeof small,
                                      .at = 0u, .text = "0123456789");

    small[clipped] = '\0';
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0123456", small, "the clipping write did not fill the room it had");
}

/**
 * @brief Checks the words written for an infinity and a NaN, and the three predicates.
 *
 * @note Neither form is a number, so nothing reads them back. What is checked is the exact text the
 *       header names, lower case and unquoted.
 * @note The sign is documented to reach the infinity and not the NaN, and both are checked, since a
 *       sign written on a NaN would be neither valid nor what the header states.
 * @note The predicates are checked against the same values, including a negative zero, which the
 *       header states reports as signed because the bit is read instead of the value compared.
 */
void test_the_non_finite_forms_and_the_predicates(void)
{
    const double positive_infinity = 1.0 / 0.0;
    const double negative_infinity = -1.0 / 0.0;
    const double not_a_number = 0.0 / 0.0;
    char produced[MMGR_ACCURACY_VERBA_BUFFER];

    (void)accuracy_finish_at(produced,
                             EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = produced, .cap = sizeof produced,
                                        .at = 0u, .real = positive_infinity, .sig = 6u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("inf", produced, "a positive infinity did not write inf");

    (void)accuracy_finish_at(produced,
                             EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = produced, .cap = sizeof produced,
                                        .at = 0u, .real = negative_infinity, .sig = 6u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("-inf", produced, "a negative infinity did not write a sign");

    (void)accuracy_finish_at(produced, EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = produced,
                                                  .cap = sizeof produced, .at = 0u, .real = not_a_number, .sig = 6u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("nan", produced, "a NaN did not write nan");

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = positive_infinity),
                             "a positive infinity was not reported as one");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = negative_infinity),
                             "a negative infinity was not reported as one");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = not_a_number),
                              "a NaN was reported as an infinity");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = not_a_number),
                             "a NaN was not reported as one");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = positive_infinity),
                              "an infinity was reported as a NaN");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = 1.0),
                              "a finite value was reported as a NaN");

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = -0.0),
                             "a negative zero was not reported as signed");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = 0.0),
                              "a positive zero was reported as signed");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = negative_infinity),
                             "a negative infinity was not reported as signed");
}

/**
 * @brief Checks that a run of entries writing in turn lands each field where the last one ended.
 *
 * @note Every writing entry takes the offset to write at and reports the offset past what it wrote,
 *       which is what lets a caller chain them. A cursor off by one shows up as a lost or a doubled
 *       character at every join and nowhere else.
 * @note The run mixes text, a character and three numeric forms, so the threading is checked across
 *       entries in different tables instead of within one.
 * @note The reference is assembled by snprintf in the same order, so the comparison is between two
 *       whole strings.
 */
void test_a_run_of_entries_threads_the_cursor(void)
{
    char produced[MMGR_ACCURACY_VERBA_BUFFER];
    char reference[MMGR_ACCURACY_VERBA_BUFFER];
    size_t at = 0u;

    at = EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = produced, .cap = sizeof produced, .at = at, .text = "id=");
    at = EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = produced, .cap = sizeof produced, .at = at,
                    .val = 4211uLL);
    at = EMBED_CALL(verba_littera.ch, VerbaLitteraCfg, .out = produced, .cap = sizeof produced, .at = at, .ch = ' ');
    at = EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = produced, .cap = sizeof produced, .at = at,
                    .val = 0xBEEFuLL, .min = 8u);
    at = EMBED_CALL(verba_littera.ch, VerbaLitteraCfg, .out = produced, .cap = sizeof produced, .at = at, .ch = ' ');
    at = EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = produced, .cap = sizeof produced, .at = at,
                    .sval = -77LL);

    const size_t length = accuracy_finish_at(produced, at);

    (void)snprintf(reference, sizeof reference, "id=%llu %0*llx %lld", 4211uLL, 8, 0xBEEFuLL, -77LL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, "a run of entries did not thread the cursor");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), length, "the reported length is not the string's length");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(verba_finis.ok, VerbaFinisCfg, .cap = sizeof produced, .at = at),
                             "a run that fit reported no room left");
}
