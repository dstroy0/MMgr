#include "memoria_operor/memoria_operor.h"

#include "unity.h"

#include <stdio.h>
#include <string.h>

enum
{
    PAD = 8,
    BODY = 40,
    ROOM = PAD + BODY + PAD
};

static unsigned char got[ROOM];
static unsigned char want[ROOM];

static void pattern(unsigned char *p, size_t n, unsigned seed)
{
    for (size_t i = 0; i < n; i++)
    {
        p[i] = (unsigned char)((i * 31u) + (seed * 7u) + 1u);
    }
}

void setUp(void)
{
    pattern(got, sizeof got, 0u);
    memcpy(want, got, sizeof want);
}

void tearDown(void)
{
}

static void same(const char *what)
{
    for (size_t i = 0; i < ROOM; i++)
    {
        if (got[i] != want[i])
        {
            char msg[96];
            (void)snprintf(msg, sizeof msg, "%s: byte %u", what, (unsigned)i);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(want[i], got[i], msg);
        }
    }
}

void test_memor_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("memoria_operor.h compiled with no header before it");
}

void test_memor_namespace_is_wired(void)
{
    const MemoriaOperorNs *ns = &memor;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(MemoriaOperorNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}

void test_cpy_matches_memcpy_at_every_length(void)
{
    static unsigned char src[BODY];
    pattern(src, sizeof src, 3u);

    for (size_t n = 0; n <= BODY; n++)
    {
        setUp();
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = got + PAD, .src = src, .bytes = n);
        memcpy(want + PAD, src, n);
        same("cpy");
    }
}

void test_cpy_matches_memcpy_at_every_offset(void)
{
    static unsigned char src[BODY];
    pattern(src, sizeof src, 5u);

    for (size_t off = 0; off < 9u; off++)
    {
        for (size_t soff = 0; soff < 9u; soff++)
        {
            setUp();
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = got + PAD + off, .src = src + soff, .bytes = (size_t)17);
            memcpy(want + PAD + off, src + soff, 17u);
            same("cpy at an offset");
        }
    }
}

void test_cpy_of_nothing_touches_nothing(void)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = got + PAD, .src = "abc", .bytes = (size_t)0);
    same("cpy of zero");
}

void test_move_matches_memmove_when_the_regions_do_not_touch(void)
{
    EMBED_CALL(memor.move_down, MemoriaCfg, .dst = got + PAD, .src = got + PAD + BODY, .bytes = (size_t)8);
    memmove(want + PAD, want + PAD + BODY, 8u);
    same("move, disjoint");
}

void test_move_matches_memmove_overlapping_forwards(void)
{
    for (size_t n = 1; n <= 33u; n++)
    {
        for (size_t gap = 1; gap < 9u; gap++)
        {
            setUp();
            EMBED_CALL(memor.move_up, MemoriaCfg, .dst = got + PAD + gap, .src = got + PAD, .bytes = n);
            memmove(want + PAD + gap, want + PAD, n);
            same("move, destination above source");
        }
    }
}

void test_move_matches_memmove_overlapping_backwards(void)
{
    for (size_t n = 1; n <= 33u; n++)
    {
        for (size_t gap = 1; gap < 9u; gap++)
        {
            setUp();
            EMBED_CALL(memor.move_down, MemoriaCfg, .dst = got + PAD, .src = got + PAD + gap, .bytes = n);
            memmove(want + PAD, want + PAD + gap, n);
            same("move, destination below source");
        }
    }
}

void test_move_onto_itself_changes_nothing(void)
{
    EMBED_CALL(memor.move_down, MemoriaCfg, .dst = got + PAD, .src = got + PAD, .bytes = (size_t)16);
    same("move onto itself");
}

void test_move_of_nothing_touches_nothing(void)
{
    EMBED_CALL(memor.move_down, MemoriaCfg, .dst = got + PAD, .src = got + PAD + 1, .bytes = (size_t)0);
    same("move of zero");
}

void test_move_of_regions_that_end_exactly_where_the_other_starts(void)
{
    EMBED_CALL(memor.move_down, MemoriaCfg, .dst = got + PAD, .src = got + PAD + 16u, .bytes = (size_t)16);
    memmove(want + PAD, want + PAD + 16u, 16u);
    same("move, adjacent");
}

void test_cmp_orders_a_whole_word_where_the_two_addresses_are_not_level(void)
{
    static EMBED_ALIGN(16) uint8_t left[40];
    static EMBED_ALIGN(16) uint8_t right[40];

    memset(left, 'a', sizeof left);
    memset(right, 'a', sizeof right);
    left[4] = 'b';
    TEST_ASSERT_GREATER_THAN_INT(0,
                                 EMBED_CALL(memor.cmp, MemoriaCfg, .src = left + 1, .other = right + 2, .bytes = 24u));
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = right + 2, .other = left + 1, .bytes = 24u));

    // Past a whole vector, the difference is the word walk's to find and not the vector walk's
    left[4] = 'a';
    left[19] = 'b';
    TEST_ASSERT_GREATER_THAN_INT(0,
                                 EMBED_CALL(memor.cmp, MemoriaCfg, .src = left + 1, .other = right + 2, .bytes = 24u));
}

void test_chr_finds_a_byte_ahead_of_the_first_word_boundary(void)
{
    static EMBED_ALIGN(16) uint8_t bytes[40];

    memset(bytes, 'a', sizeof bytes);
    bytes[1] = 'z';
    TEST_ASSERT_EQUAL_PTR(bytes + 1,
                          EMBED_CALL(memor.chr, MemoriaCfg, .src = bytes + 1, .val = (uint8_t)'z', .bytes = 24u));
}

void test_cmp_agrees_with_memcmp_on_the_sign(void)
{
    static const unsigned char a[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    static const unsigned char lo[8] = {1u, 2u, 3u, 0u, 5u, 6u, 7u, 8u};
    static const unsigned char hi[8] = {1u, 2u, 3u, 9u, 5u, 6u, 7u, 8u};

    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = a, .bytes = sizeof a));
    TEST_ASSERT_GREATER_THAN_INT(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = lo, .bytes = sizeof a));
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = hi, .bytes = sizeof a));

    TEST_ASSERT_EQUAL_INT(memcmp(a, lo, sizeof a) > 0,
                          EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = lo, .bytes = sizeof a) > 0);
    TEST_ASSERT_EQUAL_INT(memcmp(a, hi, sizeof a) < 0,
                          EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = hi, .bytes = sizeof a) < 0);
}

void test_cmp_finds_a_difference_at_every_position(void)
{
    static unsigned char a[BODY];
    static unsigned char b[BODY];

    for (size_t at = 0; at < BODY; at++)
    {
        pattern(a, sizeof a, 1u);
        pattern(b, sizeof b, 1u);
        b[at] = (unsigned char)(b[at] ^ 0x80u);

        const int mine = EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = b, .bytes = sizeof a);
        const int ref = memcmp(a, b, sizeof a);
        TEST_ASSERT_TRUE_MESSAGE((mine < 0) == (ref < 0) && (mine > 0) == (ref > 0), "the sign disagrees with memcmp");
    }
}

void test_cmp_of_nothing_is_equal(void)
{
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = "a", .other = "b", .bytes = (size_t)0));
}

void test_cmp_reads_no_further_than_it_was_told(void)
{
    static const unsigned char a[8] = {1u, 1u, 1u, 1u, 9u, 9u, 9u, 9u};
    static const unsigned char b[8] = {1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u};

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, EMBED_CALL(memor.cmp, MemoriaCfg, .src = a, .other = b, .bytes = (size_t)4),
                                  "the difference is past the count");
}

void test_chr_matches_memchr_at_every_position(void)
{
    static unsigned char hay[BODY];

    for (size_t at = 0; at < BODY; at++)
    {
        pattern(hay, sizeof hay, 2u);
        hay[at] = 0xC7u;
        for (size_t i = 0; i < at; i++)
        {
            if (hay[i] == 0xC7u)
            {
                hay[i] = 0x00u;
            }
        }

        TEST_ASSERT_EQUAL_PTR(memchr(hay, 0xC7, sizeof hay),
                              EMBED_CALL(memor.chr, MemoriaCfg, .src = hay, .bytes = sizeof hay, .val = (uint8_t)0xC7));
    }
}

void test_chr_of_a_byte_that_is_not_there(void)
{
    static unsigned char hay[BODY];
    pattern(hay, sizeof hay, 4u);
    for (size_t i = 0; i < sizeof hay; i++)
    {
        if (hay[i] == 0xFEu)
        {
            hay[i] = 0x00u;
        }
    }

    TEST_ASSERT_NULL(EMBED_CALL(memor.chr, MemoriaCfg, .src = hay, .bytes = sizeof hay, .val = (uint8_t)0xFE));
    TEST_ASSERT_EQUAL_PTR(memchr(hay, 0xFE, sizeof hay),
                          EMBED_CALL(memor.chr, MemoriaCfg, .src = hay, .bytes = sizeof hay, .val = (uint8_t)0xFE));
}

void test_chr_of_nothing_finds_nothing(void)
{
    TEST_ASSERT_NULL(EMBED_CALL(memor.chr, MemoriaCfg, .src = "a", .bytes = (size_t)0, .val = (uint8_t)'a'));
}

void test_chr_finds_a_zero_byte(void)
{
    static const unsigned char hay[4] = {1u, 2u, 0u, 3u};
    TEST_ASSERT_EQUAL_PTR(hay + 2,
                          EMBED_CALL(memor.chr, MemoriaCfg, .src = hay, .bytes = sizeof hay, .val = (uint8_t)0));
}

void test_set_matches_memset_at_every_length(void)
{
    for (size_t n = 0; n <= BODY; n++)
    {
        setUp();
        EMBED_CALL(memor.set, MemoriaCfg, .dst = got + PAD, .val = (uint8_t)0xA5, .bytes = n);
        memset(want + PAD, 0xA5, n);
        same("set");
    }
}

void test_set_matches_memset_at_every_offset(void)
{
    for (size_t off = 0; off < 9u; off++)
    {
        setUp();
        EMBED_CALL(memor.set, MemoriaCfg, .dst = got + PAD + off, .val = (uint8_t)0x5A, .bytes = (size_t)17);
        memset(want + PAD + off, 0x5A, 17u);
        same("set at an offset");
    }
}

void test_set_retains_only_the_low_byte_of_its_value(void)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = got + PAD, .val = (uint8_t)(0x1234u & 0xFFu), .bytes = (size_t)8);
    memset(want + PAD, 0x34, 8u);
    same("set of a wide value");
}

void test_zero_is_set_of_zero(void)
{
    for (size_t n = 0; n <= 33u; n++)
    {
        setUp();
        EMBED_CALL(memor.set, MemoriaCfg, .dst = got + PAD, .val = (uint8_t)0, .bytes = n);
        memset(want + PAD, 0, n);
        same("zero");
    }
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

// count elements of size bytes at base, in the order of order, through memor.sort with scratch as its room
static embed_bool sorted_by(void *base, size_t count, size_t size, int (*order)(const void *, const void *),
                            void *scratch)
{
    return EMBED_CALL(memor.sort, MemoriaCfg, .dst = base, .bytes = size, .scratch = scratch, .count = count,
                      .order = order);
}

void test_sort_puts_words_in_order(void)
{
    uint32_t words[9] = {5u, 3u, 9u, 1u, 5u, 0u, 8u, 2u, 7u};
    uint32_t scratch[9];
    const uint32_t sorted[9] = {0u, 1u, 2u, 3u, 5u, 5u, 7u, 8u, 9u};
    TEST_ASSERT_TRUE(sorted_by(words, 9u, sizeof(words[0]), words_order, scratch));
    TEST_ASSERT_EQUAL_UINT32_ARRAY(sorted, words, 9u);
}

void test_sort_keeps_equal_elements_in_their_order(void)
{
    uint32_t pairs[6][2] = {{2u, 0u}, {1u, 1u}, {2u, 2u}, {1u, 3u}, {0u, 4u}, {2u, 5u}};
    uint32_t scratch[6][2];
    const uint32_t sorted[12] = {0u, 4u, 1u, 1u, 1u, 3u, 2u, 0u, 2u, 2u, 2u, 5u};
    TEST_ASSERT_TRUE(sorted_by(pairs, 6u, sizeof(pairs[0]), pairs_order, scratch));
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
    TEST_ASSERT_TRUE(sorted_by(words, 1000u, sizeof(words[0]), words_order, scratch));
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
    TEST_ASSERT_TRUE(sorted_by(words, 0u, sizeof(words[0]), words_order, NULL));
    TEST_ASSERT_TRUE(sorted_by(words, 1u, sizeof(words[0]), words_order, NULL));
    TEST_ASSERT_FALSE_MESSAGE(sorted_by(words, 3u, sizeof(words[0]), words_order, NULL), "no room is refused");
    TEST_ASSERT_FALSE(sorted_by(words, 3u, 0u, words_order, scratch));
    TEST_ASSERT_FALSE(sorted_by(words, 3u, sizeof(words[0]), NULL, scratch));
    TEST_ASSERT_FALSE_MESSAGE(sorted_by(NULL, 3u, sizeof(words[0]), words_order, scratch), "no base is refused");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, words[0], "a refusal leaves the elements as they were");
    TEST_ASSERT_TRUE(sorted_by(words, 2u, sizeof(words[0]), words_order, scratch));
    TEST_ASSERT_EQUAL_UINT32(2u, words[0]);
    TEST_ASSERT_EQUAL_UINT32(3u, words[1]);
}
