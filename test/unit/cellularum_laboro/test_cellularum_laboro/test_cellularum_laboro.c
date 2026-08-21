// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins what cellularum_laboro DOES, so find/agree/diff can be rewritten as a sieve and the rewrite
// proved equivalent. Every case runs both case-sensitive and case-insensitive.

#include "oracle_divergence.h"
#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

/* ---------------------------------------------------------------------------------------------
 * libc is the oracle for everything below that has one. strtol and strtod have been shipped and
 * beaten on for decades; a number parsed differently from them is a finding here, not there.
 * ------------------------------------------------------------------------------------------- */

static const char *PARSE_CASES[] = {
    "0",
    "1",
    "9",
    "10",
    "42",
    "-1",
    "-42",
    "+7",
    "007",
    "2147483647",
    "-2147483648",
    "4294967295",
    "9223372036854775807",
    "  12",
    "\t34",
    "\n56",
    "12abc",
    "abc",
    "",
    "-",
    "+",
    "  ",
    "0x10",
    "1e5",
    "999999999999999999999999",
    "-999999999999999999999999",
};

void test_to_long_matches_strtol(void)
{
    for (unsigned i = 0; i < sizeof PARSE_CASES / sizeof PARSE_CASES[0]; i++)
    {
        const char *s = PARSE_CASES[i];
        const char *mend = NULL;
        char *lend = NULL;

        const long got = cellul.to_long(s, &mend);
        errno = 0;
        const long want = strtol(s, &lend, 10);

        char msg[128];
        snprintf(msg, sizeof msg, "to_long(\"%s\")", s);
        if (errno == 0)
        {
            TEST_ASSERT_EQUAL_INT64_MESSAGE(want, got, msg);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(lend, mend, msg);
        }
    }
}

void test_to_ulong_matches_strtoul(void)
{
    for (unsigned i = 0; i < sizeof PARSE_CASES / sizeof PARSE_CASES[0]; i++)
    {
        const char *s = PARSE_CASES[i];
        const char *mend = NULL;
        char *lend = NULL;

        const unsigned long got = cellul.to_ulong(s, &mend);
        errno = 0;
        const unsigned long want = strtoul(s, &lend, 10);

        char msg[128];
        snprintf(msg, sizeof msg, "to_ulong(\"%s\")", s);
        if (errno == 0 && s[0] != '-')
        {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(want, got, msg);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(lend, mend, msg);
        }
    }
}

void test_to_long_without_an_end_pointer(void)
{
    TEST_ASSERT_EQUAL_INT64(42, cellul.to_long("42", NULL));
    TEST_ASSERT_EQUAL_UINT64(42u, cellul.to_ulong("42", NULL));
}

void test_to_double_matches_strtod(void)
{
    // exactly representable, so no tie can make the two libcs disagree
    static const char *cases[] = {"0",   "1",      "-1",      "0.5", "2.25", "-2.5", "100",  "0.125",
                                  "1.5", "  3.25", "12.5abc", "abc", "",     "-0.5", "1024", "0.0625"};

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *mend = NULL;
        char *lend = NULL;

        const double got = cellul.to_double(cases[i], &mend);
        const double want = strtod(cases[i], &lend);

        char msg[128];
        snprintf(msg, sizeof msg, "to_double(\"%s\")", cases[i]);
        TEST_ASSERT_TRUE_MESSAGE(want == got, msg);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(lend, mend, msg);
    }
}

void test_to_double_handles_an_exponent(void)
{
    const char *end = NULL;
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e3", NULL) == cellul.to_double("1e3", &end), "to_double(\"1e3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1E3", NULL) == cellul.to_double("1E3", &end), "to_double(\"1E3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e-3", NULL) == cellul.to_double("1e-3", &end), "to_double(\"1e-3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e+3", NULL) == cellul.to_double("1e+3", &end), "to_double(\"1e+3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("2.5e2", NULL) == cellul.to_double("2.5e2", &end), "to_double(\"2.5e2\")");
}

void test_to_float_matches_to_double(void)
{
    static const char *cases[] = {"0", "1", "-1", "0.5", "2.25", "1024", "0.0625"};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *end = NULL;
        TEST_ASSERT_TRUE_MESSAGE((float)strtod(cases[i], NULL) == cellul.to_float(cases[i], &end), cases[i]);
    }
}

void test_to_double_without_an_end_pointer(void)
{
    TEST_ASSERT_TRUE(2.5 == cellul.to_double("2.5", NULL));
    TEST_ASSERT_TRUE(2.5f == cellul.to_float("2.5", NULL));
}

/* ---------------------------------------------------------------------------------------------
 * the resumable compares
 * ------------------------------------------------------------------------------------------- */

static mmgr_scrut_word word_of(const char *s)
{
    return scrut.load(s);
}

void test_step_word_keeps_going_while_equal(void)
{
    const char *a = "abcdefghij";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO, cellul.step_word(word_of(a), word_of(a), MMGR_FALSE, 0),
                                  "identical words with no terminator say keep going");
}

void test_step_word_stops_on_a_difference(void)
{
    // the difference sits in lane 0 so it is inside the word at 16, 32 and 64 bits alike
    const char *a = "Xbcdefgh";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of(a), word_of(b2), MMGR_FALSE, 0));
}

void test_step_word_stops_at_the_terminator(void)
{
    // terminator in lane 0, so it is inside the word at every width
    static const char a[16] = {0, "b"[0], "c"[0], "d"[0], "e"[0], "f"[0], "g"[0], "h"[0]};
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, cellul.step_word(word_of(a), word_of(b2), MMGR_FALSE, 1),
                                  "the pattern ended first and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, cellul.step_word(word_of(a), word_of(b2), MMGR_FALSE, 0),
                                  "and without end_wins it is not");
}

void test_step_word_folds_case(void)
{
    const char *a = "ABCDEFGH";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of(a), word_of(b2), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_word(word_of(a), word_of(b2), MMGR_TRUE, 0));
}

void test_step_byte_covers_the_same_three_verdicts(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_byte('a', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('a', 'b', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_byte('\0', '\0', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, cellul.step_byte('\0', 'x', MMGR_FALSE, 1),
                                  "the pattern ended and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('\0', 'x', MMGR_FALSE, 0));

    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('A', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_byte('A', 'a', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_byte('\0', '\0', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_byte('\0', 'x', MMGR_TRUE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('\0', 'x', MMGR_TRUE, 0));
}

/* ---------------------------------------------------------------------------------------------
 * chr, against strchr
 * ------------------------------------------------------------------------------------------- */

void test_chr_matches_strchr(void)
{
    static const char *hays[] = {
        "", "a", "abc", "aaabbbccc", "the quick brown fox jumps over it", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"};

    for (unsigned h = 0; h < sizeof hays / sizeof hays[0]; h++)
    {
        const size_t cap = strlen(hays[h]) + 1u;
        for (int c = 0; c < 128; c++)
        {
            const char *got = cellul.chr(hays[h], cap, (uint8_t)c);
            const char *want = strchr(hays[h], c);
            char msg[160];
            snprintf(msg, sizeof msg, "chr(\"%s\", '%c')", hays[h], c ? c : '0');
            TEST_ASSERT_EQUAL_PTR_MESSAGE(want, got, msg);
        }
    }
}

void test_chr_at_every_alignment(void)
{
    char pad[64];
    for (unsigned off = 0; off < 8u; off++)
    {
        char *s = pad + off;
        strcpy(s, "abcdefghijklmno");
        for (int c = 'a'; c <= 'p'; c++)
        {
            TEST_ASSERT_EQUAL_PTR(strchr(s, c), cellul.chr(s, 16u, (uint8_t)c));
        }
    }
}

void test_chr_respects_the_cap(void)
{
    MMGR_SKIP_ON_ORACLE("strchr has no cap to respect");
    const char *s = "abcdef";
    TEST_ASSERT_NULL_MESSAGE(cellul.chr(s, 3u, 'f'), "f is past the cap");
    TEST_ASSERT_NOT_NULL(cellul.chr(s, 3u, 'b'));
}

/* ---------------------------------------------------------------------------------------------
 * more of find, against strstr
 * ------------------------------------------------------------------------------------------- */

void test_find_matches_strstr_over_a_corpus(void)
{
    static const char *hays[] = {
        "",
        "a",
        "ab",
        "abcabcabc",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "the quick brown fox jumps over the lazy dog, mainly on the plain",
        "aaaaaaaabaaaaaaaacaaaaaaaad",
    };
    static const char *needles[] = {"a", "b", "ab", "abc", "the", "plain", "mainly on the", "zzz", "aaaaaaaa", "dog"};

    for (unsigned h = 0; h < sizeof hays / sizeof hays[0]; h++)
    {
        const size_t hcap = strlen(hays[h]) + 1u;
        for (unsigned n = 0; n < sizeof needles / sizeof needles[0]; n++)
        {
            const size_t ncap = strlen(needles[n]) + 1u;
            const char *got = cellul.find(hays[h], hcap, needles[n], ncap, MMGR_FALSE);
            const char *want = strstr(hays[h], needles[n]);
            char msg[200];
            snprintf(msg, sizeof msg, "find(\"%s\", \"%s\")", hays[h], needles[n]);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(want, got, msg);
        }
    }
}

void test_find_ci_matches_a_folded_search(void)
{
    static const char *hays[] = {"The Quick Brown Fox", "ALLUPPER", "alllower", "MiXeD cAsE hErE"};
    static const char *needles[] = {"quick", "QUICK", "brown fox", "ALLUPPER", "allupper", "mixed", "absent"};

    for (unsigned h = 0; h < sizeof hays / sizeof hays[0]; h++)
    {
        for (unsigned n = 0; n < sizeof needles / sizeof needles[0]; n++)
        {
            const size_t hlen = strlen(hays[h]);
            const size_t nlen = strlen(needles[n]);
            const char *got = cellul.find(hays[h], hlen + 1u, needles[n], nlen + 1u, MMGR_TRUE);

            // the reference: a plain fold and compare, out of libc pieces
            const char *want = NULL;
            if (nlen <= hlen)
            {
                for (size_t i = 0; i + nlen <= hlen && want == NULL; i++)
                {
                    size_t k = 0;
                    while (k < nlen && tolower((unsigned char)hays[h][i + k]) == tolower((unsigned char)needles[n][k]))
                    {
                        k++;
                    }
                    if (k == nlen)
                    {
                        want = hays[h] + i;
                    }
                }
            }
            char msg[200];
            snprintf(msg, sizeof msg, "find_ci(\"%s\", \"%s\")", hays[h], needles[n]);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(want, got, msg);
        }
    }
}

void test_find_needle_longer_than_the_haystack(void)
{
    TEST_ASSERT_NULL(cellul.find("ab", 3u, "abcdef", 7u, MMGR_FALSE));
    TEST_ASSERT_NULL(cellul.find("", 1u, "a", 2u, MMGR_FALSE));
}

void test_diff_matches_a_byte_loop(void)
{
    static const char *pairs[][2] = {
        {"", ""},
        {"a", "a"},
        {"a", "b"},
        {"abc", "abd"},
        {"abcdefgh", "abcdefgh"},
        {"abcdefghij", "abcdefghiX"},
        {"abcdefgh", "abcdefgX"},
        {"aaaa", "aaab"},
    };

    for (unsigned i = 0; i < sizeof pairs / sizeof pairs[0]; i++)
    {
        const size_t cap = strlen(pairs[i][0]) + 1u;
        size_t want = 0;
        while (want < cap && pairs[i][0][want] == pairs[i][1][want])
        {
            want++;
        }
        TEST_ASSERT_EQUAL_size_t(want, cellul.diff(pairs[i][0], pairs[i][1], cap, MMGR_FALSE));
    }
}

void test_copy_of_an_empty_destination(void)
{
    char d[4];
    TEST_ASSERT_EQUAL_size_t(0u, cellul.copy(d, "abc", 0u));
}

void test_ws_and_digit_agree_with_ctype(void)
{
    for (int c = 0; c < 256; c++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(isspace(c) != 0, cellul.ws((char)c) != 0, "ws must agree with isspace");
        TEST_ASSERT_EQUAL_INT_MESSAGE(isdigit(c) != 0, cellul.digit((char)c) != 0, "digit must agree with isdigit");
    }
}

/* ---------------------------------------------------------------------------------------------
 * the word stepper, over both foldings and both endings
 *
 * Two events race inside a word: the pattern ends, or the two differ. end_wins says who takes the
 * lane when they land together, and the fold says which bytes count as a difference. That is four
 * combinations, and each has to be driven from lane 0 so it holds at every word width.
 * ------------------------------------------------------------------------------------------- */

void test_step_word_ignoring_case_agrees_on_a_folded_word(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO,
                                  cellul.step_word(word_of("ABCDEFGH"), word_of("abcdefgh"), MMGR_TRUE, 0),
                                  "a whole word of case differences is no difference at all");
}

void test_step_word_ignoring_case_still_sees_a_real_difference(void)
{
    // '1' and '2' fold to themselves, so the difference survives the fold.
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of("1bcdefgh"), word_of("2bcdefgh"), MMGR_TRUE, 0));
}

// A word wide enough for every environment, so a lane index means the same thing at 16, 32 and
// 64 bits. word_of loads a whole word, and a short literal would have it read past the literal.
//
// The pattern ends at lane 1 and the two part company at lane 2, so the end comes first outright
// rather than tying with the difference.
static const char ENDS_FIRST_A[8] = {'a', 0, 0, 0, 0, 0, 0, 0};
static const char ENDS_FIRST_B[8] = {'a', 0, 'X', 0, 0, 0, 0, 0};

// Both events land in lane 0, which is the tie end_wins exists to settle.
static const char TIED_A[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static const char TIED_B[8] = {'a', 0, 0, 0, 0, 0, 0, 0};

void test_step_word_ignoring_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES,
                                  cellul.step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_TRUE, 0),
                                  "the end came first, so end_wins never had to decide");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_TRUE, 1));
}

void test_step_word_ignoring_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, cellul.step_word(word_of(TIED_A), word_of(TIED_B), MMGR_TRUE, 1),
                                  "the end takes the tie");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, cellul.step_word(word_of(TIED_A), word_of(TIED_B), MMGR_TRUE, 0),
                                  "the difference takes the tie");
}

void test_step_word_matching_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_FALSE, 1));
}

void test_step_word_matching_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_word(word_of(TIED_A), word_of(TIED_B), MMGR_FALSE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of(TIED_A), word_of(TIED_B), MMGR_FALSE, 0));
}

void test_step_word_of_a_difference_that_beats_the_end(void)
{
    // No terminator anywhere in the pattern's word, so the difference is unopposed and end_wins
    // has nothing to give the lane to.
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of("Xbcdefgh"), word_of("abcdefgh"), MMGR_FALSE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_word(word_of("1bcdefgh"), word_of("2bcdefgh"), MMGR_TRUE, 1));
}

void test_step_word_of_two_words_that_both_run_on(void)
{
    // No terminator and no difference in either fold, so the compare has to keep going.
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_word(word_of("abcdefgh"), word_of("abcdefgh"), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_word(word_of("abcdefgh"), word_of("abcdefgh"), MMGR_TRUE, 1));
}

void test_step_byte_over_both_foldings_and_both_endings(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_byte('a', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, cellul.step_byte('A', 'a', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('A', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('1', '2', MMGR_TRUE, 0));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, cellul.step_byte('\0', '\0', MMGR_FALSE, 0),
                                  "both ending together is a match either way");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_byte('\0', '\0', MMGR_TRUE, 1));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, cellul.step_byte('\0', 'a', MMGR_FALSE, 1),
                                  "the pattern ending is a match when the end wins");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, cellul.step_byte('\0', 'a', MMGR_FALSE, 0),
                                  "and is not when it does not");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, cellul.step_byte('\0', 'a', MMGR_TRUE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, cellul.step_byte('\0', 'a', MMGR_TRUE, 0));
}

/* ---------------------------------------------------------------------------------------------
 * diff, which reports where two runs part company
 * ------------------------------------------------------------------------------------------- */

void test_diff_finds_the_first_differing_byte(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, cellul.diff("abc", "xbc", 3u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(1u, cellul.diff("abc", "axc", 3u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(2u, cellul.diff("abc", "abx", 3u, MMGR_FALSE));
}

void test_diff_of_runs_that_agree_is_the_whole_run(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, cellul.diff("abc", "abc", 3u, MMGR_FALSE),
                                     "no difference means the read cap, not an index");
    TEST_ASSERT_EQUAL_size_t(0u, cellul.diff("abc", "abc", 0u, MMGR_FALSE));
}

void test_diff_past_the_first_word(void)
{
    // Far enough in that the scan has to take a second load before it finds anything.
    static const char a[] = "the quick brown fox jumps over the lazy dog";
    static const char b[] = "the quick brown fox jumps over the LAZY dog";

    TEST_ASSERT_EQUAL_size_t(35u, cellul.diff(a, b, sizeof a - 1u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof a - 1u, cellul.diff(a, b, sizeof a - 1u, MMGR_TRUE),
                                     "the same pair agrees once case stops counting");
}

void test_diff_ignoring_case(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, cellul.diff("ABCd", "abcX", 4u, MMGR_TRUE));
    TEST_ASSERT_EQUAL_size_t(0u, cellul.diff("1", "2", 1u, MMGR_TRUE));
}

/* ---------------------------------------------------------------------------------------------
 * the parsers, at their edges
 * ------------------------------------------------------------------------------------------- */

void test_to_double_takes_a_leading_plus(void)
{
    const char *end = NULL;
    TEST_ASSERT_EQUAL_DOUBLE(2.5, cellul.to_double("+2.5", &end));
    TEST_ASSERT_EQUAL_DOUBLE(2.5, strtod("+2.5", NULL));
}

void test_to_double_takes_a_signed_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e+3", NULL), cellul.to_double("1e+3", NULL));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e-3", NULL), cellul.to_double("1e-3", NULL));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e3", NULL), cellul.to_double("1e3", NULL));
}

void test_to_double_clamps_an_absurd_exponent(void)
{
    // The exponent accumulator stops climbing long before it could overflow, so a run of digits
    // that no double can hold still terminates and still consumes the whole run.
    const char *end = NULL;
    const double v = cellul.to_double("1e999999", &end);

    TEST_ASSERT_TRUE_MESSAGE(v > 1.0e300 || v != v, "an exponent past the range does not come back small");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, (size_t)(end - (const char *)"1e999999"),
                                     "every digit of the exponent is consumed");
}

void test_to_double_of_a_negative_absurd_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, cellul.to_double("1e-999999", NULL));
}

void test_to_float_narrows_what_to_double_parses(void)
{
    TEST_ASSERT_EQUAL_FLOAT(2.5f, cellul.to_float("2.5", NULL));
    TEST_ASSERT_EQUAL_FLOAT((float)strtod("-0.125", NULL), cellul.to_float("-0.125", NULL));
}

/* ---------------------------------------------------------------------------------------------
 * a prefix compare that runs out of subject before it runs out of agreement
 *
 * starts() walks whole words looking for whichever comes first, the pattern's terminator or a
 * difference. When the read cap ends before either turns up, the walk falls out of the bottom of
 * the loop and end_wins alone decides. Every other case here ends inside the loop, so this is the
 * only way to reach that.
 * ------------------------------------------------------------------------------------------- */

void test_starts_when_the_read_cap_ends_first(void)
{
    // No terminator inside the cap and no difference inside it either: the pattern and the
    // subject agree for every byte the compare is allowed to look at.
    static const char pre[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_TRUE_MESSAGE(cellul.starts(s, pre, sizeof pre, MMGR_FALSE),
                             "they agreed for every byte that could be read");
    TEST_ASSERT_TRUE_MESSAGE(cellul.starts(s, pre, sizeof pre, MMGR_TRUE), "and the same ignoring case");
}

void test_eq_when_the_read_cap_ends_first(void)
{
    MMGR_SKIP_ON_ORACLE("strcmp has no read cap to end first - it reads until something terminates");
    static const char a[16] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'};
    static const char b2[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    // eq asks whether two strings are the same string, so it needs a terminator to say yes. These
    // two agree for every byte inside the cap and neither of them ends, so neither has been shown
    // to be a whole string and the answer is no whichever way case is counted. starts asks the
    // weaker question and says yes to the same pair.
    TEST_ASSERT_FALSE_MESSAGE(cellul.eq(a, b2, sizeof a, MMGR_FALSE), "case counts, and there is no terminator");
    TEST_ASSERT_FALSE_MESSAGE(cellul.eq(a, b2, sizeof a, MMGR_TRUE),
                              "case does not count, and there is still no terminator");
}

void test_starts_finds_a_difference_with_no_terminator_in_the_word(void)
{
    // The difference is in the first word and the pattern's terminator is not, so the lane race
    // has only one runner in it.
    static const char pre[16] = {'a', 'b', 'X', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_FALSE(cellul.starts(s, pre, sizeof pre, MMGR_FALSE));
    TEST_ASSERT_FALSE(cellul.starts(s, pre, sizeof pre, MMGR_TRUE));
}
