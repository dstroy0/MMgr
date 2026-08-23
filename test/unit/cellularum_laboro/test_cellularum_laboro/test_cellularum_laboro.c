
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

static const char *find_at(const char *hay, const char *needle, mmgr_bool ci)
{
    return mmgr_cellul_find(hay, CAP, needle, CAP, ci);
}

static long hit(const char *hay, const char *needle, mmgr_bool ci)
{
    const char *p = find_at(hay, needle, ci);
    return (p == NULL) ? -1L : (long)(p - hay);
}

void test_len_stops_at_nul_and_at_cap(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_len("", CAP));
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_cellul_len("abc", CAP));
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_cellul_len("abc", 2u));
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
    TEST_ASSERT_TRUE(mmgr_cellul_has("hello world", CAP, "world", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(mmgr_cellul_has("hello world", CAP, "WORLD", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(mmgr_cellul_has("hello world", CAP, "WORLD", CAP, MMGR_TRUE));
}

void test_eq_both_cases(void)
{
    TEST_ASSERT_TRUE(mmgr_cellul_eq("abc", "abc", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(mmgr_cellul_eq("abc", "ABC", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(mmgr_cellul_eq("abc", "ABC", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(mmgr_cellul_eq("abc", "abd", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(mmgr_cellul_eq("abc", "abcd", CAP, MMGR_FALSE));
}

void test_starts_both_cases(void)
{
    TEST_ASSERT_TRUE(mmgr_cellul_starts("abcdef", "abc", CAP, MMGR_FALSE));
    TEST_ASSERT_FALSE(mmgr_cellul_starts("abcdef", "ABC", CAP, MMGR_FALSE));
    TEST_ASSERT_TRUE(mmgr_cellul_starts("abcdef", "ABC", CAP, MMGR_TRUE));
    TEST_ASSERT_FALSE(mmgr_cellul_starts("ab", "abc", CAP, MMGR_FALSE));
}

void test_diff_returns_the_first_differing_offset(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_cellul_diff("abcd", "abce", 4u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_diff("Abcd", "abcd", 4u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(4u, mmgr_cellul_diff("Abcd", "abcd", 4u, MMGR_TRUE));
}

void test_diff_crossing_a_word_boundary(void)
{
        TEST_ASSERT_EQUAL_size_t(9u, mmgr_cellul_diff("aaaaaaaaab", "aaaaaaaaac", 10u, MMGR_FALSE));
}

void test_copy_truncates_and_terminates(void)
{
    char dst[8];
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_cellul_copy(dst, "abc", sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abc", dst);
    TEST_ASSERT_EQUAL_size_t(7u, mmgr_cellul_copy(dst, "abcdefghij", sizeof dst));
    TEST_ASSERT_EQUAL_STRING("abcdefg", dst);
}

void test_classifiers(void)
{
    TEST_ASSERT_TRUE(mmgr_cellul_ws(' '));
    TEST_ASSERT_TRUE(mmgr_cellul_ws('\t'));
    TEST_ASSERT_FALSE(mmgr_cellul_ws('a'));
    TEST_ASSERT_TRUE(mmgr_cellul_digit('0'));
    TEST_ASSERT_TRUE(mmgr_cellul_digit('9'));
    TEST_ASSERT_FALSE(mmgr_cellul_digit('a'));
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

        const long got = mmgr_cellul_to_long(s, mend);
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

        const unsigned long got = mmgr_cellul_to_ulong(s, mend);
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
    TEST_ASSERT_EQUAL_INT64(42, mmgr_cellul_to_long("42", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_UINT64(42u, mmgr_cellul_to_ulong("42", mmgr_cellul_nowhere));
}

void test_to_double_matches_strtod(void)
{
        static const char *cases[] = {"0",   "1",      "-1",      "0.5", "2.25", "-2.5", "100",  "0.125",
                                  "1.5", "  3.25", "12.5abc", "abc", "",     "-0.5", "1024", "0.0625"};

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *mend = NULL;
        char *lend = NULL;

        const double got = mmgr_cellul_to_double(cases[i], mend);
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
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e3", NULL) == mmgr_cellul_to_double("1e3", end), "to_double(\"1e3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1E3", NULL) == mmgr_cellul_to_double("1E3", end), "to_double(\"1E3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e-3", NULL) == mmgr_cellul_to_double("1e-3", end), "to_double(\"1e-3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("1e+3", NULL) == mmgr_cellul_to_double("1e+3", end), "to_double(\"1e+3\")");
    TEST_ASSERT_TRUE_MESSAGE(strtod("2.5e2", NULL) == mmgr_cellul_to_double("2.5e2", end), "to_double(\"2.5e2\")");
}

void test_to_float_matches_to_double(void)
{
    static const char *cases[] = {"0", "1", "-1", "0.5", "2.25", "1024", "0.0625"};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *end = NULL;
        TEST_ASSERT_TRUE_MESSAGE((float)strtod(cases[i], NULL) == mmgr_cellul_to_float(cases[i], end), cases[i]);
    }
}

void test_to_double_without_an_end_pointer(void)
{
    TEST_ASSERT_TRUE(2.5 == mmgr_cellul_to_double("2.5", mmgr_cellul_nowhere));
    TEST_ASSERT_TRUE(2.5f == mmgr_cellul_to_float("2.5", mmgr_cellul_nowhere));
}


static mmgr_scrut_word word_of(const char *s)
{
    return scrut.load(s);
}

void test_step_word_keeps_going_while_equal(void)
{
    const char *a = "abcdefghij";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO, mmgr_cellul_step_word(word_of(a), word_of(a), MMGR_FALSE, 0),
                                  "identical words with no terminator say keep going");
}

void test_step_word_stops_on_a_difference(void)
{
        const char *a = "Xbcdefgh";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of(a), word_of(b2), MMGR_FALSE, 0));
}

void test_step_word_stops_at_the_terminator(void)
{
        static const char a[16] = {0, "b"[0], "c"[0], "d"[0], "e"[0], "f"[0], "g"[0], "h"[0]};
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(a), word_of(b2), MMGR_FALSE, 1),
                                  "the pattern ended first and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of(a), word_of(b2), MMGR_FALSE, 0),
                                  "and without end_wins it is not");
}

void test_step_word_folds_case(void)
{
    const char *a = "ABCDEFGH";
    const char *b2 = "abcdefgh";
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of(a), word_of(b2), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_word(word_of(a), word_of(b2), MMGR_TRUE, 0));
}

void test_step_byte_covers_the_same_three_verdicts(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_byte('a', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('a', 'b', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', '\0', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', 'x', MMGR_FALSE, 1),
                                  "the pattern ended and end_wins says that is a match");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('\0', 'x', MMGR_FALSE, 0));

    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('A', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_byte('A', 'a', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', '\0', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', 'x', MMGR_TRUE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('\0', 'x', MMGR_TRUE, 0));
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
            const char *got = mmgr_cellul_chr(hays[h], cap, (uint8_t)c);
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
            TEST_ASSERT_EQUAL_PTR(strchr(s, c), mmgr_cellul_chr(s, 16u, (uint8_t)c));
        }
    }
}

void test_chr_respects_the_cap(void)
{
    MMGR_SKIP_ON_ORACLE("strchr has no cap to respect");
    const char *s = "abcdef";
    TEST_ASSERT_NULL_MESSAGE(mmgr_cellul_chr(s, 3u, 'f'), "f is past the cap");
    TEST_ASSERT_NOT_NULL(mmgr_cellul_chr(s, 3u, 'b'));
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
            const char *got = mmgr_cellul_find(hays[h], hcap, needles[n], ncap, MMGR_FALSE);
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
            const char *got = mmgr_cellul_find(hays[h], hlen + 1u, needles[n], nlen + 1u, MMGR_TRUE);

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
    TEST_ASSERT_NULL(mmgr_cellul_find("ab", 3u, "abcdef", 7u, MMGR_FALSE));
    TEST_ASSERT_NULL(mmgr_cellul_find("", 1u, "a", 2u, MMGR_FALSE));
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
        TEST_ASSERT_EQUAL_size_t(want, mmgr_cellul_diff(pairs[i][0], pairs[i][1], cap, MMGR_FALSE));
    }
}

void test_copy_of_an_empty_destination(void)
{
    char d[4];
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_copy(d, "abc", 0u));
}

void test_ws_and_digit_agree_with_ctype(void)
{
    for (int c = 0; c < 256; c++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(isspace(c) != 0, mmgr_cellul_ws((char)c) != 0, "ws must agree with isspace");
        TEST_ASSERT_EQUAL_INT_MESSAGE(isdigit(c) != 0, mmgr_cellul_digit((char)c) != 0, "digit must agree with isdigit");
    }
}


void test_step_word_ignoring_case_agrees_on_a_folded_word(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_GO,
                                  mmgr_cellul_step_word(word_of("ABCDEFGH"), word_of("abcdefgh"), MMGR_TRUE, 0),
                                  "a whole word of case differences is no difference at all");
}

void test_step_word_ignoring_case_still_sees_a_real_difference(void)
{
        TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of("1bcdefgh"), word_of("2bcdefgh"), MMGR_TRUE, 0));
}

static const char ENDS_FIRST_A[8] = {'a', 0, 0, 0, 0, 0, 0, 0};
static const char ENDS_FIRST_B[8] = {'a', 0, 'X', 0, 0, 0, 0, 0};

static const char TIED_A[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static const char TIED_B[8] = {'a', 0, 0, 0, 0, 0, 0, 0};

void test_step_word_ignoring_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES,
                                  mmgr_cellul_step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_TRUE, 0),
                                  "the end came first, so end_wins never had to decide");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_TRUE, 1));
}

void test_step_word_ignoring_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(TIED_A), word_of(TIED_B), MMGR_TRUE, 1),
                                  "the end takes the tie");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of(TIED_A), word_of(TIED_B), MMGR_TRUE, 0),
                                  "the difference takes the tie");
}

void test_step_word_matching_case_ends_before_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(ENDS_FIRST_A), word_of(ENDS_FIRST_B), MMGR_FALSE, 1));
}

void test_step_word_matching_case_ends_in_the_same_lane_as_a_difference(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_word(word_of(TIED_A), word_of(TIED_B), MMGR_FALSE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of(TIED_A), word_of(TIED_B), MMGR_FALSE, 0));
}

void test_step_word_of_a_difference_that_beats_the_end(void)
{
            TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of("Xbcdefgh"), word_of("abcdefgh"), MMGR_FALSE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_word(word_of("1bcdefgh"), word_of("2bcdefgh"), MMGR_TRUE, 1));
}

void test_step_word_of_two_words_that_both_run_on(void)
{
        TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_word(word_of("abcdefgh"), word_of("abcdefgh"), MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_word(word_of("abcdefgh"), word_of("abcdefgh"), MMGR_TRUE, 1));
}

void test_step_byte_over_both_foldings_and_both_endings(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_byte('a', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_GO, mmgr_cellul_step_byte('A', 'a', MMGR_TRUE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('A', 'a', MMGR_FALSE, 0));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('1', '2', MMGR_TRUE, 0));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', '\0', MMGR_FALSE, 0),
                                  "both ending together is a match either way");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', '\0', MMGR_TRUE, 1));

    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', 'a', MMGR_FALSE, 1),
                                  "the pattern ending is a match when the end wins");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_SWAR_NO, mmgr_cellul_step_byte('\0', 'a', MMGR_FALSE, 0),
                                  "and is not when it does not");
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_YES, mmgr_cellul_step_byte('\0', 'a', MMGR_TRUE, 1));
    TEST_ASSERT_EQUAL_INT(MMGR_SWAR_NO, mmgr_cellul_step_byte('\0', 'a', MMGR_TRUE, 0));
}


void test_diff_finds_the_first_differing_byte(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_diff("abc", "xbc", 3u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_cellul_diff("abc", "axc", 3u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_cellul_diff("abc", "abx", 3u, MMGR_FALSE));
}

void test_diff_of_runs_that_agree_is_the_whole_run(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, mmgr_cellul_diff("abc", "abc", 3u, MMGR_FALSE),
                                     "no difference means the read cap, not an index");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_diff("abc", "abc", 0u, MMGR_FALSE));
}

void test_diff_past_the_first_word(void)
{
        static const char a[] = "the quick brown fox jumps over the lazy dog";
    static const char b[] = "the quick brown fox jumps over the LAZY dog";

    TEST_ASSERT_EQUAL_size_t(35u, mmgr_cellul_diff(a, b, sizeof a - 1u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof a - 1u, mmgr_cellul_diff(a, b, sizeof a - 1u, MMGR_TRUE),
                                     "the same pair agrees once case stops counting");
}

void test_diff_ignoring_case(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_cellul_diff("ABCd", "abcX", 4u, MMGR_TRUE));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellul_diff("1", "2", 1u, MMGR_TRUE));
}


void test_to_double_takes_a_leading_plus(void)
{
    const char *end = NULL;
    TEST_ASSERT_EQUAL_DOUBLE(2.5, mmgr_cellul_to_double("+2.5", end));
    TEST_ASSERT_EQUAL_DOUBLE(2.5, strtod("+2.5", NULL));
}

void test_to_double_takes_a_signed_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e+3", NULL), mmgr_cellul_to_double("1e+3", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e-3", NULL), mmgr_cellul_to_double("1e-3", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE(strtod("1e3", NULL), mmgr_cellul_to_double("1e3", mmgr_cellul_nowhere));
}

void test_to_double_clamps_an_absurd_exponent(void)
{
            const char *end = NULL;
    const double v = mmgr_cellul_to_double("1e999999", end);

    TEST_ASSERT_TRUE_MESSAGE(v > 1.0e300 || v != v, "an exponent past the range does not come back small");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, (size_t)(end - (const char *)"1e999999"),
                                     "every digit of the exponent is consumed");
}

void test_to_double_of_a_negative_absurd_exponent(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, mmgr_cellul_to_double("1e-999999", mmgr_cellul_nowhere));
}

void test_to_float_narrows_what_to_double_parses(void)
{
    TEST_ASSERT_EQUAL_FLOAT(2.5f, mmgr_cellul_to_float("2.5", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_FLOAT((float)strtod("-0.125", NULL), mmgr_cellul_to_float("-0.125", mmgr_cellul_nowhere));
}


void test_starts_when_the_read_cap_ends_first(void)
{
            static const char pre[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_TRUE_MESSAGE(mmgr_cellul_starts(s, pre, sizeof pre, MMGR_FALSE),
                             "they agreed for every byte that could be read");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_cellul_starts(s, pre, sizeof pre, MMGR_TRUE), "and the same ignoring case");
}

void test_eq_when_the_read_cap_ends_first(void)
{
    MMGR_SKIP_ON_ORACLE("strcmp has no read cap to end first - it reads until something terminates");
    static const char a[16] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'};
    static const char b2[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

                    TEST_ASSERT_FALSE_MESSAGE(mmgr_cellul_eq(a, b2, sizeof a, MMGR_FALSE), "case counts, and there is no terminator");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_cellul_eq(a, b2, sizeof a, MMGR_TRUE),
                              "case does not count, and there is still no terminator");
}

void test_starts_finds_a_difference_with_no_terminator_in_the_word(void)
{
            static const char pre[16] = {'a', 'b', 'X', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
    static const char s[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

    TEST_ASSERT_FALSE(mmgr_cellul_starts(s, pre, sizeof pre, MMGR_FALSE));
    TEST_ASSERT_FALSE(mmgr_cellul_starts(s, pre, sizeof pre, MMGR_TRUE));
}


static uint64_t bits_of(double v)
{
    uint64_t b = 0;
    memcpy(&b, &v, sizeof b);
    return b;
}

static void same_as_strtod(const char *s)
{
    const double got = mmgr_cellul_to_double(s, mmgr_cellul_nowhere);
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
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(0.0, mmgr_cellul_to_double("1e-400", mmgr_cellul_nowhere), "past the bottom is zero");
    TEST_ASSERT_EQUAL_DOUBLE(0.0, mmgr_cellul_to_double("1e-1000", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(-0.0, mmgr_cellul_to_double("-1e-1000", mmgr_cellul_nowhere), "and keeps its sign");

    const double up = mmgr_cellul_to_double("1e400", mmgr_cellul_nowhere);
    TEST_ASSERT_TRUE_MESSAGE(up > 1.0e308, "past the top is an infinity");
    TEST_ASSERT_TRUE(mmgr_cellul_to_double("1e1000", mmgr_cellul_nowhere) > 1.0e308);
    TEST_ASSERT_TRUE(mmgr_cellul_to_double("-1e1000", mmgr_cellul_nowhere) < -1.0e308);
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

        const double got = mmgr_cellul_to_double(s, mmgr_cellul_nowhere);
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

        const double got = mmgr_cellul_to_double(s, mmgr_cellul_nowhere);
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
            const double over = mmgr_cellul_to_double("1.8e308", mmgr_cellul_nowhere);
    TEST_ASSERT_TRUE_MESSAGE(over > 1.7976931348623157e308, "just past the largest double is an infinity");
    TEST_ASSERT_TRUE(mmgr_cellul_to_double("-1.8e308", mmgr_cellul_nowhere) < -1.7976931348623157e308);

            TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(0.0, mmgr_cellul_to_double("1e-330", mmgr_cellul_nowhere), "below the smallest subnormal is zero");
    TEST_ASSERT_EQUAL_DOUBLE(0.0, mmgr_cellul_to_double("4.9e-330", mmgr_cellul_nowhere));
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(-0.0, mmgr_cellul_to_double("-1e-330", mmgr_cellul_nowhere), "and keeps its sign on the way");
}

void test_a_subnormal_that_rounds_up_into_the_normals(void)
{
                same_as_strtod("2.2250738585072012e-308");
    same_as_strtod("2.2250738585072013e-308");
    same_as_strtod("2.2250738585072011e-308");
    same_as_strtod("2.2250738585072009e-308");
}


void test_rd_str_reads_a_length_prefixed_run(void)
{
    static const uint8_t buf[9] = {0x00u, 0x00u, 0x00u, 0x03u, 'a', 'b', 'c', 'x', 'y'};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_TRUE(mmgr_cellul_rd_str(buf, sizeof buf, off, s, slen));
    TEST_ASSERT_EQUAL_UINT32(3u, slen);
    TEST_ASSERT_EQUAL_PTR(buf + 4, s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(7u, (size_t)(s - buf) + slen,
                                     "the offset lands past the run, ready for the next field");
}

void test_rd_str_reads_an_empty_run(void)
{
    static const uint8_t buf[4] = {0u, 0u, 0u, 0u};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 9u;

    TEST_ASSERT_TRUE(mmgr_cellul_rd_str(buf, sizeof buf, off, s, slen));
    TEST_ASSERT_EQUAL_UINT32(0u, slen);
    TEST_ASSERT_EQUAL_size_t(4u, (size_t)(s - buf) + slen);
}

void test_rd_str_rewinds_when_the_run_is_cut_short(void)
{
    static const uint8_t buf[6] = {0x00u, 0x00u, 0x00u, 0x09u, 'a', 'b'};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_FALSE_MESSAGE(mmgr_cellul_rd_str(buf, sizeof buf, off, s, slen), "the length claims nine, two are there");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, off, "the offset is put back where it started, not left mid field");
}

void test_rd_str_refuses_a_missing_length(void)
{
    static const uint8_t buf[2] = {0u, 0u};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_FALSE(mmgr_cellul_rd_str(buf, sizeof buf, off, s, slen));
    TEST_ASSERT_EQUAL_size_t(0u, off);
}

void test_rd_str_refuses_a_cursor_already_past_the_end(void)
{
                    static const uint8_t buf[8] = {0u, 0u, 0u, 1u, 'x', 0u, 0u, 0u};
    size_t off = sizeof buf + 1u;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_FALSE(mmgr_cellul_rd_str(buf, sizeof buf, off, s, slen));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof buf + 1u, off, "a refused read leaves the cursor alone");
    TEST_ASSERT_NULL(s);
}


void test_mpint_fixed_right_aligns_and_pads(void)
{
    static const uint8_t m[2] = {0x12u, 0x34u};
    uint8_t out[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};

    TEST_ASSERT_TRUE(mmgr_cellul_mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x34u, out[3]);
}

void test_mpint_fixed_drops_the_sign_padding(void)
{
        static const uint8_t m[3] = {0x00u, 0x80u, 0x01u};
    uint8_t out[2] = {0xFFu, 0xFFu};

    TEST_ASSERT_TRUE(mmgr_cellul_mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x80u, out[0], "the leading zero is not part of the value");
    TEST_ASSERT_EQUAL_HEX8(0x01u, out[1]);
}

void test_mpint_fixed_of_an_exact_width(void)
{
    static const uint8_t m[2] = {0xABu, 0xCDu};
    uint8_t out[2] = {0};

    TEST_ASSERT_TRUE(mmgr_cellul_mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8(0xABu, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCDu, out[1]);
}

void test_mpint_fixed_of_zero_is_all_zero(void)
{
    static const uint8_t m[3] = {0u, 0u, 0u};
    uint8_t out[4] = {1u, 2u, 3u, 4u};

    TEST_ASSERT_TRUE(mmgr_cellul_mpint_fixed(m, sizeof m, out, sizeof out));
    for (unsigned i = 0; i < 4u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, out[i]);
    }
}

void test_mpint_fixed_refuses_a_value_too_wide(void)
{
    static const uint8_t m[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t out[2] = {0xFFu, 0xFFu};

    TEST_ASSERT_FALSE_MESSAGE(mmgr_cellul_mpint_fixed(m, sizeof m, out, sizeof out), "four bytes do not fit in two");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, out[0], "a refused conversion leaves the output alone");
}

