// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

static int ref_num(int c)
{
    return c >= '0' && c <= '9';
}
static int ref_upper(int c)
{
    return c >= 'A' && c <= 'Z';
}
static int ref_lower(int c)
{
    return c >= 'a' && c <= 'z';
}
static int ref_alpha(int c)
{
    return ref_upper(c) || ref_lower(c);
}
static int ref_alnum(int c)
{
    return ref_alpha(c) || ref_num(c);
}
static int ref_hex(int c)
{
    return ref_num(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
static int ref_punct(int c)
{
    return c >= 0x21 && c <= 0x7E && !ref_alnum(c);
}
static int ref_space(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}
static int ref_print(int c)
{
    return c >= 0x20 && c <= 0x7E;
}
static int ref_ctrl(int c)
{
    return c < 0x20 || c == 0x7F;
}

// A class is named by an index, not by a mask: the masks are file local to the module's .c, so a
// suite gets at them the same way anything else does.
static void all_256(MmgrAsciiClass k, int (*ref)(int), const char *what)
{
    for (int c = 0; c < 256; c++)
    {
        // nothing at or above 0x80 belongs to any class
        const int want = (c < 128) ? ref(c) : 0;
        const int got = ascii.in(k, (uint8_t)c);
        if (got != want)
        {
            TEST_ASSERT_EQUAL_INT_MESSAGE(want, got, what);
        }
    }
}

void test_ascii_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("ascii_persona_bitorum.h compiled with no header before it");
}

void test_ascii_persona_bitorum_is_128_bits(void)
{
    TEST_ASSERT_EQUAL_size_t(16u, sizeof(MmgrAsciiMask));
}

// The enum values are API: a caller that compiled against them holds the numbers, not the names,
// and the generator builds both the enum and the mask table from one ordered list. Reordering that
// list renumbers every class silently, so the numbering is pinned here rather than trusted.
void test_ascii_class_numbering_is_pinned(void)
{
    TEST_ASSERT_EQUAL_INT(0, MMGR_ASCII_NUM);
    TEST_ASSERT_EQUAL_INT(1, MMGR_ASCII_ALPHA);
    TEST_ASSERT_EQUAL_INT(2, MMGR_ASCII_ALNUM);
    TEST_ASSERT_EQUAL_INT(3, MMGR_ASCII_UPPER);
    TEST_ASSERT_EQUAL_INT(4, MMGR_ASCII_LOWER);
    TEST_ASSERT_EQUAL_INT(5, MMGR_ASCII_HEX);
    TEST_ASSERT_EQUAL_INT(6, MMGR_ASCII_PUNCT);
    TEST_ASSERT_EQUAL_INT(7, MMGR_ASCII_SPACE);
    TEST_ASSERT_EQUAL_INT(8, MMGR_ASCII_CTRL);
    TEST_ASSERT_EQUAL_INT(9, MMGR_ASCII_PRINT);
    TEST_ASSERT_EQUAL_INT(10, MMGR_ASCII_CLASSES);
}

void test_ascii_num(void)
{
    all_256(MMGR_ASCII_NUM, ref_num, "num");
}
void test_ascii_alpha(void)
{
    all_256(MMGR_ASCII_ALPHA, ref_alpha, "alpha");
}
void test_ascii_alnum(void)
{
    all_256(MMGR_ASCII_ALNUM, ref_alnum, "alnum");
}
void test_ascii_upper(void)
{
    all_256(MMGR_ASCII_UPPER, ref_upper, "upper");
}
void test_ascii_lower(void)
{
    all_256(MMGR_ASCII_LOWER, ref_lower, "lower");
}
void test_ascii_hex(void)
{
    all_256(MMGR_ASCII_HEX, ref_hex, "hex");
}
void test_ascii_punct(void)
{
    all_256(MMGR_ASCII_PUNCT, ref_punct, "punct");
}
void test_ascii_space(void)
{
    all_256(MMGR_ASCII_SPACE, ref_space, "space");
}
void test_ascii_print(void)
{
    all_256(MMGR_ASCII_PRINT, ref_print, "print");
}
void test_ascii_ctrl(void)
{
    all_256(MMGR_ASCII_CTRL, ref_ctrl, "ctrl");
}

void test_ascii_high_bytes_are_in_no_class(void)
{
    for (int c = 128; c < 256; c++)
    {
        TEST_ASSERT_FALSE(ascii.in(MMGR_ASCII_PRINT, (uint8_t)c));
        TEST_ASSERT_FALSE(ascii.in(MMGR_ASCII_CTRL, (uint8_t)c));
        TEST_ASSERT_FALSE(ascii.in(MMGR_ASCII_ALNUM, (uint8_t)c));
    }
}

void test_ascii_classes_partition_the_printables(void)
{
    // every printable byte is exactly one of alnum, punct or space
    for (int c = 0x20; c <= 0x7E; c++)
    {
        const int n = ascii.in(MMGR_ASCII_ALNUM, (uint8_t)c) + ascii.in(MMGR_ASCII_PUNCT, (uint8_t)c) +
                      ascii.in(MMGR_ASCII_SPACE, (uint8_t)c);
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, n, "a printable byte belongs to exactly one of alnum, punct, space");
    }
}
