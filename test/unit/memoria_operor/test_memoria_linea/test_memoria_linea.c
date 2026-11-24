#include "memoria_operor/memoria_linea.h"

#include "unity.h"

#include <stdint.h>

enum
{
    ROOM = 96
};

static uint8_t s_bytes[ROOM];

// each byte its own value, so a copy that lands one place off shows
static void bytes_patterned(void)
{
    for (size_t at = 0u; at < ROOM; at++)
    {
        s_bytes[at] = (uint8_t)(at + 1u);
    }
}

void setUp(void)
{
    bytes_patterned();
}

void tearDown(void)
{
}

void test_copy_writes_the_source_into_a_region_apart(void)
{
    mmgr_bytes_copy(&s_bytes[60], &s_bytes[3], 21u);
    for (size_t at = 0u; at < 21u; at++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(at + 4u), s_bytes[60u + at]);
    }
    TEST_ASSERT_EQUAL_UINT8(60u, s_bytes[59]);
    TEST_ASSERT_EQUAL_UINT8(82u, s_bytes[81]);
}

void test_move_is_a_copy_where_the_destination_lies_below_the_source(void)
{
    mmgr_bytes_move(&s_bytes[2], &s_bytes[7], 37u);
    for (size_t at = 0u; at < 37u; at++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(at + 8u), s_bytes[2u + at]);
    }
    TEST_ASSERT_EQUAL_UINT8(2u, s_bytes[1]);
}

void test_move_is_a_copy_where_the_destination_lies_above_the_source(void)
{
    mmgr_bytes_move(&s_bytes[9], &s_bytes[4], 37u);
    for (size_t at = 0u; at < 37u; at++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(at + 5u), s_bytes[9u + at]);
    }
    TEST_ASSERT_EQUAL_UINT8(47u, s_bytes[46]);
}

void test_move_onto_itself_changes_nothing(void)
{
    mmgr_bytes_move(&s_bytes[5], &s_bytes[5], 30u);
    for (size_t at = 0u; at < ROOM; at++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(at + 1u), s_bytes[at]);
    }
}

void test_set_writes_the_value_and_no_byte_past_the_region(void)
{
    mmgr_bytes_set(&s_bytes[10], 0xA5u, 19u);
    for (size_t at = 0u; at < 19u; at++)
    {
        TEST_ASSERT_EQUAL_UINT8(0xA5u, s_bytes[10u + at]);
    }
    TEST_ASSERT_EQUAL_UINT8(10u, s_bytes[9]);
    TEST_ASSERT_EQUAL_UINT8(30u, s_bytes[29]);
}

void test_compare_answers_zero_exactly_where_every_byte_agrees(void)
{
    static uint8_t left[40];
    static uint8_t right[40];
    for (size_t at = 0u; at < 40u; at++)
    {
        left[at] = (uint8_t)at;
        right[at] = (uint8_t)at;
    }
    TEST_ASSERT_EQUAL_INT(0, mmgr_bytes_compare(left, right, 33u));
    right[21] = 200u;
    TEST_ASSERT_TRUE(mmgr_bytes_compare(left, right, 33u) < 0);
    TEST_ASSERT_TRUE(mmgr_bytes_compare(right, left, 33u) > 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mmgr_bytes_compare(left, right, 21u), "a difference past the count reads as none");
}

void test_find_answers_the_first_byte_that_holds_the_value(void)
{
    s_bytes[30] = 0x77u;
    s_bytes[50] = 0x77u;
    TEST_ASSERT_EQUAL_PTR(&s_bytes[30], mmgr_bytes_find(&s_bytes[0], 0x77u, 64u));
    TEST_ASSERT_EQUAL_PTR(&s_bytes[50], mmgr_bytes_find(&s_bytes[31], 0x77u, 33u));
    TEST_ASSERT_NULL(mmgr_bytes_find(&s_bytes[31], 0x77u, 19u));
}

// the order of two unsigned words, as a compare of qsort answers it
static int words_order(const void *left, const void *right)
{
    const uint32_t a = *(const uint32_t *)left;
    const uint32_t b = *(const uint32_t *)right;
    return (a > b) - (a < b);
}

// the order of two pairs by their first word alone, and so pairs of equal first words show whether a sort is stable
static int pairs_order(const void *left, const void *right)
{
    return words_order(left, right);
}

void test_sort_puts_words_in_order(void)
{
    uint32_t words[9] = {5u, 3u, 9u, 1u, 5u, 0u, 8u, 2u, 7u};
    uint32_t scratch[9];
    const uint32_t sorted[9] = {0u, 1u, 2u, 3u, 5u, 5u, 7u, 8u, 9u};
    TEST_ASSERT_TRUE(mmgr_bytes_sort(words, 9u, sizeof(words[0]), words_order, scratch));
    TEST_ASSERT_EQUAL_UINT32_ARRAY(sorted, words, 9u);
}

void test_sort_keeps_equal_elements_in_their_order(void)
{
    uint32_t pairs[6][2] = {{2u, 0u}, {1u, 1u}, {2u, 2u}, {1u, 3u}, {0u, 4u}, {2u, 5u}};
    uint32_t scratch[6][2];
    const uint32_t sorted[12] = {0u, 4u, 1u, 1u, 1u, 3u, 2u, 0u, 2u, 2u, 2u, 5u};
    TEST_ASSERT_TRUE(mmgr_bytes_sort(pairs, 6u, sizeof(pairs[0]), pairs_order, scratch));
    TEST_ASSERT_EQUAL_UINT32_ARRAY(sorted, &pairs[0][0], 12u);
}

void test_sort_of_many_words_leaves_them_in_order_with_each_one_kept(void)
{
    static uint32_t words[1000];
    static uint32_t scratch[1000];
    uint32_t state = 12345u;
    uint64_t sum = 0u;
    for (size_t at = 0u; at < 1000u; at++)
    {
        state = (state * 1103515245u) + 12345u;
        words[at] = state % 257u;
        sum += words[at];
    }
    TEST_ASSERT_TRUE(mmgr_bytes_sort(words, 1000u, sizeof(words[0]), words_order, scratch));
    uint64_t after = 0u;
    for (size_t at = 0u; at < 1000u; at++)
    {
        after += words[at];
        if (at > 0u)
        {
            TEST_ASSERT_TRUE(words[at - 1u] <= words[at]);
        }
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(sum, after, "the sort keeps every word");
}

void test_sort_of_fewer_than_two_elements_and_its_refusals(void)
{
    uint32_t words[3] = {3u, 2u, 1u};
    uint32_t scratch[3];
    TEST_ASSERT_TRUE(mmgr_bytes_sort(words, 0u, sizeof(words[0]), words_order, NULL));
    TEST_ASSERT_TRUE(mmgr_bytes_sort(words, 1u, sizeof(words[0]), words_order, NULL));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_bytes_sort(words, 3u, sizeof(words[0]), words_order, NULL), "no room is refused");
    TEST_ASSERT_FALSE(mmgr_bytes_sort(words, 3u, 0u, words_order, scratch));
    TEST_ASSERT_FALSE(mmgr_bytes_sort(words, 3u, sizeof(words[0]), NULL, scratch));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_bytes_sort(NULL, 3u, sizeof(words[0]), words_order, scratch), "no base is refused");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, words[0], "a refusal leaves the elements as they were");
    TEST_ASSERT_TRUE(mmgr_bytes_sort(words, 2u, sizeof(words[0]), words_order, scratch));
    TEST_ASSERT_EQUAL_UINT32(2u, words[0]);
    TEST_ASSERT_EQUAL_UINT32(3u, words[1]);
}
