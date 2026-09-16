#include "unity.h"

#include "memoria_anularis/memoria_anularis.h"

#define CAP 64u
#define NSEGS 4u

static mmgr_ring g_ring;
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_buf[CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_src[CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_dst[CAP * 4u];

static void fresh(void)
{
    for (size_t i = 0; i < sizeof g_src; i++)
    {
        g_src[i] = (uint8_t)(i + 1u);
    }
    for (size_t i = 0; i < sizeof g_buf; i++)
    {
        g_buf[i] = 0u;
    }
    TEST_ASSERT_TRUE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP, .segment_count = NSEGS));
}

void test_init_refuses_bad_sizes(void)
{
    TEST_ASSERT_FALSE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = 0u, .segment_count = NSEGS));
    TEST_ASSERT_FALSE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = 48u, .segment_count = NSEGS));
    TEST_ASSERT_FALSE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP, .segment_count = 0u));
    TEST_ASSERT_FALSE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP, .segment_count = 3u));
    TEST_ASSERT_FALSE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                                 .segment_count = CAP * 2u));
    TEST_ASSERT_TRUE(
        EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP, .segment_count = NSEGS));
}

void test_init_starts_empty_with_every_loculus_free(void)
{
    fresh();

    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));
    TEST_ASSERT_EQUAL_size_t(CAP - 1u, EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &g_ring));
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &g_ring));

    const embed_word ready = EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &g_ring);

    for (size_t i = 0; i < (size_t)MMGR_RING_LOCULI; i++)
    {
        TEST_ASSERT_NOT_EQUAL(0u, (unsigned)(ready & (embed_word)((embed_word)1 << i)));
    }
}

void test_put_then_read_round_trips(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));
    TEST_ASSERT_EQUAL_size_t(16u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));

    const size_t got = EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 16u);

    TEST_ASSERT_EQUAL_size_t(16u, got);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 16u);
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));
}

void test_put_refuses_more_than_vacant(void)
{
    fresh();

    TEST_ASSERT_FALSE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = CAP));
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));
    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = CAP - 1u));
}

void test_read_takes_only_what_arrived(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 4u));

    const size_t got = EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 32u);

    TEST_ASSERT_EQUAL_size_t(4u, got);
}

void test_move_wraps_in_order(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 56u));
    TEST_ASSERT_EQUAL_size_t(56u, EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 56u));

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 32u));
    TEST_ASSERT_EQUAL_size_t(32u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));

    const size_t got = EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 32u);

    TEST_ASSERT_EQUAL_size_t(32u, got);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 32u);
}

void test_move_carries_every_length_at_every_offset(void)
{
    for (size_t start = 0u; start < CAP; start++)
    {
        for (size_t n = 1u; n < CAP; n++)
        {
            fresh();

            if (start != 0u)
            {
                TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = start));
                EMBED_CALL(anularis.consume, AnularisCfg, .ring = &g_ring, .bytes = start);
            }
            if (n > EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &g_ring))
            {
                continue;
            }

            TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = n));

            g_dst[n] = 0xA5u;
            TEST_ASSERT_EQUAL_size_t(n,
                                     EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = n));
            TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, n);
            TEST_ASSERT_EQUAL_HEX8(0xA5u, g_dst[n]);
        }
    }
}

void test_peek_leaves_the_tail_and_consume_moves_it(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));

    EMBED_CALL(anularis.peek, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 8u, .offset = 0u);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 8u);
    TEST_ASSERT_EQUAL_size_t(16u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));

    EMBED_CALL(anularis.consume, AnularisCfg, .ring = &g_ring, .bytes = 8u);
    TEST_ASSERT_EQUAL_size_t(8u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring));

    EMBED_CALL(anularis.peek, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = 8u, .offset = 0u);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&g_src[8], g_dst, 8u);
}

void test_peek_holds_a_request_above_capacity(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));

    for (size_t i = 0; i < sizeof g_dst; i++)
    {
        g_dst[i] = 0xA5u;
    }

    EMBED_CALL(anularis.peek, AnularisCfg, .ring = &g_ring, .dst = g_dst, .bytes = CAP * 3u, .offset = 0u);

    for (size_t i = CAP; i < sizeof g_dst; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xA5u, g_dst[i]);
    }
}

void test_read_byte_empties_the_ring(void)
{
    fresh();

    TEST_ASSERT_TRUE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_src, .bytes = 3u));

    uint8_t b = 0u;

    for (size_t i = 0; i < 3u; i++)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(anularis.read_byte, AnularisCfg, .ring = &g_ring, .dst = &b));
        TEST_ASSERT_EQUAL_HEX8(g_src[i], b);
    }
    TEST_ASSERT_FALSE(EMBED_CALL(anularis.read_byte, AnularisCfg, .ring = &g_ring, .dst = &b));
}

void test_loculus_next_picks_the_lowest_and_reports_empty(void)
{
    fresh();

    TEST_ASSERT_EQUAL_INT(-1,
                          (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .ring = &g_ring, .mask = (embed_word)0));
    TEST_ASSERT_EQUAL_INT(0,
                          (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .ring = &g_ring, .mask = (embed_word)1));
    TEST_ASSERT_EQUAL_INT(
        3, (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .ring = &g_ring, .mask = (embed_word)0x18));
}

void test_hold_takes_a_loculus_and_drop_returns_it(void)
{
    fresh();

    const embed_word before = EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &g_ring);
    const embed_iword pick = EMBED_CALL(anularis.loculus_next, AnularisCfg, .ring = &g_ring, .mask = before);

    TEST_ASSERT_EQUAL_INT(0, (int)pick);

    TEST_ASSERT_TRUE(
        EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &g_ring, .index = 0u, .src = g_src, .bytes = 12u));
    TEST_ASSERT_FALSE(
        EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &g_ring, .index = 0u, .src = g_src, .bytes = 12u));

    const embed_word held = EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &g_ring);

    TEST_ASSERT_EQUAL_UINT(0u, (unsigned)(held & (embed_word)1));

    const mmgr_ring_span *const k = EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &g_ring, .index = 0u);

    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_EQUAL_PTR(g_src, k->buf);
    TEST_ASSERT_EQUAL_size_t(12u, k->bytes);
    TEST_ASSERT_EQUAL_size_t(0u, k->read_offset);

    EMBED_CALL(anularis.loculus_drop, AnularisCfg, .ring = &g_ring, .index = 0u);
    TEST_ASSERT_EQUAL_UINT((unsigned)before,
                           (unsigned)EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &g_ring));
}

void test_hold_refuses_a_loculus_that_does_not_exist(void)
{
    fresh();

    TEST_ASSERT_FALSE(EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &g_ring, .index = MMGR_RING_LOCULI,
                                 .src = g_src, .bytes = 4u));
    TEST_ASSERT_NULL(EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &g_ring, .index = MMGR_RING_LOCULI));
}

void test_segments_publish_and_release_in_order(void)
{
    fresh();

    size_t idx = 99u;

    TEST_ASSERT_FALSE(EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &g_ring, .out_index = &idx));

    for (size_t i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &g_ring, .out_index = &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
        EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &g_ring);
    }

    TEST_ASSERT_FALSE(EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &g_ring, .out_index = &idx));
    TEST_ASSERT_EQUAL_size_t(NSEGS, EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &g_ring));

    for (size_t i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &g_ring, .out_index = &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
        EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &g_ring);
    }
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &g_ring));
}

void test_seg_at_walks_the_buffer(void)
{
    fresh();

    for (size_t i = 0; i < NSEGS; i++)
    {
        const uint8_t *const at = EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &g_ring, .index = i);

        TEST_ASSERT_EQUAL_PTR(&g_buf[i * (CAP / NSEGS)], at);
    }
}
