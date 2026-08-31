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

void test_set_keeps_only_the_low_byte_of_its_value(void)
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
