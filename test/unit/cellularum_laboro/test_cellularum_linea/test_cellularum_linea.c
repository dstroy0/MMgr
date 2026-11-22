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

void test_unsigned_in_reads_a_base_and_stops_past_its_digits(void)
{
    const char *end = NULL;
    const char *const hex = " 0x1fZ";
    TEST_ASSERT_EQUAL_UINT64(31u, (uint64_t)mmgr_text_unsigned_in(hex, &end, 16u));
    TEST_ASSERT_EQUAL_PTR(hex + 5, end);
    TEST_ASSERT_EQUAL_UINT64(0xABCu, (uint64_t)mmgr_text_unsigned_in("abc", NULL, 16u));
    TEST_ASSERT_EQUAL_UINT64(5u, (uint64_t)mmgr_text_unsigned_in("101", NULL, 2u));
    TEST_ASSERT_EQUAL_UINT64(35u, (uint64_t)mmgr_text_unsigned_in("Z", NULL, 36u));
    TEST_ASSERT_EQUAL_UINT64(42u, (uint64_t)mmgr_text_unsigned_in("+42", NULL, 10u));
}

void test_unsigned_in_takes_the_base_of_the_prefix_for_a_base_of_zero(void)
{
    TEST_ASSERT_EQUAL_UINT64(255u, (uint64_t)mmgr_text_unsigned_in("0XfF", NULL, 0u));
    TEST_ASSERT_EQUAL_UINT64(8u, (uint64_t)mmgr_text_unsigned_in("010", NULL, 0u));
    TEST_ASSERT_EQUAL_UINT64(10u, (uint64_t)mmgr_text_unsigned_in("10", NULL, 0u));
    const char *end = NULL;
    const char *const bare = "0xg";
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, (uint64_t)mmgr_text_unsigned_in(bare, &end, 0u),
                                     "an x with no hex digit after it reads the 0 alone");
    TEST_ASSERT_EQUAL_PTR(bare + 1, end);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)mmgr_text_unsigned_in("0xg", NULL, 16u));
}

void test_unsigned_in_negates_saturates_and_refuses_a_base(void)
{
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(embed_word)(0u - (embed_word)5u),
                             (uint64_t)mmgr_text_unsigned_in("-5", NULL, 10u));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE((uint64_t)(embed_word) ~(embed_word)0u,
                                     (uint64_t)mmgr_text_unsigned_in("123456789012345678901234567890", NULL, 10u),
                                     "digits past the word answer all ones");
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(embed_word) ~(embed_word)0u,
                             (uint64_t)mmgr_text_unsigned_in("-123456789012345678901234567890", NULL, 10u));
    const char *end = NULL;
    const char *const text = "17";
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)mmgr_text_unsigned_in(text, &end, 1u));
    TEST_ASSERT_EQUAL_PTR(text, end);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)mmgr_text_unsigned_in(text, NULL, 37u));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)mmgr_text_unsigned_in("zz", &end, 10u));
}

void test_span_counts_the_bytes_the_set_holds(void)
{
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_text_span("  \tlane", " \t", 64u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_text_span("lane", " ", 64u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_text_span("lane", "", 64u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, mmgr_text_span("aaaa", "a", 2u), "the bound stops the count");
    TEST_ASSERT_EQUAL_size_t(4u, mmgr_text_span("aaaa", "a", 64u));
}

void test_span_not_counts_the_bytes_before_the_set(void)
{
    TEST_ASSERT_EQUAL_size_t(4u, mmgr_text_span_not("lane=7", "=;", 64u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_text_span_not("=7", "=", 64u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(6u, mmgr_text_span_not("lane 7", "", 64u), "an empty set counts the whole text");
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_text_span_not("lane", "=", 3u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_text_span_not("\xC3=", "=", 64u), "a byte reads as unsigned");
}

void test_char_last_answers_the_last_byte_before_the_terminator(void)
{
    const char *const text = "a/b/c";
    TEST_ASSERT_EQUAL_PTR(text + 3, mmgr_text_char_last(text, (uint8_t)'/', 64u));
    TEST_ASSERT_NULL(mmgr_text_char_last(text, (uint8_t)'z', 64u));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(text + 5, mmgr_text_char_last(text, 0u, 64u), "a zero answers the terminator");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(text + 1, mmgr_text_char_last(text, (uint8_t)'/', 3u), "the bound stops the walk");
    TEST_ASSERT_EQUAL_PTR(text + 2, mmgr_text_char_last(text, 0u, 2u));
}

void test_class_tests_a_byte_against_its_class(void)
{
    TEST_ASSERT_TRUE(mmgr_text_is_num((uint8_t)'7'));
    TEST_ASSERT_FALSE(mmgr_text_is_num((uint8_t)'x'));
    TEST_ASSERT_TRUE(mmgr_text_is_alnum((uint8_t)'Q'));
    TEST_ASSERT_TRUE(mmgr_text_is_space((uint8_t)'\t'));
    TEST_ASSERT_TRUE(mmgr_text_is_upper((uint8_t)'K'));
    TEST_ASSERT_TRUE(mmgr_text_is_lower((uint8_t)'k'));
    TEST_ASSERT_TRUE(mmgr_text_is_hex((uint8_t)'f'));
    TEST_ASSERT_FALSE(mmgr_text_is_hex((uint8_t)'g'));
    TEST_ASSERT_TRUE(mmgr_text_is_punct((uint8_t)'['));
    TEST_ASSERT_TRUE(mmgr_text_is_ctrl(0x7Fu));
    TEST_ASSERT_TRUE(mmgr_text_is_print((uint8_t)' '));
    TEST_ASSERT_FALSE(mmgr_text_is_print(0x7Fu));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_text_is_alpha(0xC3u), "a byte past 0x7F is in no class");
}

void test_lower_and_upper_fold_the_letters_alone(void)
{
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'a', mmgr_text_lower((uint8_t)'A'));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'z', mmgr_text_lower((uint8_t)'Z'));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'q', mmgr_text_lower((uint8_t)'q'));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)'@', mmgr_text_lower((uint8_t)'@'), "the byte before A stays");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'[', mmgr_text_lower((uint8_t)'['));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'A', mmgr_text_upper((uint8_t)'a'));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'Z', mmgr_text_upper((uint8_t)'z'));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'Q', mmgr_text_upper((uint8_t)'Q'));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)'`', mmgr_text_upper((uint8_t)'`'), "the byte before a stays");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'{', mmgr_text_upper((uint8_t)'{'));
    TEST_ASSERT_EQUAL_UINT8(0xE3u, mmgr_text_upper(0xE3u));
}
