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

static const char *find_in(const char *hay, const char *needle, embed_bool ci)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = hay, .cap = strlen(hay) + 1u, .other = needle,
                      .other_cap = strlen(needle) + 1u, .ci = ci);
}

void test_two_rows_find_a_needle_late_in_a_long_haystack(void)
{
    static const char hay[] = "the quick brown fox jumps over the lazy dog and keeps on going past the end";
    const char *at = find_in(hay, "lazy", EMBED_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 35, at);
}

void test_two_rows_find_a_needle_of_one_repeated_byte(void)
{
    static const char hay[] = "................................aaaa............................";
    const char *at = find_in(hay, "aaaa", EMBED_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 32, at);
}

void test_two_rows_find_a_needle_whose_ends_repeat(void)
{
    static const char hay[] = "----------------------------------------abcabc--------------------------";
    const char *at = find_in(hay, "abcabc", EMBED_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 40, at);
}

void test_two_rows_report_a_needle_that_is_not_there(void)
{
    static const char hay[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    TEST_ASSERT_NULL(find_in(hay, "aaab", EMBED_FALSE));
    TEST_ASSERT_NULL(find_in(hay, "zzzzzzzz", EMBED_FALSE));
}

void test_two_rows_find_a_needle_ignoring_case(void)
{
    static const char hay[] = "0123456789012345678901234567890123456789 The Lazy Dog 01234567890123456789";
    const char *at = find_in(hay, "lazy dog", EMBED_TRUE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 45, at);
}

void test_two_rows_find_a_needle_at_the_very_start(void)
{
    static const char hay[] = "needle in a haystack that runs on well past the end of the first word";

    TEST_ASSERT_EQUAL_PTR(hay, find_in(hay, "needle", EMBED_FALSE));
}

void test_two_rows_find_a_needle_of_two_bytes(void)
{
    static const char hay[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxqzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    const char *at = find_in(hay, "qz", EMBED_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 32, at);
}

void test_two_rows_find_a_long_needle(void)
{
    static const char hay[] = "prefix bytes here and then AAAABBBBCCCCDDDD and some trailing bytes after it";
    const char *at = find_in(hay, "AAAABBBBCCCCDDDD", EMBED_FALSE);

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_PTR(hay + 27, at);
}
