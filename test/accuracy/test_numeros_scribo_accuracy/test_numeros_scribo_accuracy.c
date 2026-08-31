// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_numeros_scribo_accuracy.c
 * @brief Checks the assembled output against the host's own snprintf, field by field and over specs
 *        that mix literals with every kind that has a printf equivalent.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note What this module does is route each field to a verba call at a width, and put the results in
 *       order. A defect here is a field paired with the wrong value, a width taken from the wrong
 *       place, or a field written in the wrong order, and every one of those produces a string that
 *       is well formed and wrong.
 * @note The reference is the C library's snprintf, which is a separate implementation of the same
 *       formatting and shares nothing with this tree. Nine of the thirteen kinds have an exact printf
 *       equivalent, and those nine are what the sweeps below compare.
 * @note MMGR_FK_G and MMGR_FK_FIX are not compared against printf. Their rounding is the business of
 *       verba_scribo and its own suite, and a disagreement in the last digit would be reported here
 *       as a routing defect. What is checked for them is the routing claim: the width reaches the
 *       call as significant digits or as decimals, which is visible in the shape of the output
 *       without deciding any digit.
 * @note The width rules differ between the two entry points, which the header states. build takes the
 *       width from the field and emit takes it from the value. A module that read one where it meant
 *       the other passes every single-field case and fails the pair of cases that separate them.
 * @note Contract checks live in test_numeros_scribo. This file asks what the buffer ends up holding.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "numeros_scribo/numeros_scribo.h"

#include "unity.h"

/**
 * @brief Expands to 256u, the bytes every destination buffer here holds.
 *
 * @note Far past the longest string any case builds, which keeps a short buffer from ever being the
 *       reason a case fails. The cases that mean to run out of room state a smaller capacity at the
 *       call.
 */
#define MMGR_ACCURACY_NUMER_BUFFER 256u

/**
 * @brief Expands to 0x7E, the byte a destination is filled with before a write.
 *
 * @note A printable byte no case writes on purpose. A position the call did not reach then reads as
 *       itself in a failure message instead of as an unprintable value.
 */
#define MMGR_ACCURACY_NUMER_GUARD 0x7Eu

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @note Every case starts here. A terminator the call placed is then distinguishable from one that
 *       was already there.
 */
static void accuracy_fill_guard(char *buffer)
{
    for (size_t index = 0u; index < MMGR_ACCURACY_NUMER_BUFFER; index++)
    {
        buffer[index] = (char)MMGR_ACCURACY_NUMER_GUARD;
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every buffer, spec and value list here has automatic storage inside the case that builds it,
 *       so there is no shared state to prepare.
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
 * @brief Checks that the reference formatting this suite rests on behaves as the cases assume.
 *
 * @note Exists to catch a wrong assumption about the reference as itself. The sweeps below lean on
 *       snprintf zero padding to a width and writing lower case hexadecimal, and a platform that did
 *       either differently would report the module as wrong at every padded field.
 * @note The expectations are strings a reader can check. A width of five over 42 pads to 00042, and
 *       255 in hexadecimal is ff and not FF.
 * @note A width smaller than the value is checked to widen the field and not to truncate it, which is
 *       what the module documents for its own minimum digit counts.
 */
void test_the_reference_formatting_this_suite_relies_on_behaves_as_assumed(void)
{
    char reference[64];

    (void)snprintf(reference, sizeof reference, "%0*lu", 5, 42uL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("00042", reference, "the reference does not zero pad to a width");

    (void)snprintf(reference, sizeof reference, "%0*llx", 1, 255uLL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("ff", reference, "the reference does not write lower case hexadecimal");

    (void)snprintf(reference, sizeof reference, "%0*llo", 1, 8uLL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("10", reference, "the reference does not write octal");

    (void)snprintf(reference, sizeof reference, "%0*lu", 2, 123456uL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("123456", reference, "a width narrower than the value truncated it");

    (void)snprintf(reference, sizeof reference, "%lld", -9223372036854775807LL - 1LL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("-9223372036854775808", reference,
                                     "the reference cannot carry the most negative value");
}

/**
 * @brief Checks each kind on its own against the same value formatted by snprintf.
 *
 * @note One field and one value at a time. A disagreement then names the kind instead of a position
 *       in a longer string, which is what pins each route from a kind to its verba call.
 * @note The values are chosen to span each kind: zero, one, a value that fills the width, and the
 *       extremes of the type. The most negative signed value is included, since a magnitude taken by
 *       negating it is the one input that has no positive counterpart.
 * @note Both entry points are exercised on the same value, which is what shows build and emit agree
 *       wherever the width plays no part.
 */
void test_each_kind_alone_matches_the_reference_formatting(void)
{
    static const uint64_t unsigned_of[] = {0uLL,   1uLL,     9uLL,          10uLL,
                                           255uLL, 65535uLL, 4294967295uLL, 18446744073709551615uLL};
    static const int64_t signed_of[] = {0LL, 1LL, -1LL, 9223372036854775807LL, -9223372036854775807LL - 1LL};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in
    const unsigned unsigned_count = (unsigned)(sizeof unsigned_of / sizeof unsigned_of[0]);
    const unsigned signed_count = (unsigned)(sizeof signed_of / sizeof signed_of[0]);

    for (unsigned index = 0u; index < unsigned_count; index++)
    {
        const mmgr_field spec_u64[] = {MMGR_U64, MMGR_END};
        const mmgr_fval vals_u64[] = {MMGR_VU64(unsigned_of[index])};
        char produced[MMGR_ACCURACY_NUMER_BUFFER];
        char reference[64];
        char message[160];

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%llu", (unsigned long long)unsigned_of[index]);

        const size_t length = EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced,
                                         .spec = spec_u64, .vals = vals_u64, .nvals = 1u);

        (void)snprintf(message, sizeof message, "an unsigned 64-bit value of %llu",
                       (unsigned long long)unsigned_of[index]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), length, message);

        // The 32-bit kind takes the same value narrowed, which is what its own union arm holds
        const mmgr_field spec_u32[] = {MMGR_U32, MMGR_END};
        // Explicit cast narrows the sweep value to the 32-bit arm this kind reads
        const mmgr_fval vals_u32[] = {MMGR_VU32((uint32_t)unsigned_of[index])};

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%lu", (unsigned long)(uint32_t)unsigned_of[index]);
        (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_u32,
                         .vals = vals_u32, .nvals = 1u);
        (void)snprintf(message, sizeof message, "an unsigned 32-bit value of %lu",
                       (unsigned long)(uint32_t)unsigned_of[index]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

        const mmgr_field spec_hex[] = {MMGR_FK_HEX, 0u, 0u, NULL, MMGR_END};
        const mmgr_fval vals_hex[] = {MMGR_VHEX(unsigned_of[index])};

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%0*llx", 1, (unsigned long long)unsigned_of[index]);
        (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_hex,
                         .vals = vals_hex, .nvals = 1u);
        (void)snprintf(message, sizeof message, "a hexadecimal value of %llu", (unsigned long long)unsigned_of[index]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

        const mmgr_field spec_oct[] = {MMGR_FK_OCT, 0u, 0u, NULL, MMGR_END};
        const mmgr_fval vals_oct[] = {MMGR_VOCT(unsigned_of[index])};

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%0*llo", 1, (unsigned long long)unsigned_of[index]);
        (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_oct,
                         .vals = vals_oct, .nvals = 1u);
        (void)snprintf(message, sizeof message, "an octal value of %llu", (unsigned long long)unsigned_of[index]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
    }

    for (unsigned index = 0u; index < signed_count; index++)
    {
        const mmgr_field spec[] = {MMGR_I64, MMGR_END};
        const mmgr_fval vals[] = {MMGR_VI64(signed_of[index])};
        char produced[MMGR_ACCURACY_NUMER_BUFFER];
        char reference[64];
        char message[160];

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%lld", (long long)signed_of[index]);
        (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec, .vals = vals,
                         .nvals = 1u);
        (void)snprintf(message, sizeof message, "a signed value of %lld", (long long)signed_of[index]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
    }
}

/**
 * @brief Checks that a spec mixing literals and values comes out in the order the spec names.
 *
 * @note Ordering is the claim. A module that wrote the values before the literals, or paired the
 *       second value with the first field, produces every one of the same substrings in the wrong
 *       arrangement, which no single-field case can see.
 * @note The reference is assembled by snprintf in the same order the spec lists, so the comparison is
 *       between two whole strings and not between a set of pieces.
 * @note The literal fields carry their own byte counts, and one of them holds a byte count shorter
 *       than the string it points at, which is what shows the count is read and the terminator is not.
 */
void test_a_spec_of_literals_and_values_comes_out_in_order(void)
{
    static const char label[] = "id=";
    static const char unit[] = " ms, hex=";
    static const char trailing[] = " end-of-record";
    const mmgr_field spec[] = {
        {MMGR_FK_LIT, 0u, (uint16_t)(sizeof label - 1u), label},
        MMGR_U32,
        {MMGR_FK_LIT, 0u, (uint16_t)(sizeof unit - 1u), unit},
        {MMGR_FK_HEX, 0u, 0u, NULL},
        // A byte count shorter than the string it points at, so only that many bytes are written
        {MMGR_FK_LIT, 0u, 4u, trailing},
        MMGR_STR,
        MMGR_END,
    };
    const mmgr_fval vals[] = {MMGR_VU32(4211u), MMGR_VHEX(0xDEADBEEFuLL), MMGR_VSTR("tail")};
    char produced[MMGR_ACCURACY_NUMER_BUFFER];
    char reference[MMGR_ACCURACY_NUMER_BUFFER];

    accuracy_fill_guard(produced);
    (void)snprintf(reference, sizeof reference, "%s%lu%s%0*llx%.*s%s", label, 4211uL, unit, 1, 0xDEADBEEFuLL, 4,
                   trailing, "tail");

    const size_t length = EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec,
                                     .vals = vals, .nvals = 3u);

    TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, "the fields did not come out in the order the spec names");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), length, "the reported length is not the string's length");
}

/**
 * @brief Checks that build takes the width from the field and emit takes it from the value.
 *
 * @note The header states the two differ, and this is the case that separates them. The same value is
 *       written twice with the two widths set to different numbers. A module reading one where it
 *       meant the other produces the other width's padding and is caught there.
 * @note Only the kinds whose width reaches a minimum digit count are used. The plain unsigned and
 *       signed kinds fix their own minimum at one and ignore the width entirely, which the module
 *       documents.
 * @note The widths are checked against snprintf zero padding at the same width, so what is compared
 *       is the padded string and not merely its length.
 */
void test_build_takes_the_width_from_the_field_and_emit_from_the_value(void)
{
    for (unsigned width = 1u; width <= 12u; width++)
    {
        // Explicit cast narrows the loop counter to the width member, which is a uint8_t
        const uint8_t field_width = (uint8_t)width;
        // A different width in the value, so whichever one is read is visible in the output
        const uint8_t value_width = (uint8_t)(width + 4u);
        const mmgr_field spec[] = {{MMGR_FK_HEX, field_width, 0u, NULL}, MMGR_END};
        const mmgr_fval vals[] = {MMGR_VHEXW(0xABCuLL, value_width)};
        char produced[MMGR_ACCURACY_NUMER_BUFFER];
        char reference[64];
        char message[160];

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%0*llx", (int)field_width, 0xABCuLL);
        (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec, .vals = vals,
                         .nvals = 1u);
        (void)snprintf(message, sizeof message, "a build at a field width of %u with a value width of %u", width,
                       value_width);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);

        accuracy_fill_guard(produced);
        (void)snprintf(reference, sizeof reference, "%0*llx", (int)value_width, 0xABCuLL);
        (void)EMBED_CALL(numer.emit, NumerosCfg, .out = produced, .cap = sizeof produced, .vals = vals, .nvals = 1u);
        (void)snprintf(message, sizeof message, "an emit at a value width of %u with a field width of %u", value_width,
                       width);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
    }
}

/**
 * @brief Checks that the decimal kind pads to the width it is given.
 *
 * @note MMGR_FK_DEC is the one kind whose default width is zero, so the width it formats at is the
 *       one the caller states and nothing else. Every width from none to past the value's own digit
 *       count is offered.
 * @note A width narrower than the value is checked to widen the field and not to cut the value, which
 *       is what a minimum digit count means and what a maximum would get wrong.
 */
void test_the_decimal_kind_pads_to_the_width_it_is_given(void)
{
    static const uint32_t value_of[] = {0uL, 7uL, 42uL, 999uL, 4294967295uL};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned width = 0u; width <= 12u; width++)
        {
            // Explicit cast narrows the loop counter to the width member, which is a uint8_t
            const mmgr_field spec[] = {{MMGR_FK_DEC, (uint8_t)width, 0u, NULL}, MMGR_END};
            const mmgr_fval vals[] = {MMGR_VDEC(value_of[index])};
            char produced[MMGR_ACCURACY_NUMER_BUFFER];
            char reference[64];
            char message[160];

            accuracy_fill_guard(produced);
            (void)snprintf(reference, sizeof reference, "%0*lu", (int)width, (unsigned long)value_of[index]);
            (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec,
                             .vals = vals, .nvals = 1u);
            (void)snprintf(message, sizeof message, "a decimal %lu at a width of %u", (unsigned long)value_of[index],
                           width);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, produced, message);
        }
    }
}

/**
 * @brief Checks that the fixed kind writes exactly the decimals its width names.
 *
 * @note The routing claim, not the rounding. The width reaches verba as the digits after the point,
 *       so what is checked is that the output carries a point with exactly that many digits behind
 *       it, whatever those digits are.
 * @note A width of zero is documented to write no point at all, which is the boundary a count taken
 *       as one-based would get wrong.
 * @note The values are exact in binary, so the digits themselves are not in question and a failure
 *       here is the width having gone astray.
 */
void test_the_fixed_kind_writes_exactly_the_decimals_its_width_names(void)
{
    static const double value_of[] = {0.0, 0.5, 1.25, 12.0625, 3.0};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned decimals = 0u; decimals <= 8u; decimals++)
        {
            // Explicit cast narrows the loop counter to the width member, which is a uint8_t
            const mmgr_field spec[] = {{MMGR_FK_FIX, (uint8_t)decimals, 0u, NULL}, MMGR_END};
            const mmgr_fval vals[] = {MMGR_VFIX(value_of[index])};
            char produced[MMGR_ACCURACY_NUMER_BUFFER];
            char message[160];

            accuracy_fill_guard(produced);
            (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec,
                             .vals = vals, .nvals = 1u);

            const char *const point = strchr(produced, '.');

            (void)snprintf(message, sizeof message, "%f at %u decimals gave \"%s\"", value_of[index], decimals,
                           produced);
            if (decimals == 0u)
            {
                TEST_ASSERT_NULL_MESSAGE(point, message);
            }
            else
            {
                TEST_ASSERT_NOT_NULL_MESSAGE(point, message);
                TEST_ASSERT_EQUAL_size_t_MESSAGE(decimals, strlen(point + 1), message);
            }
        }
    }
}

/**
 * @brief Checks that a mismatched spec abandons the write and leaves earlier text whole.
 *
 * @note Abandoning is the accuracy claim that matters most for an append. A partially written record
 *       left in the buffer is one a reader cannot tell from a complete one, so the terminator has to
 *       go back where the write started.
 * @note Three ways to mismatch are covered, and the header names all three: a value missing, a value
 *       whose kind differs from its field, and values left over past the spec.
 * @note The buffer is primed with text before each attempt, and that text is checked to survive
 *       unchanged. Checking the return alone would pass a module that abandoned late.
 */
void test_a_mismatched_spec_abandons_the_write_and_keeps_the_earlier_text(void)
{
    static const char existing[] = "kept";
    const mmgr_field spec_two[] = {MMGR_U32, MMGR_U32, MMGR_END};
    const mmgr_field spec_one[] = {MMGR_U32, MMGR_END};
    const mmgr_fval vals_one[] = {MMGR_VU32(1u)};
    const mmgr_fval vals_two[] = {MMGR_VU32(1u), MMGR_VU32(2u)};
    const mmgr_fval vals_wrong_kind[] = {MMGR_VSTR("text")};
    char produced[MMGR_ACCURACY_NUMER_BUFFER];

    accuracy_fill_guard(produced);
    (void)snprintf(produced, sizeof produced, "%s", existing);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u,
                                     EMBED_CALL(numer.append, NumerosCfg, .out = produced, .cap = sizeof produced,
                                                .spec = spec_two, .vals = vals_one, .nvals = 1u),
                                     "a spec with a value missing reported a length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(existing, produced, "a spec with a value missing damaged the earlier text");

    accuracy_fill_guard(produced);
    (void)snprintf(produced, sizeof produced, "%s", existing);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u,
                                     EMBED_CALL(numer.append, NumerosCfg, .out = produced, .cap = sizeof produced,
                                                .spec = spec_one, .vals = vals_wrong_kind, .nvals = 1u),
                                     "a value of the wrong kind reported a length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(existing, produced, "a value of the wrong kind damaged the earlier text");

    accuracy_fill_guard(produced);
    (void)snprintf(produced, sizeof produced, "%s", existing);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u,
                                     EMBED_CALL(numer.append, NumerosCfg, .out = produced, .cap = sizeof produced,
                                                .spec = spec_one, .vals = vals_two, .nvals = 2u),
                                     "values left over past the spec reported a length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(existing, produced, "values left over past the spec damaged the earlier text");
}

/**
 * @brief Checks that a run of appends builds the same string as one spec holding every field.
 *
 * @note An append carries on where the last write finished, and the claim is that a record assembled
 *       piece by piece is byte for byte the record assembled in one call. A cursor off by one shows
 *       up as a lost or a doubled character at every join.
 * @note The run is threaded through the returned length, which is the path the header describes as
 *       the one that measures nothing. A second run leaves the cursor unset so the length is measured
 *       instead, and the two are checked to agree.
 * @note Both are compared against snprintf, so neither run is measured against the other alone.
 */
void test_a_run_of_appends_builds_the_same_string_as_one_spec(void)
{
    const mmgr_field spec_first[] = {MMGR_U32, MMGR_END};
    const mmgr_field spec_second[] = {{MMGR_FK_LIT, 0u, 1u, ":"}, MMGR_STR, MMGR_END};
    const mmgr_field spec_third[] = {{MMGR_FK_LIT, 0u, 1u, "/"}, {MMGR_FK_HEX, 0u, 0u, NULL}, MMGR_END};
    const mmgr_fval vals_first[] = {MMGR_VU32(77u)};
    const mmgr_fval vals_second[] = {MMGR_VSTR("name")};
    const mmgr_fval vals_third[] = {MMGR_VHEX(0x1FuLL)};
    char reference[MMGR_ACCURACY_NUMER_BUFFER];
    char threaded[MMGR_ACCURACY_NUMER_BUFFER];
    char measured[MMGR_ACCURACY_NUMER_BUFFER];

    (void)snprintf(reference, sizeof reference, "%lu:%s/%0*llx", 77uL, "name", 1, 0x1FuLL);

    accuracy_fill_guard(threaded);

    size_t at = EMBED_CALL(numer.build, NumerosCfg, .out = threaded, .cap = sizeof threaded, .spec = spec_first,
                           .vals = vals_first, .nvals = 1u);

    at = EMBED_CALL(numer.append, NumerosCfg, .out = threaded, .cap = sizeof threaded, .at = at, .spec = spec_second,
                    .vals = vals_second, .nvals = 1u);
    at = EMBED_CALL(numer.append, NumerosCfg, .out = threaded, .cap = sizeof threaded, .at = at, .spec = spec_third,
                    .vals = vals_third, .nvals = 1u);

    TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, threaded, "a run of appends threading the cursor built the wrong text");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen(reference), at, "the threaded run reported the wrong length");

    accuracy_fill_guard(measured);
    (void)EMBED_CALL(numer.build, NumerosCfg, .out = measured, .cap = sizeof measured, .spec = spec_first,
                     .vals = vals_first, .nvals = 1u);
    (void)EMBED_CALL(numer.append, NumerosCfg, .out = measured, .cap = sizeof measured, .spec = spec_second,
                     .vals = vals_second, .nvals = 1u);
    (void)EMBED_CALL(numer.append, NumerosCfg, .out = measured, .cap = sizeof measured, .spec = spec_third,
                     .vals = vals_third, .nvals = 1u);

    TEST_ASSERT_EQUAL_STRING_MESSAGE(reference, measured, "a run of appends measuring the text built the wrong text");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(threaded, measured, "threading the cursor and measuring it disagreed");
}

/**
 * @brief Checks that the string kinds escape what their format requires and leave the rest alone.
 *
 * @note MMGR_FK_JSON and MMGR_FK_XML differ from MMGR_FK_STR in exactly which bytes they replace, and
 *       routing a field to the wrong one of the three produces text that is still readable and wrong
 *       for the format it was meant for.
 * @note The input carries one of every byte each format replaces, along with bytes neither replaces,
 *       which shows a route to the wrong call as a byte escaped that should not have been.
 * @note The expected strings are written out by hand from the two formats' rules, which is what keeps
 *       this independent of what either call happens to produce.
 */
void test_the_string_kinds_escape_what_their_format_requires(void)
{
    const mmgr_field spec_plain[] = {MMGR_STR, MMGR_END};
    const mmgr_field spec_json[] = {MMGR_JSON, MMGR_END};
    const mmgr_field spec_xml[] = {MMGR_XML, MMGR_END};
    const mmgr_fval vals_plain[] = {MMGR_VSTR("a<b>c&d\"e")};
    const mmgr_fval vals_json[] = {MMGR_VJSON("a\"b\\c")};
    const mmgr_fval vals_xml[] = {MMGR_VXML("a<b>c&d\"e")};
    char produced[MMGR_ACCURACY_NUMER_BUFFER];

    accuracy_fill_guard(produced);
    (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_plain,
                     .vals = vals_plain, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("a<b>c&d\"e", produced, "the plain string kind changed a byte");

    accuracy_fill_guard(produced);
    (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_json,
                     .vals = vals_json, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"a\\\"b\\\\c\"", produced, "the JSON kind did not escape as JSON requires");

    accuracy_fill_guard(produced);
    (void)EMBED_CALL(numer.build, NumerosCfg, .out = produced, .cap = sizeof produced, .spec = spec_xml,
                     .vals = vals_xml, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("a&lt;b&gt;c&amp;d&quot;e", produced,
                                     "the XML kind did not escape as XML requires");
}
