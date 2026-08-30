/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "config/mmgr_string_shim.h"

void test_shim_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr_string_shim.h compiled with no header before it");
}

void test_shim_bound_is_a_compile_time_constant(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_CARCER_MAX, MMGR_STR_MAX, "the read cap is the buffer bound, not SIZE_MAX");
}

void test_shim_strlen(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, strlen(""));
    TEST_ASSERT_EQUAL_size_t(5u, strlen("hello"));
    TEST_ASSERT_EQUAL_size_t(3u, strnlen("hello", 3u));
}

void test_shim_strstr(void)
{
    const char *hay = "the quick brown fox";
    TEST_ASSERT_EQUAL_PTR(hay + 10, strstr(hay, "brown"));
    TEST_ASSERT_EQUAL_PTR(hay, strstr(hay, "the"));
    TEST_ASSERT_NULL(strstr(hay, "zebra"));
    TEST_ASSERT_EQUAL_PTR(hay + 10, strcasestr(hay, "BROWN"));
}

void test_shim_strcmp_is_equality(void)
{
    TEST_ASSERT_EQUAL_INT(0, strcmp("abc", "abc"));
    TEST_ASSERT_NOT_EQUAL(0, strcmp("abc", "abd"));
    TEST_ASSERT_EQUAL_INT(0, strcasecmp("AbC", "aBc"));
}

void test_shim_strncmp_bounds_the_compare(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, strncmp("abcXX", "abcYY", 3u), "equal in the first three");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strncmp("abcXX", "abcYY", 4u), "differ by the fourth");
}

void test_shim_memory_entries(void)
{
    char d[16];
    TEST_ASSERT_EQUAL_PTR(d, memset(d, 0, sizeof d));
    TEST_ASSERT_EQUAL_CHAR(0, d[0]);
    TEST_ASSERT_EQUAL_PTR(d, memcpy(d, "hello", 6u));
    TEST_ASSERT_EQUAL_STRING("hello", d);
    TEST_ASSERT_EQUAL_INT(0, memcmp(d, "hello", 6u));
    TEST_ASSERT_EQUAL_PTR(d + 1, memchr(d, 'e', 6u));
    TEST_ASSERT_NULL(memchr(d, 'z', 6u));
}

void test_shim_memmove_handles_overlap(void)
{
    char d[16];
    memcpy(d, "abcdef", 7u);
    memmove(d + 1, d, 5u);
    TEST_ASSERT_EQUAL_CHAR('a', d[0]);
    TEST_ASSERT_EQUAL_CHAR('a', d[1]);
    TEST_ASSERT_EQUAL_CHAR('e', d[5]);
}

void test_shim_strchr_finds_the_terminator(void)
{
    const char *s = "abc";
    TEST_ASSERT_EQUAL_PTR(s + 1, strchr(s, 'b'));
    TEST_ASSERT_NULL(strchr(s, 'z'));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(s + 3, strchr(s, '\0'), "strchr is defined to find the terminator");
}

void test_shim_strlcpy_reports_what_it_wrote(void)
{
    char d[8];
    TEST_ASSERT_EQUAL_size_t(3u, strlcpy(d, "abc", sizeof d));
    TEST_ASSERT_EQUAL_STRING("abc", d);
}
