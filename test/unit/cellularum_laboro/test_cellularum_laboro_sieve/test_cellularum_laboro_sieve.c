// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The sieve, with more than one row.
//
// MMGR_SIEVE_ROWS is one by default, and at one row three loops in the scanner never run: the pick
// has no earlier row to have taken a position, the reach has no second row to be further out than
// the first, and the fold has no second row to and in. All three are written for the general case
// and the default never reaches them, so raising the knob is the only way to run what is there.
//
// The knob is the module's own `#ifndef`, so this compiles the translation unit in with two rows
// rather than one. Nothing about the shipped library changes: the default is still one row, and
// the suite beside this one still tests it.
//
// What is asserted is `find`'s contract and nothing below it. Which positions the sieve picks is
// cost-driven and free to change; that a needle is found where it actually sits is not.
#define MMGR_SIEVE_ROWS 2u

#include "cellularum_laboro/cellularum_laboro.c"

#include "unity.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/** @brief find, against the two-row copy compiled into this suite. */
static const char *find_in(const char *hay, const char *needle, mmgr_bool ci)
{
    return mmgr_cellul_find(hay, strlen(hay) + 1u, needle, strlen(needle) + 1u, ci);
}

void test_two_rows_find_a_needle_late_in_a_long_haystack(void)
{
    // Long enough that the word loop runs rather than the epilogue taking the whole thing, which
    // is what puts the second row into the fold.
    static const char hay[] = "the quick brown fox jumps over the lazy dog and keeps on going past the end";
    const char *at = find_in(hay, "lazy", MMGR_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 35, at);
}

void test_two_rows_find_a_needle_of_one_repeated_byte(void)
{
    // Every position in the needle holds the same byte, so the second row's best position is one
    // the first row already took - which is the only way the pick's `taken` ever comes back set.
    static const char hay[] = "................................aaaa............................";
    const char *at = find_in(hay, "aaaa", MMGR_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 32, at);
}

void test_two_rows_find_a_needle_whose_ends_repeat(void)
{
    static const char hay[] = "----------------------------------------abcabc--------------------------";
    const char *at = find_in(hay, "abcabc", MMGR_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 40, at);
}

void test_two_rows_report_a_needle_that_is_not_there(void)
{
    static const char hay[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    TEST_ASSERT_NULL(find_in(hay, "aaab", MMGR_FALSE));
    TEST_ASSERT_NULL(find_in(hay, "zzzzzzzz", MMGR_FALSE));
}

void test_two_rows_find_a_needle_ignoring_case(void)
{
    static const char hay[] = "0123456789012345678901234567890123456789 The Lazy Dog 01234567890123456789";
    const char *at = find_in(hay, "lazy dog", MMGR_TRUE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 45, at);
}

void test_two_rows_find_a_needle_at_the_very_start(void)
{
    static const char hay[] = "needle in a haystack that runs on well past the end of the first word";

    TEST_ASSERT_EQUAL_PTR(hay, find_in(hay, "needle", MMGR_FALSE));
}

void test_two_rows_find_a_needle_of_two_bytes(void)
{
    // The shortest needle that still asks for two rows: one row per byte, and the two positions
    // are forced apart.
    static const char hay[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxqzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    const char *at = find_in(hay, "qz", MMGR_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 32, at);
}

void test_two_rows_find_a_long_needle(void)
{
    // Longer than one word, so the verify hands its remainder to diff and the reach is the verify's
    // rather than the anchor's.
    static const char hay[] = "prefix bytes here and then AAAABBBBCCCCDDDD and some trailing bytes after it";
    const char *at = find_in(hay, "AAAABBBBCCCCDDDD", MMGR_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 27, at);
}
