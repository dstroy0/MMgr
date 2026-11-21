#include "cellularum_laboro/cellularum_linea.h"

#include "unity.h"

#include <stdint.h>

static char s_text[64];

void setUp(void)
{
    for (size_t at = 0u; at < sizeof s_text; at++)
    {
        s_text[at] = 0x7F;
    }
}

void tearDown(void)
{
}

void test_length_counts_the_bytes_before_the_terminator(void)
{
    TEST_ASSERT_EQUAL_size_t(11u, mmgr_text_length("orior exact", 64u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_text_length("", 64u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, mmgr_text_length("orior exact", 4u),
                                     "no terminator inside the bound answers it");
}

void test_equal_answers_true_only_where_both_end_together(void)
{
    TEST_ASSERT_TRUE(mmgr_text_equal("record", "record", 64u));
    TEST_ASSERT_FALSE(mmgr_text_equal("record", "records", 64u));
    TEST_ASSERT_FALSE(mmgr_text_equal("record", "Record", 64u));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_text_equal("record", "record", 3u), "no end inside the bound is no match");
}

void test_order_orders_as_strcmp_does(void)
{
    TEST_ASSERT_EQUAL_INT(0, mmgr_text_order("orior", "orior", 64u));
    TEST_ASSERT_LESS_THAN_INT(0, mmgr_text_order("abc", "abd", 64u));
    TEST_ASSERT_GREATER_THAN_INT(0, mmgr_text_order("abd", "abc", 64u));
    TEST_ASSERT_LESS_THAN_INT_MESSAGE(0, mmgr_text_order("ab", "abc", 64u), "a text that ends first comes first");
    TEST_ASSERT_GREATER_THAN_INT(0, mmgr_text_order("abc", "ab", 64u));
    TEST_ASSERT_EQUAL_INT(0, mmgr_text_order("", "", 64u));
    TEST_ASSERT_LESS_THAN_INT(0, mmgr_text_order("", "a", 64u));
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, mmgr_text_order("\xC3", "a", 64u), "a byte reads as unsigned");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mmgr_text_order("abcdef", "abcxyz", 3u), "no difference inside the bound");
}

void test_starts_answers_true_where_the_prefix_ends_first(void)
{
    TEST_ASSERT_TRUE(mmgr_text_starts("record_sum", "record", 64u));
    TEST_ASSERT_TRUE(mmgr_text_starts("record_sum", "", 64u));
    TEST_ASSERT_FALSE(mmgr_text_starts("record", "record_sum", 64u));
    TEST_ASSERT_FALSE(mmgr_text_starts("Record_sum", "record", 64u));
}

void test_char_answers_the_first_byte_before_the_terminator(void)
{
    const char *const text = "a=b=c";
    TEST_ASSERT_EQUAL_PTR(text + 1, mmgr_text_char(text, (uint8_t)'=', 64u));
    TEST_ASSERT_NULL(mmgr_text_char(text, (uint8_t)'z', 64u));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(text + 5, mmgr_text_char(text, 0u, 64u), "a zero answers the terminator");
}

void test_find_answers_the_first_place_of_the_needle(void)
{
    const char *const haystack = "abcabcabd";
    TEST_ASSERT_EQUAL_PTR(haystack + 6, mmgr_text_find(haystack, "abd", 64u));
    TEST_ASSERT_EQUAL_PTR(haystack + 1, mmgr_text_find(haystack, "bc", 64u));
    TEST_ASSERT_NULL(mmgr_text_find(haystack, "abe", 64u));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(haystack, mmgr_text_find(haystack, "", 64u), "an empty needle answers the haystack");
}

void test_copy_writes_the_text_and_a_terminator_inside_the_bound(void)
{
    TEST_ASSERT_EQUAL_size_t(5u, mmgr_text_copy(s_text, "sweep", 64u));
    TEST_ASSERT_EQUAL_STRING("sweep", s_text);
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_text_copy(&s_text[16], "sweep", 4u));
    TEST_ASSERT_EQUAL_STRING("swe", &s_text[16]);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0x7F, s_text[20], "no byte past the bound is written");
}

void test_copy_of_no_room_writes_nothing(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_text_copy(s_text, "sweep", 0u));
    TEST_ASSERT_EQUAL_INT8(0x7F, s_text[0]);
}

void test_unsigned_reads_the_digits_and_stops_past_them(void)
{
    const char *end = NULL;
    const char *const text = "  +4096 lanes";
    TEST_ASSERT_EQUAL_UINT64(4096u, (uint64_t)mmgr_text_unsigned(text, &end));
    TEST_ASSERT_EQUAL_PTR(text + 7, end);
}

void test_unsigned_with_no_digit_answers_zero_at_the_start(void)
{
    const char *end = NULL;
    const char *const text = "lanes";
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)mmgr_text_unsigned(text, &end));
    TEST_ASSERT_EQUAL_PTR(text, end);
    TEST_ASSERT_EQUAL_UINT64(17u, (uint64_t)mmgr_text_unsigned("17", NULL));
}
