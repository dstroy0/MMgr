/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "oracle_divergence.h"
#include "unity.h"

static const char *mmgr_cellul_nowhere;

#include "cellularum_laboro/cellularum_laboro.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAP 256u

/* to_long and to_ulong parse to the target's own word. A value needing more bits than the
   target has is not a case the host's 64 bit strtol can be an oracle for. */
static const long WORD_LONG_MAX = (long)((mmgr_word) ~(mmgr_word)0 >> 1);
static const unsigned long WORD_ULONG_MAX = (unsigned long)(mmgr_word) ~(mmgr_word)0;

static const char *find_at(const char *hay, const char *needle, mmgr_bool ci)
{
    return MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = hay, .cap = CAP, .other = needle, .other_cap = CAP, .ci = ci);
}

static long hit(const char *hay, const char *needle, mmgr_bool ci)
{
    const char *p = find_at(hay, needle, ci);
    return (p == NULL) ? -1L : (long)(p - hay);
}

void test_len_stops_at_nul_and_at_cap(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = "", .cap = CAP));
    TEST_ASSERT_EQUAL_size_t(3u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = "abc", .cap = CAP));
    TEST_ASSERT_EQUAL_size_t(2u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = "abc", .cap = 2u));
}

void test_find_empty_needle_matches_at_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, hit("abc", "", MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(0, hit("abc", "", MMGR_TRUE));
}

void test_find_at_every_offset(void)
{
        const char *h = "0123456789abcdef0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        char one[2] = {h[i], '\0'};
        TEST_ASSERT_EQUAL_INT((long)(strchr(h, one[0]) - h), hit(h, one, MMGR_FALSE));
    }
}

void test_find_spans_a_word_boundary(void)
{
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
            TEST_ASSERT_EQUAL_INT(-1, hit("a_b", "a?b", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(-1, hit("a@b", "a`b", MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(1, hit("_A_", "a", MMGR_TRUE));
}

void test_has_agrees_with_find(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = "hello world", .cap = CAP, .other = "world", .other_cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = "hello world", .cap = CAP, .other = "WORLD", .other_cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = "hello world", .cap = CAP, .other = "WORLD", .other_cap = CAP, .ci = MMGR_TRUE));
}

void test_eq_both_cases(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = "abc", .other = "abc", .cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = "abc", .other = "ABC", .cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = "abc", .other = "ABC", .cap = CAP, .ci = MMGR_TRUE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = "abc", .other = "abd", .cap = CAP, .ci = MMGR_TRUE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = "abc", .other = "abcd", .cap = CAP, .ci = MMGR_FALSE));
}

void test_starts_both_cases(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = "abcdef", .other = "abc", .cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = "abcdef", .other = "ABC", .cap = CAP, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = "abcdef", .other = "ABC", .cap = CAP, .ci = MMGR_TRUE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = "ab", .other = "abc", .cap = CAP, .ci = MMGR_FALSE));
}

void test_diff_returns_the_first_differing_offset(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abcd", .other = "abce", .cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "Abcd", .other = "abcd", .cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(4u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "Abcd", .other = "abcd", .cap = 4u, .ci = MMGR_TRUE));
}

void test_diff_crossing_a_word_boundary(void)
{
        TEST_ASSERT_EQUAL_size_t(9u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "aaaaaaaaab", .other = "aaaaaaaaac", .cap = 10u, .ci = MMGR_FALSE));
}

void test_copy_truncates_and_terminates(void)
{
    char dst[8];
    TEST_ASSERT_EQUAL_size_t(3u, MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = dst, .src = "abc", .cap = sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abc", dst);
    TEST_ASSERT_EQUAL_size_t(7u, MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = dst, .src = "abcdefghij", .cap = sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abcdefg", dst);
}

void test_classifiers(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = (const char[]){' ', 0}, .at = 0));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = (const char[]){'\t', 0}, .at = 0));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = (const char[]){'a', 0}, .at = 0));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = (const char[]){'0', 0}, .at = 0));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = (const char[]){'9', 0}, .at = 0));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = (const char[]){'a', 0}, .at = 0));
}


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

        const mmgr_iword got = MMGR_CALL(cellul.to_long, TransfiguroCfg, .src = s, .end = &mend);
        errno = 0;
        const long want = strtol(s, &lend, 10);

        char msg[128];
        snprintf(msg, sizeof msg, "to_long(\"%s\")", s);
        if (errno == 0 && want >= -WORD_LONG_MAX - 1L && want <= WORD_LONG_MAX)
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

        const mmgr_word got = MMGR_CALL(cellul.to_ulong, TransfiguroCfg, .src = s, .end = &mend);
        errno = 0;
        const unsigned long want = strtoul(s, &lend, 10);

        char msg[128];
        snprintf(msg, sizeof msg, "to_ulong(\"%s\")", s);
        if (errno == 0 && s[0] != '-' && want <= WORD_ULONG_MAX)
        {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(want, got, msg);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(lend, mend, msg);
        }
    }
}

void test_to_long_without_an_end_pointer(void)
{
    TEST_ASSERT_EQUAL_INT64(42, MMGR_CALL(cellul.to_long, TransfiguroCfg, .src = "42", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_UINT64(42u, MMGR_CALL(cellul.to_ulong, TransfiguroCfg, .src = "42", .end = &mmgr_cellul_nowhere));
}

void test_to_double_matches_strtod(void)
{
        static const char *cases[] = {"0",   "1",      "-1",      "0.5", "2.25", "-2.5", "100",  "0.125",
                                  "1.5", "  3.25", "12.5abc", "abc", "",     "-0.5", "1024", "0.0625"};

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *mend = NULL;
        char *lend = NULL;

        const double got = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = cases[i], .end = &mend);
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
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e3", NULL) == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e3", .end = &end), "to_double(\"1e3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1E3", NULL) == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1E3", .end = &end), "to_double(\"1E3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e-3", NULL) == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-3", .end = &end), "to_double(\"1e-3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e+3", NULL) == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e+3", .end = &end), "to_double(\"1e+3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("2.5e2", NULL) == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "2.5e2", .end = &end), "to_double(\"2.5e2\")");
}

void test_to_float_matches_to_double(void)
{
    static const char *cases[] = {"0", "1", "-1", "0.5", "2.25", "1024", "0.0625"};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *end = NULL;
        TEST_ASSERT_TRUE_MESSAGE((float)strtod(cases[i], NULL) == MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = cases[i], .end = &end), cases[i]);
    }
}

void test_to_double_without_an_end_pointer(void)
{
    TEST_ASSERT_TRUE(2.5 == MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "2.5", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_TRUE(2.5f == MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = "2.5", .end = &mmgr_cellul_nowhere));
}


static mmgr_word word_of(const char *s)
{
    return MMGR_CALL(word.load, ScrutWordCfg, .at = s);
}

void test_step_word_keeps_going_while_equal(void)
{
    const char *a = "abcdefghij";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(a), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE),
                                  "identical words with no terminator say keep going");
}

void test_step_word_stops_on_a_difference(void)
{
        const char *a = "Xbcdefgh";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(b2), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
}

void test_step_word_stops_at_the_terminator(void)
{
        static const char a[16] = {0, "b"[0], "c"[0], "d"[0], "e"[0], "f"[0], "g"[0], "h"[0]};
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(b2), .ci = MMGR_FALSE, .end_wins = MMGR_TRUE),
                                  "the pattern ended first and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(b2), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE),
                                  "and without end_wins it is not");
}

void test_step_word_folds_case(void)
{
    const char *a = "ABCDEFGH";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(b2), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(a), .wb = word_of(b2), .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
}

void test_step_byte_covers_the_same_three_verdicts(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'a', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'a', .cb = 'b', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = '\0', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'x', .ci = MMGR_FALSE, .end_wins = MMGR_TRUE),
                                  "the pattern ended and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'x', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));

    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'A', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'A', .cb = 'a', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = '\0', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'x', .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'x', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
}


void test_chr_matches_strchr(void)
{
    static const char *hays[] = {
        "", "a", "abc", "aaabbbccc", "the quick brown fox jumps over it", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"};

    for (unsigned h = 0; h < sizeof hays / sizeof hays[0]; h++)
    {
        const size_t cap = strlen(hays[h]) + 1u;
        for (int c = 0; c < 128; c++)
        {
            const char *got = MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = hays[h], .cap = cap, .byte = (uint8_t)c);
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
            TEST_ASSERT_EQUAL_PTR(strchr(s, c), MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = s, .cap = 16u, .byte = (uint8_t)c));
        }
    }
}

void test_chr_respects_the_cap(void)
{
    MMGR_SKIP_ON_ORACLE("strchr has no cap to respect");
    const char *s = "abcdef";
    TEST_ASSERT_NULL_MESSAGE(MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = s, .cap = 3u, .byte = 'f'), "f is past the cap");
    TEST_ASSERT_NOT_NULL(MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = s, .cap = 3u, .byte = 'b'));
}


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
            const char *got = MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = hays[h], .cap = hcap, .other = needles[n], .other_cap = ncap, .ci = MMGR_FALSE);
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
            const char *got = MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = hays[h], .cap = hlen + 1u, .other = needles[n], .other_cap = nlen + 1u, .ci = MMGR_TRUE);

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
    TEST_ASSERT_NULL(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = "ab", .cap = 3u, .other = "abcdef", .other_cap = 7u, .ci = MMGR_FALSE));
    TEST_ASSERT_NULL(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = "", .cap = 1u, .other = "a", .other_cap = 2u, .ci = MMGR_FALSE));
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
        TEST_ASSERT_EQUAL_size_t(want, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = pairs[i][0], .other = pairs[i][1], .cap = cap, .ci = MMGR_FALSE));
    }
}

void test_copy_of_an_empty_destination(void)
{
    char d[4];
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = d, .src = "abc", .cap = 0u));
}

void test_ws_and_digit_agree_with_ctype(void)
{
    for (int c = 0; c < 256; c++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            isspace(c) != 0,
            MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = (const char[]){(char)c, 0}, .at = 0) != 0,
            "ws must agree with isspace");
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            isdigit(c) != 0,
            MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = (const char[]){(char)c, 0}, .at = 0) != 0,
            "digit must agree with isdigit");
    }
}


void test_step_word_ignoring_case_agrees_on_a_folded_word(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO,
                                  MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("ABCDEFGH"), .wb = word_of("abcdefgh"), .ci = MMGR_TRUE, .end_wins = MMGR_FALSE),
                                  "a whole word of case differences is no difference at all");
}

void test_step_word_ignoring_case_still_sees_a_real_difference(void)
{
        TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("1bcdefgh"), .wb = word_of("2bcdefgh"), .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
}

static const char ENDS_FIRST_A[8] = {'a', 0, 0, 0, 0, 0, 0, 0};
static const char ENDS_FIRST_B[8] = {'a', 0, 'X', 0, 0, 0, 0, 0};

static const char TIED_A[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static const char TIED_B[8] = {'a', 0, 0, 0, 0, 0, 0, 0};

void test_step_word_ignoring_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES,
                                  MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(ENDS_FIRST_A), .wb = word_of(ENDS_FIRST_B), .ci = MMGR_TRUE, .end_wins = MMGR_FALSE),
                                  "the end came first, so end_wins never had to decide");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(ENDS_FIRST_A), .wb = word_of(ENDS_FIRST_B), .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));
}

void test_step_word_ignoring_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(TIED_A), .wb = word_of(TIED_B), .ci = MMGR_TRUE, .end_wins = MMGR_TRUE),
                                  "the end takes the tie");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(TIED_A), .wb = word_of(TIED_B), .ci = MMGR_TRUE, .end_wins = MMGR_FALSE),
                                  "the difference takes the tie");
}

void test_step_word_matching_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(ENDS_FIRST_A), .wb = word_of(ENDS_FIRST_B), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(ENDS_FIRST_A), .wb = word_of(ENDS_FIRST_B), .ci = MMGR_FALSE, .end_wins = MMGR_TRUE));
}

void test_step_word_matching_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(TIED_A), .wb = word_of(TIED_B), .ci = MMGR_FALSE, .end_wins = MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of(TIED_A), .wb = word_of(TIED_B), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
}

void test_step_word_of_a_difference_that_beats_the_end(void)
{
            TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("Xbcdefgh"), .wb = word_of("abcdefgh"), .ci = MMGR_FALSE, .end_wins = MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("1bcdefgh"), .wb = word_of("2bcdefgh"), .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));
}

void test_step_word_of_two_words_that_both_run_on(void)
{
        TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("abcdefgh"), .wb = word_of("abcdefgh"), .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_word, VerboProgrediorCfg, .wa = word_of("abcdefgh"), .wb = word_of("abcdefgh"), .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));
}

void test_step_byte_over_both_foldings_and_both_endings(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'a', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'A', .cb = 'a', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = 'A', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '1', .cb = '2', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = '\0', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE),
                                  "both ending together is a match either way");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = '\0', .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_TRUE),
                                  "the pattern ending is a match when the end wins");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'a', .ci = MMGR_FALSE, .end_wins = MMGR_FALSE),
                                  "and is not when it does not");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'a', .ci = MMGR_TRUE, .end_wins = MMGR_TRUE));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, MMGR_CALL(cellul.step_byte, VerboProgrediorCfg, .ca = '\0', .cb = 'a', .ci = MMGR_TRUE, .end_wins = MMGR_FALSE));
}


void test_diff_finds_the_first_differing_byte(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abc", .other = "xbc", .cap = 3u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(1u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abc", .other = "axc", .cap = 3u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(2u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abc", .other = "abx", .cap = 3u, .ci = MMGR_FALSE));
}

void test_diff_of_runs_that_agree_is_the_whole_run(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abc", .other = "abc", .cap = 3u, .ci = MMGR_FALSE),
                                     "no difference means the read cap, not an index");
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "abc", .other = "abc", .cap = 0u, .ci = MMGR_FALSE));
}

void test_diff_past_the_first_word(void)
{
        static const char a[] = "the quick brown fox jumps over the lazy dog";
    static const char b[] = "the quick brown fox jumps over the LAZY dog";

    TEST_ASSERT_EQUAL_size_t(35u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = a, .other = b, .cap = sizeof a - 1u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof a - 1u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = a, .other = b, .cap = sizeof a - 1u, .ci = MMGR_TRUE),
                                     "the same pair agrees once case stops counting");
}

void test_diff_ignoring_case(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "ABCd", .other = "abcX", .cap = 4u, .ci = MMGR_TRUE));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = "1", .other = "2", .cap = 1u, .ci = MMGR_TRUE));
}


void test_to_double_takes_a_leading_plus(void)
{
    const char *end = NULL;
    TEST_ASSERT_EQUAL_DOUBLE(2.5, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "+2.5", .end = &end));
    TEST_ASSERT_EQUAL_DOUBLE(2.5, strtod("+2.5", NULL));
}

void test_to_double_takes_a_signed_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e+3", NULL), MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e+3", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e-3", NULL), MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-3", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e3", NULL), MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e3", .end = &mmgr_cellul_nowhere));
}

void test_to_double_clamps_an_absurd_exponent(void)
{
            const char *end = NULL;
    const double v = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e999999", .end = &end);

    TEST_ASSERT_TRUE_MESSAGE(v > 1.0e300 || v != v, "an exponent past the range does not come back small");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, (size_t)(end - (const char *)"1e999999"),
                                     "every digit of the exponent is consumed");
}

void test_to_double_of_a_negative_absurd_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-999999", .end = &mmgr_cellul_nowhere));
}

void test_to_float_narrows_what_to_double_parses(void)
{
    TEST_ASSERT_EQUAL_FLOAT(2.5f, MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = "2.5", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_FLOAT((float)strtod("-0.125", NULL), MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = "-0.125", .end = &mmgr_cellul_nowhere));
}


void test_starts_when_the_read_cap_ends_first(void)
{
            static const char pre[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = s, .other = pre, .cap = sizeof pre, .ci = MMGR_FALSE),
                             "they agreed for every byte that could be read");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = s, .other = pre, .cap = sizeof pre, .ci = MMGR_TRUE), "and the same ignoring case");
}

void test_eq_when_the_read_cap_ends_first(void)
{
    MMGR_SKIP_ON_ORACLE("strcmp has no read cap to end first - it reads until something terminates");
    static const char a[16] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'};
    static const char b2[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

                    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = a, .other = b2, .cap = sizeof a, .ci = MMGR_FALSE), "case counts, and there is no terminator");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = a, .other = b2, .cap = sizeof a, .ci = MMGR_TRUE),
                              "case does not count, and there is still no terminator");
}

void test_starts_finds_a_difference_with_no_terminator_in_the_word(void)
{
            static const char pre[16] = {'a', 'b', 'X', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_FALSE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = s, .other = pre, .cap = sizeof pre, .ci = MMGR_FALSE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = s, .other = pre, .cap = sizeof pre, .ci = MMGR_TRUE));
}


static uint64_t bits_of(double v)
{
    uint64_t b = 0;
    memcpy(&b, &v, sizeof b);
    return b;
}

static void same_as_strtod(const char *s)
{
    const double got = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = s, .end = &mmgr_cellul_nowhere);
    const double want = strtod(s, NULL);

    if (bits_of(got) != bits_of(want))
    {
        char msg[160];
        (void)snprintf(msg, sizeof msg, "\"%s\": got %.17g (%016llx), wanted %.17g (%016llx)", s, got,
                       (unsigned long long)bits_of(got), want, (unsigned long long)bits_of(want));
        TEST_FAIL_MESSAGE(msg);
    }
}

void test_a_power_of_ten_past_what_is_exactly_a_double(void)
{
            same_as_strtod("1e22");
    same_as_strtod("1e23");
    same_as_strtod("1e24");
    same_as_strtod("1.7976931348623157e308");
    same_as_strtod("2.2250738585072014e-308");
}

void test_the_reciprocal_table_carries_the_negative_exponents(void)
{
    same_as_strtod("1e-23");
    same_as_strtod("1e-50");
    same_as_strtod("1e-100");
    same_as_strtod("1e-200");
    same_as_strtod("1e-300");
    same_as_strtod("1.2345678901234567e-250");
}

void test_every_step_of_the_table_gets_used(void)
{
            static const int steps[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    char s[64];

    for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++)
    {
        (void)snprintf(s, sizeof s, "1.5e%d", steps[i]);
        same_as_strtod(s);
        (void)snprintf(s, sizeof s, "1.5e-%d", steps[i]);
        same_as_strtod(s);
    }
        same_as_strtod("9.87654321e287");
    same_as_strtod("9.87654321e-287");
}

void test_the_subnormals(void)
{
    same_as_strtod("4.9406564584124654e-324");     same_as_strtod("9.8813129168249309e-324");
    same_as_strtod("1e-320");
    same_as_strtod("2.4703282292062328e-324");     same_as_strtod("1.5e-323");
}

void test_underflow_and_overflow(void)
{
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-400", .end = &mmgr_cellul_nowhere), "past the bottom is zero");
    TEST_ASSERT_EQUAL_DOUBLE(0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-1000", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(-0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "-1e-1000", .end = &mmgr_cellul_nowhere), "and keeps its sign");

    const double up = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e400", .end = &mmgr_cellul_nowhere);
    TEST_ASSERT_TRUE_MESSAGE(up > 1.0e308, "past the top is an infinity");
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e1000", .end = &mmgr_cellul_nowhere) > 1.0e308);
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "-1e1000", .end = &mmgr_cellul_nowhere) < -1.0e308);
}

void test_more_digits_than_the_mantissa_can_hold(void)
{
            same_as_strtod("1234567890123456789012345");
    same_as_strtod("0.12345678901234567890123456789");
    same_as_strtod("1.0000000000000000000000001");
    same_as_strtod("1.0000000000000000000000000");
}

void test_a_rounding_that_carries_out_of_the_mantissa(void)
{
        same_as_strtod("1.9999999999999999");
    same_as_strtod("9.9999999999999999e22");
    same_as_strtod("4.4501477170144023e-308"); }

void test_the_leading_zero_count_at_every_width(void)
{
            char s[64];

    for (unsigned bit = 0; bit < 63u; bit++)
    {
        const uint64_t m = (uint64_t)1 << bit;
        (void)snprintf(s, sizeof s, "%llue30", (unsigned long long)m);
        same_as_strtod(s);
    }
}

void test_the_conversion_over_random_bit_patterns(void)
{
                uint64_t st = 0x9E3779B97F4A7C15ull;
    char s[64];

    for (unsigned i = 0; i < 20000u; i++)
    {
        st ^= st << 13;
        st ^= st >> 7;
        st ^= st << 17;

        double v;
        memcpy(&v, &st, sizeof v);
        if (v != v || v > 1.7e308 || v < -1.7e308)
        {
            continue;
        }
        (void)snprintf(s, sizeof s, "%.17g", v);

        const double got = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = s, .end = &mmgr_cellul_nowhere);
        if (bits_of(got) != bits_of(v))
        {
            char msg[160];
            (void)snprintf(msg, sizeof msg, "\"%s\" came back %016llx, wanted %016llx", s,
                           (unsigned long long)bits_of(got), (unsigned long long)bits_of(v));
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

void test_the_conversion_over_strobed_bits(void)
{
            uint64_t st = 0x243F6A8885A308D3ull;
    char s[64];

    for (unsigned i = 0; i < 20000u; i++)
    {
        double base = 1.0;
        uint64_t b;
        memcpy(&b, &base, sizeof b);

        for (unsigned k = 0; k < 1u + (i % 5u); k++)
        {
            st ^= st << 13;
            st ^= st >> 7;
            st ^= st << 17;
            b ^= (uint64_t)1 << (st % 64u);
        }

        double v;
        memcpy(&v, &b, sizeof v);
        if (v != v || v > 1.7e308 || v < -1.7e308)
        {
            continue;
        }
        (void)snprintf(s, sizeof s, "%.17g", v);

        const double got = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = s, .end = &mmgr_cellul_nowhere);
        if (bits_of(got) != bits_of(v))
        {
            char msg[160];
            (void)snprintf(msg, sizeof msg, "strobed \"%s\" came back %016llx, wanted %016llx", s,
                           (unsigned long long)bits_of(got), (unsigned long long)bits_of(v));
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

void test_the_ends_of_the_range_through_the_table(void)
{
            const double over = MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1.8e308", .end = &mmgr_cellul_nowhere);
    TEST_ASSERT_TRUE_MESSAGE(over > 1.7976931348623157e308, "just past the largest double is an infinity");
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "-1.8e308", .end = &mmgr_cellul_nowhere) < -1.7976931348623157e308);

            TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "1e-330", .end = &mmgr_cellul_nowhere), "below the smallest subnormal is zero");
    TEST_ASSERT_EQUAL_DOUBLE(0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "4.9e-330", .end = &mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(-0.0, MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = "-1e-330", .end = &mmgr_cellul_nowhere), "and keeps its sign on the way");
}

void test_a_subnormal_that_rounds_up_into_the_normals(void)
{
                same_as_strtod("2.2250738585072012e-308");
    same_as_strtod("2.2250738585072013e-308");
    same_as_strtod("2.2250738585072011e-308");
    same_as_strtod("2.2250738585072009e-308");
}













