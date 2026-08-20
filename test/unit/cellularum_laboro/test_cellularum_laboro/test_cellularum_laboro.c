// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins what cellularum_laboro DOES, so find/agree/diff can be rewritten as a sieve and the rewrite
// proved equivalent. Every case runs both case-sensitive and case-insensitive.

#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"

#include <string.h>

#define CAP 256u

static const char *find_at(const char *hay, const char *needle, mmgr_bool ci)
{
    return cellul.find(hay, CAP, needle, CAP, ci);
}

// Offset of the hit, or -1. Easier to assert on than a pointer.
static long hit(const char *hay, const char *needle, mmgr_bool ci)
{
    const char *p = find_at(hay, needle, ci);
    return (p == NULL) ? -1L : (long)(p - hay);
}

void test_len_stops_at_nul_and_at_cap(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, cellul.len("", CAP));
    TEST_ASSERT_EQUAL_size_t(3u, cellul.len("abc", CAP));
    TEST_ASSERT_EQUAL_size_t(2u, cellul.len("abc", 2u));
}

void test_find_empty_needle_matches_at_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, hit("abc", "", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(0, hit("abc", "", MMGR_TRUE));
}

void test_find_at_every_offset(void)
{
    // One row of the sieve is enough for a single byte; this pins where it lands.
    const char *h = "0123456789abcdef0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        char one[2] = {h[i], '\0'};
        TEST_ASSERT_EQUAL_INT((long)(strchr(h, one[0]) - h), hit(h, one, MMGR_FALSE));
    }
}

void test_find_spans_a_word_boundary(void)
{
    // The needle straddles the 8-byte boundary, which is the case a word-at-a-time scan gets wrong
    // if it only ever compares aligned words.
    const char *h = "aaaaaaaXYZaaaaaaaa";
    TEST_ASSERT_EQUAL_INT(7, hit(h, "XYZ", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(7, hit(h, "xyz", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(-1, hit(h, "xyz", MMGR_FALSE));
}

void test_find_needle_longer_than_one_word(void)
{
    const char *h = "prefix_ABCDEFGHIJKLMNOP_suffix";
    TEST_ASSERT_EQUAL_INT(7, hit(h, "ABCDEFGHIJKLMNOP", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(7, hit(h, "abcdefghijklmnop", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(-1, hit(h, "ABCDEFGHIJKLMNOQ", MMGR_FALSE));
}

void test_find_needle_lengths_one_through_nine(void)
{
    // Walks the w = 1/2/4/8 ladder the current implementation selects on.
    const char *h = "____abcdefghi____";
    const char *n[9] = {"a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh", "abcdefghi"};
    for (int k = 0; k < 9; k++)
    {
        TEST_ASSERT_EQUAL_INT(4, hit(h, n[k], MMGR_FALSE));
        TEST_ASSERT_EQUAL_INT(4, hit(h, n[k], MMGR_TRUE));
    }
}

void test_find_prefers_the_first_of_several_matches(void)
{
    TEST_ASSERT_EQUAL_INT(2, hit("__ab__ab__ab", "ab", MMGR_FALSE));
}

void test_find_near_miss_shares_a_prefix(void)
{
    // Every candidate passes the first sieve row and dies on a later one.
    TEST_ASSERT_EQUAL_INT(13, hit("abXabYabZabW_abc", "abc", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(-1, hit("abXabYabZabW", "abc", MMGR_FALSE));
}

void test_find_absent(void)
{
    TEST_ASSERT_EQUAL_INT(-1, hit("the quick brown fox", "zzz", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(-1, hit("", "a", MMGR_FALSE));
}

void test_find_ci_folds_only_letters(void)
{
    // 0x20 is the case bit, so a naive fold corrupts the pairs that differ by it and are not
    // letters at all: '_' (0x5F) vs '?' (0x3F), '@' (0x40) vs '`' (0x60).
    TEST_ASSERT_EQUAL_INT(-1, hit("a_b", "a?b", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(-1, hit("a@b", "a`b", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(1, hit("_A_", "a", MMGR_TRUE));
}

void test_has_agrees_with_find(void)
{
    TEST_ASSERT_TRUE(cellul.has("hello world", CAP, "world", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(cellul.has("hello world", CAP, "WORLD", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.has("hello world", CAP, "WORLD", CAP, MMGR_TRUE));
}

void test_eq_both_cases(void)
{
    TEST_ASSERT_TRUE(cellul.eq("abc", "abc", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(cellul.eq("abc", "ABC", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.eq("abc", "ABC", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(cellul.eq("abc", "abd", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(cellul.eq("abc", "abcd", CAP, MMGR_FALSE));
}

void test_starts_both_cases(void)
{
    TEST_ASSERT_TRUE(cellul.starts("abcdef", "abc", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(cellul.starts("abcdef", "ABC", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.starts("abcdef", "ABC", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(cellul.starts("ab", "abc", CAP, MMGR_FALSE));
}

void test_diff_returns_the_first_differing_offset(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, cellul.diff("abcd", "abce", 4u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(0u, cellul.diff("Abcd", "abcd", 4u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(4u, cellul.diff("Abcd", "abcd", 4u, MMGR_TRUE));
}

void test_diff_crossing_a_word_boundary(void)
{
    // The difference sits past the first word, so a word-at-a-time compare has to carry.
    TEST_ASSERT_EQUAL_size_t(9u, cellul.diff("aaaaaaaaab", "aaaaaaaaac", 10u, MMGR_FALSE));
}

void test_copy_truncates_and_terminates(void)
{
    char dst[8];
    TEST_ASSERT_EQUAL_size_t(3u, cellul.copy(dst, "abc", sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abc", dst);
    TEST_ASSERT_EQUAL_size_t(7u, cellul.copy(dst, "abcdefghij", sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abcdefg", dst);
}

void test_classifiers(void)
{
    TEST_ASSERT_TRUE(cellul.ws(' '));
    TEST_ASSERT_TRUE(cellul.ws('\t'));
    TEST_ASSERT_FALSE(cellul.ws('a'));
    TEST_ASSERT_TRUE(cellul.digit('0'));
    TEST_ASSERT_TRUE(cellul.digit('9'));
    TEST_ASSERT_FALSE(cellul.digit('a'));
}
