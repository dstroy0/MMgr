// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_ascii_persona_bitorum_accuracy.c
 * @brief Checks every class bitmap against range comparisons written out from the class list in
 *        ascii_persona_bitorum.h, over all 256 byte values.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The module is a bitmap lookup, and a bitmap is 128 independent bits per class. One wrong bit
 *       is one wrong byte out of 128 and shows up nowhere else, so every byte of every class is
 *       tested and none is sampled.
 * @note The reference is a chain of range compares on the code point, which is the construction the
 *       bitmap replaced. It reads no part of s_class and shares no arithmetic with the module.
 * @note Each range below is transcribed from the class list in the header, written as the numbers
 *       ASCII gives those characters. The first case ties those numbers back to the characters the
 *       header names.
 * @note Contract checks on a class at or above MMGR_ASCII_CLASSES live in test_ascii_persona_bitorum.
 *       This file asks which code points are in which class.
 */
#include <stdint.h>
#include <stdio.h>

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

#include "unity.h"

/**
 * @brief Expands to 256u, the number of values a byte holds and the width of every sweep here.
 *
 * @note The sweep runs past 0x7F on purpose. The half above it is where the module returns
 *       EMBED_FALSE without reading a bitmap, and that half is a claim like any other.
 */
#define MMGR_ACCURACY_ASCII_BYTES 256u

/**
 * @brief Expands to 0x80u, the first code point no class covers.
 *
 * @note Sixteen bytes of bitmap reach code point 127. This is the one past it.
 */
#define MMGR_ACCURACY_ASCII_LIMIT 0x80u

/**
 * @brief One class, its name for a failure message, and how many code points it holds.
 *
 * @note The count is a second statement about the same bitmap, arrived at by adding up the lengths
 *       of the runs in the header. A bitmap with one extra bit set matches every range test that
 *       looks at the bytes the class names and still fails the count.
 */
typedef struct
{
    MmgrAsciiClass kind; /**< Class this row describes. */
    const char *name;    /**< Enumerator name, printed when a case fails. */
    unsigned population; /**< Code points the class holds, counted from the header's runs. */
} AccuracyClassRow;

/**
 * @brief Every class, in enumerator order, with the population each one is expected to hold.
 *
 * @note MMGR_ASCII_CLASSES is absent. It counts the enumerators and is not a class.
 * @note The populations are the run lengths added up: 10 digits, 26 letters a case, 15 + 7 + 6 + 4
 *       punctuation, 5 + 1 whitespace, 32 + 1 control, and 126 - 32 + 1 printable.
 */
static const AccuracyClassRow accuracy_class_of[] = {
    {MMGR_ASCII_NUM, "MMGR_ASCII_NUM", 10u},     {MMGR_ASCII_ALPHA, "MMGR_ASCII_ALPHA", 52u},
    {MMGR_ASCII_ALNUM, "MMGR_ASCII_ALNUM", 62u}, {MMGR_ASCII_UPPER, "MMGR_ASCII_UPPER", 26u},
    {MMGR_ASCII_LOWER, "MMGR_ASCII_LOWER", 26u}, {MMGR_ASCII_HEX, "MMGR_ASCII_HEX", 22u},
    {MMGR_ASCII_PUNCT, "MMGR_ASCII_PUNCT", 32u}, {MMGR_ASCII_SPACE, "MMGR_ASCII_SPACE", 6u},
    {MMGR_ASCII_CTRL, "MMGR_ASCII_CTRL", 33u},   {MMGR_ASCII_PRINT, "MMGR_ASCII_PRINT", 95u},
};

/**
 * @brief Expands to the number of rows in accuracy_class_of.
 *
 * @note Every sweep walks the rows. The assertion below holds this count at one row per class.
 */
#define MMGR_ACCURACY_ASCII_ROWS (sizeof accuracy_class_of / sizeof accuracy_class_of[0])

/**
 * @brief Asserts there is one row per class enumerator.
 *
 * @note A class added to MmgrAsciiClass without a row here would go untested, and every sweep below
 *       would keep passing while covering one class fewer than the module ships.
 * @note Both sides are known at compile time, so the build refuses the mismatch and no case has to
 *       run to find it.
 */
EMBED_STATIC_ASSERT(MMGR_ACCURACY_ASCII_ROWS == (size_t)MMGR_ASCII_CLASSES,
                    "every ASCII class needs a row in accuracy_class_of");

/**
 * @brief Returns whether a code point falls inside a closed range.
 *
 * @param[in] code  Code point to test.
 * @param[in] first Lowest code point in the range.
 * @param[in] last  Highest code point in the range, included.
 * @return          1 inside the range, 0 outside it.
 * @note Both ends are included, which is how every run in the header is written.
 */
static int accuracy_range_holds_code(unsigned code, unsigned first, unsigned last)
{
    return ((code >= first) && (code <= last)) ? 1 : 0;
}

/**
 * @brief Returns whether a code point belongs to a class, from the runs the header names.
 *
 * @param[in] kind Class to test against.
 * @param[in] code Code point to look up, 0 through 255.
 * @return         1 when the class covers the code point, 0 when it does not.
 * @note This is the whole reference. Each arm is the header's run list written as ASCII numbers, and
 *       no arm consults another: MMGR_ASCII_ALNUM lists its three runs out in full instead of
 *       reaching for the digit and letter arms, so one wrong bitmap cannot hide behind another.
 * @note Code points at or above MMGR_ACCURACY_ASCII_LIMIT return 0 for every class, which is the
 *       coverage the header states.
 * @warning A kind of MMGR_ASCII_CLASSES returns 0. Nothing here passes it, and the module documents
 *          it as out of range.
 */
static int accuracy_class_holds_code(MmgrAsciiClass kind, unsigned code)
{
    if (code >= MMGR_ACCURACY_ASCII_LIMIT)
    {
        return 0;
    }

    switch (kind)
    {
    case MMGR_ASCII_NUM:
        return accuracy_range_holds_code(code, 48u, 57u);
    case MMGR_ASCII_ALPHA:
        return accuracy_range_holds_code(code, 65u, 90u) || accuracy_range_holds_code(code, 97u, 122u);
    case MMGR_ASCII_ALNUM:
        return accuracy_range_holds_code(code, 48u, 57u) || accuracy_range_holds_code(code, 65u, 90u) ||
               accuracy_range_holds_code(code, 97u, 122u);
    case MMGR_ASCII_UPPER:
        return accuracy_range_holds_code(code, 65u, 90u);
    case MMGR_ASCII_LOWER:
        return accuracy_range_holds_code(code, 97u, 122u);
    case MMGR_ASCII_HEX:
        return accuracy_range_holds_code(code, 48u, 57u) || accuracy_range_holds_code(code, 65u, 70u) ||
               accuracy_range_holds_code(code, 97u, 102u);
    case MMGR_ASCII_PUNCT:
        return accuracy_range_holds_code(code, 33u, 47u) || accuracy_range_holds_code(code, 58u, 64u) ||
               accuracy_range_holds_code(code, 91u, 96u) || accuracy_range_holds_code(code, 123u, 126u);
    case MMGR_ASCII_SPACE:
        return accuracy_range_holds_code(code, 9u, 13u) || accuracy_range_holds_code(code, 32u, 32u);
    case MMGR_ASCII_CTRL:
        return accuracy_range_holds_code(code, 0u, 31u) || accuracy_range_holds_code(code, 127u, 127u);
    case MMGR_ASCII_PRINT:
        return accuracy_range_holds_code(code, 32u, 126u);
    case MMGR_ASCII_CLASSES:
    default:
        break;
    }
    return 0;
}

/**
 * @brief Returns what the module reports for a class and a byte, as a 0 or a 1.
 *
 * @param[in] kind Class to test against.
 * @param[in] code Code point to look up, 0 through 255.
 * @return         1 when ascii.in reported membership, 0 when it did not.
 * @note Flattens the embed_bool to the same 0 or 1 accuracy_class_holds_code returns. The comparison is then
 *       between two memberships and not between two containers.
 */
static int accuracy_module_membership(MmgrAsciiClass kind, unsigned code)
{
    // Explicit cast narrows the sweep counter to the byte the entry takes. Every caller holds it
    // below MMGR_ACCURACY_ASCII_BYTES
    return (EMBED_CALL(ascii.in, AsciiCfg, .kind = kind, .byte = (uint8_t)code) != EMBED_FALSE) ? 1 : 0;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note The bitmaps are initialized data and no case writes anything, so there is no state to
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
 * @brief Checks that the numbers this suite tests with are the characters the header names.
 *
 * @note Exists to catch a defect in the reference as itself. Every range in accuracy_class_holds_code is
 *       written as a number, and the header describes the same runs as characters. On a host whose
 *       execution character set is not ASCII those two descriptions part company, and every case
 *       below would then compare the module against ranges nobody meant.
 * @note Each run is pinned at both ends. A character set that moved a range without resizing it is
 *       caught here as well as one that resized it.
 * @note The count of letters is checked against the endpoints. A set that spaced the letters out
 *       would keep 'A' at 65 and 'Z' somewhere above 90.
 */
void test_the_code_points_this_suite_tests_with_are_the_characters_the_header_names(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(48, '0', "the digit run does not start at 48");
    TEST_ASSERT_EQUAL_INT_MESSAGE(57, '9', "the digit run does not end at 57");
    TEST_ASSERT_EQUAL_INT_MESSAGE(65, 'A', "the upper case run does not start at 65");
    TEST_ASSERT_EQUAL_INT_MESSAGE(70, 'F', "the upper case hexadecimal run does not end at 70");
    TEST_ASSERT_EQUAL_INT_MESSAGE(90, 'Z', "the upper case run does not end at 90");
    TEST_ASSERT_EQUAL_INT_MESSAGE(97, 'a', "the lower case run does not start at 97");
    TEST_ASSERT_EQUAL_INT_MESSAGE(102, 'f', "the lower case hexadecimal run does not end at 102");
    TEST_ASSERT_EQUAL_INT_MESSAGE(122, 'z', "the lower case run does not end at 122");
    TEST_ASSERT_EQUAL_INT_MESSAGE(33, '!', "the first punctuation run does not start at 33");
    TEST_ASSERT_EQUAL_INT_MESSAGE(47, '/', "the first punctuation run does not end at 47");
    TEST_ASSERT_EQUAL_INT_MESSAGE(58, ':', "the second punctuation run does not start at 58");
    TEST_ASSERT_EQUAL_INT_MESSAGE(64, '@', "the second punctuation run does not end at 64");
    TEST_ASSERT_EQUAL_INT_MESSAGE(91, '[', "the third punctuation run does not start at 91");
    TEST_ASSERT_EQUAL_INT_MESSAGE(96, '`', "the third punctuation run does not end at 96");
    TEST_ASSERT_EQUAL_INT_MESSAGE(123, '{', "the fourth punctuation run does not start at 123");
    TEST_ASSERT_EQUAL_INT_MESSAGE(126, '~', "the fourth punctuation run does not end at 126");
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, ' ', "the space is not at 32");
}

/**
 * @brief Checks each class against the ranges the header names, at every one of the 256 byte values.
 *
 * @note This is the case the file exists for. 10 classes at 256 bytes is 2560 memberships, and every
 *       one of them is a separate bit in the data the module ships.
 * @note The failure message carries the class name and the code point. A wrong bit is located from
 *       the message alone, with nothing to narrow down by hand.
 */
void test_every_class_holds_exactly_the_code_points_its_declaration_names(void)
{
    for (size_t row = 0u; row < MMGR_ACCURACY_ASCII_ROWS; row++)
    {
        for (unsigned code = 0u; code < MMGR_ACCURACY_ASCII_BYTES; code++)
        {
            char message[96];

            (void)snprintf(message, sizeof message, "%s disagrees with its declared ranges at code point %u",
                           accuracy_class_of[row].name, code);
            TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_class_holds_code(accuracy_class_of[row].kind, code),
                                          accuracy_module_membership(accuracy_class_of[row].kind, code), message);
        }
    }
}

/**
 * @brief Checks that every byte at or above 0x80 is in no class at all.
 *
 * @note The sweep above already covers these, and this case states the rule on its own because it is
 *       the one property that holds for all ten classes at once. A bitmap grown to thirty-two bytes
 *       would fail here first and with a message naming the reason.
 * @note 128 code points at 10 classes. A single class that started reading past its sixteen bytes
 *       would report membership out of whatever followed it in memory.
 */
void test_no_byte_at_or_above_the_ascii_limit_belongs_to_any_class(void)
{
    for (size_t row = 0u; row < MMGR_ACCURACY_ASCII_ROWS; row++)
    {
        for (unsigned code = MMGR_ACCURACY_ASCII_LIMIT; code < MMGR_ACCURACY_ASCII_BYTES; code++)
        {
            char message[96];

            (void)snprintf(message, sizeof message, "%s claimed code point %u, which is past its coverage",
                           accuracy_class_of[row].name, code);
            TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(accuracy_class_of[row].kind, code), message);
        }
    }
}

/**
 * @brief Checks that each class holds the number of code points its runs add up to.
 *
 * @note A second statement about the same bitmap, and one a reader can check by hand: 26 letters in
 *       a case, 10 digits, 95 printable characters. The range sweep and this count fail on different
 *       defects, since a bit set at a code point the class never mentions passes every range compare
 *       written for the runs it does mention.
 * @note Counted over all 256 byte values. A bit set above 0x7F raises the total.
 */
void test_each_class_holds_the_number_of_code_points_its_runs_add_up_to(void)
{
    for (size_t row = 0u; row < MMGR_ACCURACY_ASCII_ROWS; row++)
    {
        unsigned counted = 0u;

        for (unsigned code = 0u; code < MMGR_ACCURACY_ASCII_BYTES; code++)
        {
            // Explicit widening of the 0 or 1 membership into the running total. Nothing else is
            // added here, so the total is the population
            counted += (unsigned)accuracy_module_membership(accuracy_class_of[row].kind, code);
        }

        char message[96];

        (void)snprintf(message, sizeof message, "%s does not hold the number of code points its runs give",
                       accuracy_class_of[row].name);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(accuracy_class_of[row].population, counted, message);
    }
}

/**
 * @brief Checks the byte on each side of every run named in the header.
 *
 * @note The sweep covers these already. They are written out again because an off-by-one at a run's
 *       end is the defect a hand-built bitmap actually acquires, and a case that names the character
 *       reports it as itself instead of as one code point out of 2560.
 * @note Each pair is the last character outside a run and the first inside it, or the reverse at the
 *       far end. '/' and '0' bracket the digits, '@' and 'A' the upper case letters, and '`' and 'a'
 *       the lower case ones, which are the three joins where the runs sit next to each other.
 * @note 'F' and 'G' and 'f' and 'g' bracket the hexadecimal letters, which is the one run that stops
 *       partway through a letter range and the one place a bitmap copied from its neighbor shows up.
 */
void test_the_byte_on_each_side_of_every_run_falls_on_the_right_side_of_it(void)
{
    // Explicit casts take each character literal from the int the language gives it to the unsigned
    // the helper counts in. Every one is a code point below 128, so no value changes crossing over
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_NUM, (unsigned)'/'), "'/' is not a digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_NUM, (unsigned)'0'), "'0' is a digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_NUM, (unsigned)'9'), "'9' is a digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_NUM, (unsigned)':'), "':' is not a digit");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_UPPER, (unsigned)'@'),
                                  "'@' is not an upper case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_UPPER, (unsigned)'A'),
                                  "'A' is an upper case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_UPPER, (unsigned)'Z'),
                                  "'Z' is an upper case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_UPPER, (unsigned)'['),
                                  "'[' is not an upper case letter");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_LOWER, (unsigned)'`'),
                                  "'`' is not a lower case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_LOWER, (unsigned)'a'),
                                  "'a' is a lower case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_LOWER, (unsigned)'z'),
                                  "'z' is a lower case letter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_LOWER, (unsigned)'{'),
                                  "'{' is not a lower case letter");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_HEX, (unsigned)'F'),
                                  "'F' is a hexadecimal digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_HEX, (unsigned)'G'),
                                  "'G' is not a hexadecimal digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_HEX, (unsigned)'f'),
                                  "'f' is a hexadecimal digit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_HEX, (unsigned)'g'),
                                  "'g' is not a hexadecimal digit");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_SPACE, 8u), "8 is not whitespace");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_SPACE, 9u), "9 is whitespace");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_SPACE, 13u), "13 is whitespace");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_SPACE, 14u), "14 is not whitespace");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_CTRL, 31u), "31 is a control code");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_CTRL, 32u), "32 is not a control code");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_CTRL, 127u), "127 is a control code");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_PRINT, 31u), "31 is not printable");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_PRINT, 32u), "32 is printable");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, accuracy_module_membership(MMGR_ASCII_PRINT, 126u), "126 is printable");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, accuracy_module_membership(MMGR_ASCII_PRINT, 127u), "127 is not printable");
}
